#ifndef AIAGENTSERVICE_H
#define AIAGENTSERVICE_H

#include "HistoryStore.h"
#include "ToolProviderInterface.h"

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace agent4cpp {
class Agent;
class InMemoryKnowledgeStore;
class ToolRegistry;
}

// 一次问答的结果。
struct AiResult {
    QString answer;
    QString error;
    QStringList trace;

    // 判据只看 error：模型可能只调了工具没给文字，answer 为空不算这一轮失败
    bool ok() const { return error.isEmpty(); }
};

// Ask() 立刻返回，循环跑在后台线程；工具要碰硬件时由使用方切回主线程

class AiAgentService : public QObject {
    Q_OBJECT

public:
    explicit AiAgentService(QObject* parent = nullptr);
    ~AiAgentService() override;

    // 不持有不释放，调用方保证生命周期长于本对象
    void SetToolProviders(const QList<IToolProvider*>& providers);

    bool IsReady() const;

    bool IsBusy() const { return m_busy; }

    QString ConfigHint() const;

    QStringList ExamplePrompts() const;

    void ReloadConfig();

    void StartNewSession();

    QString CurrentSessionId() const { return m_session.id; }

    QList<AiSession> Sessions() const;

    bool DeleteSession(const QString& id);

    // 导入会重建 Agent，并重置当前对话上下文
    int ImportKnowledge(const QStringList& files);

    int KnowledgeFileCount() const { return m_knowledgeFiles.size(); }

    int KnowledgeChunkCount() const { return m_knowledgeChunkCount; }

    QStringList KnowledgeFiles() const { return m_knowledgeFiles; }

    void ClearKnowledge();

public slots:

    void Ask(const QString& question);

signals:

    // 成功和失败都会发：界面只挂这一个信号就能收到本轮收尾，不必再区分错误通道
    void SigFinished(AiResult result);

    void SigBusyChanged(bool busy);

    void SigSessionsChanged();

    void SigKnowledgeChanged();

private:
    void BuildAgent();

    void EnsureKnowledgeStore();

    QList<IToolProvider*> m_providers;

    std::unique_ptr<agent4cpp::ToolRegistry> m_registry;
    std::shared_ptr<agent4cpp::Agent> m_agent;

    std::unique_ptr<agent4cpp::InMemoryKnowledgeStore> m_knowledge;
    QStringList m_knowledgeFiles;
    int m_knowledgeChunkCount { 0 };

    AiSession m_session;

    // 后台线程碰不到 m_session，问题先存在这里，等 finished 回到主线程再连同答复一起写入
    QString m_pendingQuestion;

    // 父对象就是本服务，finished 会在创建它的（主）线程投递，回调里可以安全碰会话与界面
    QFutureWatcher<AiResult>* m_watcher;

    bool m_busy { false };
};

#endif
