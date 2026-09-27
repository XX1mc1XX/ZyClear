#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QList>
#include <QString>
#include <QStringList>

struct AiTurn {
    QString question;
    QString answer;
    QString error;
    QStringList trace;
};

struct AiSession {
    QString id;
    QString title;
    QString createdAt;
    QList<AiTurn> turns;

    QString subtitle() const;
};

// 旧会话只能查看/复制/删除，不能接着聊：agent4cpp 的 Agent 只有追加，没有注入历史消息的入口

class HistoryStore {
public:
    static QString StorageDir();

    static QList<AiSession> List();

    static AiSession Load(const QString& id);

    static bool Save(const AiSession& session);

    static bool Remove(const QString& id);

    static AiSession CreateNew();
};

#endif
