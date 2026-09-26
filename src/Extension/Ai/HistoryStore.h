#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QList>
#include <QString>
#include <QStringList>

// 一问一答，连同这轮调用了哪些工具
struct AiTurn {
    QString question;
    QString answer;
    QString error;
    QStringList trace;
};

// 一次会话 = 若干轮问答
struct AiSession {
    QString id;
    QString title;
    QString createdAt;
    QList<AiTurn> turns;

    // 界面上显示的副标题，例如 "3 轮 · 09-27 14:02"
    QString subtitle() const;
};

// =============================================================================
// 会话历史存储
//
// 【存在哪】可执行文件目录下的 ai_sessions/，一次会话一个 json 文件。
//   用文件而不是注册表/QSettings：聊天记录会长，塞进注册表既难看又难备份。
//   而且一个文件一次会话，删起来就是删文件，不会牵一发动全身。
//
// 【为什么不支持「打开旧会话继续聊」】
//   agent4cpp 的 Agent 只提供「追加」，没有「注入一批历史消息」的入口，
//   所以没法把一个旧会话原样恢复成活动上下文。这里的做法是：
//   历史会话可以查看、复制、删除；想接着聊就新建一次会话，
//   需要旧结论时把它作为参考资料导入知识库（导入面板里有这条）。
// =============================================================================

class HistoryStore {
public:
    static QString StorageDir();

    // 按时间倒序列出全部会话（只读元信息，turns 也是完整的，文件不大）
    static QList<AiSession> List();

    static AiSession Load(const QString& id);

    static bool Save(const AiSession& session);

    static bool Remove(const QString& id);

    // 造一个新的空会话（id 用时间戳，保证唯一且天然按时间排序）
    static AiSession CreateNew();
};

#endif
