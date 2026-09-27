#include "HistoryStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {
const char* kFileSuffix = ".json";
}

QString AiSession::subtitle() const
{
    // createdAt 形如 "2026-09-27 14:02:11"，这里取「月-日 时:分」
    const QString stamp = createdAt.size() >= 16
        ? createdAt.mid(5, 11)
        : createdAt;

    return QStringLiteral("%1 轮 · %2").arg(turns.size()).arg(stamp);
}

// 放在可执行文件旁而非 AppData：便携版拷走就跟着走，卸载时删目录即清干净
QString HistoryStore::StorageDir()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/ai_sessions");
}

AiSession HistoryStore::CreateNew()
{
    AiSession session;
    session.id = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")); // 带毫秒，同一秒连开两次会话也不会撞 id
    session.createdAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return session;
}

bool HistoryStore::Save(const AiSession& session)
{
    if (session.id.isEmpty()) {
        return false;
    }

    QDir dir(StorageDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QJsonArray turns;
    for (const AiTurn& turn : session.turns) {
        QJsonObject item;
        item.insert(QStringLiteral("question"), turn.question);
        item.insert(QStringLiteral("answer"), turn.answer);
        item.insert(QStringLiteral("error"), turn.error);

        QJsonArray trace;
        for (const QString& step : turn.trace) {
            trace.append(step);
        }
        item.insert(QStringLiteral("trace"), trace);

        turns.append(item);
    }

    QJsonObject root;
    root.insert(QStringLiteral("id"), session.id);
    root.insert(QStringLiteral("title"), session.title);
    root.insert(QStringLiteral("created_at"), session.createdAt);
    root.insert(QStringLiteral("turns"), turns);

    QFile file(dir.filePath(session.id + QString::fromLatin1(kFileSuffix)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    // 缩进格式便于直接打开文件排查问题，多出来的体积可忽略
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

AiSession HistoryStore::Load(const QString& id)
{
    AiSession session;
    if (id.isEmpty()) {
        return session;
    }

    QFile file(QDir(StorageDir()).filePath(id + QString::fromLatin1(kFileSuffix)));
    if (!file.open(QIODevice::ReadOnly)) {
        return session;
    }

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    session.id = root.value(QStringLiteral("id")).toString();
    session.title = root.value(QStringLiteral("title")).toString();
    session.createdAt = root.value(QStringLiteral("created_at")).toString();

    const QJsonArray turns = root.value(QStringLiteral("turns")).toArray();
    for (const QJsonValue& value : turns) {
        const QJsonObject item = value.toObject();
        AiTurn turn;
        turn.question = item.value(QStringLiteral("question")).toString();
        turn.answer = item.value(QStringLiteral("answer")).toString();
        turn.error = item.value(QStringLiteral("error")).toString();

        const QJsonArray trace = item.value(QStringLiteral("trace")).toArray();
        for (const QJsonValue& step : trace) {
            turn.trace << step.toString();
        }

        session.turns.append(turn);
    }

    return session;
}

QList<AiSession> HistoryStore::List()
{
    QList<AiSession> sessions;

    QDir dir(StorageDir());
    if (!dir.exists()) {
        return sessions;
    }

    const QFileInfoList files = dir.entryInfoList(
        QStringList { QStringLiteral("*") + QString::fromLatin1(kFileSuffix) },
        QDir::Files, QDir::Time); // QDir::Time 是修改时间倒序，最近聊过的排在前面

    for (const QFileInfo& info : files) {
        AiSession session = Load(info.completeBaseName());
        // 半截或损坏的文件读出来 id 为空，直接跳过，不让坏文件挡住整份列表
        if (!session.id.isEmpty()) {
            sessions.append(session);
        }
    }

    return sessions;
}

bool HistoryStore::Remove(const QString& id)
{
    if (id.isEmpty()) {
        return false;
    }

    QFile file(QDir(StorageDir()).filePath(id + QString::fromLatin1(kFileSuffix)));
    if (!file.exists()) {
        return false;
    }

    return file.remove();
}
