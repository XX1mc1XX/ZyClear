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

// 读写只发生在主线程（图形界面和无头循环各自是一次一问一答），没有并发路径，也就没有加锁
class HistoryStore {
public:
    static QString StorageDir();

    static QList<AiSession> List();

    static AiSession Load(const QString& id);

    // 整份覆盖写：文件始终是一个完整会话的快照，不会留下追加到一半的半截记录
    static bool Save(const AiSession& session);

    static bool Remove(const QString& id);

    static AiSession CreateNew();
};

#endif
