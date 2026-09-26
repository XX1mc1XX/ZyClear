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
    QString answer; // 模型最终答复（失败时为空）
    QString error; // 失败原因（成功时为空）
    QStringList trace; // 过程中调用过的工具，按顺序排列

    bool ok() const { return error.isEmpty(); }
};

// =============================================================================
// AI 助手服务
//
// 【定位】通用层：这里不认识任何具体客户端。
//   宿主能力通过 IToolProvider 注入进来，本类只负责：
//     组装工具 → 跑多步循环 → 管会话历史 → 管知识库
//
// 【怎么搬到别的客户端】
//   新客户端只要写一个 IToolProvider 实现（把已有函数包成工具，几十行），
//   然后照 mainwindow 那几行装配即可。这个文件一行都不用改。
//
// 【线程模型】Ask() 立刻返回，循环跑在后台线程。
//   耗时的部分是「等模型回复」（几秒到几十秒）；
//   工具要碰硬件时由使用方自己切回主线程（见 Integration/CameraToolProvider）。
// =============================================================================

class AiAgentService : public QObject {
    Q_OBJECT

public:
    explicit AiAgentService(QObject* parent = nullptr);
    ~AiAgentService() override;

    // 注入宿主能力。不持有、不释放，调用方保证生命周期长于本对象
    void SetToolProviders(const QList<IToolProvider*>& providers);

    // 配置齐全（基地址 + 模型名 + Key）且至少有一个工具提供者，才为 true
    bool IsReady() const;

    bool IsBusy() const { return m_busy; }

    QString ConfigHint() const;

    // 界面欢迎语用的示例问法，取自各工具提供者
    QStringList ExamplePrompts() const;

    void ReloadConfig();

    // ---- 会话历史 ----

    void StartNewSession();

    QString CurrentSessionId() const { return m_session.id; }

    QList<AiSession> Sessions() const;

    bool DeleteSession(const QString& id);

    // ---- 知识库 ----

    // 导入本地文档（txt / md）。返回成功导入的文件数。
    // 导入后会自动重建 Agent，下一次提问就能检索到。
    int ImportKnowledge(const QStringList& files);

    int KnowledgeFileCount() const { return m_knowledgeFiles.size(); }

    int KnowledgeChunkCount() const { return m_knowledgeChunkCount; }

    // 已导入的文件名（不含完整路径），给界面列表用
    QStringList KnowledgeFiles() const { return m_knowledgeFiles; }

    void ClearKnowledge();

public slots:

    void Ask(const QString& question);

signals:

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

    // 本地知识库（轻量 RAG）。属于通用能力，所以由本类持有，
    // 有文档导进来才创建，省得没用到还占着内存。
    std::unique_ptr<agent4cpp::InMemoryKnowledgeStore> m_knowledge;
    QStringList m_knowledgeFiles;
    int m_knowledgeChunkCount { 0 };

    AiSession m_session;

    // 本轮问题。Ask 发起时记下，后台跑完回到主线程时才用得上，
    // 用来把这一轮写进会话历史
    QString m_pendingQuestion;

    QFutureWatcher<AiResult>* m_watcher;

    bool m_busy { false };
};

#endif
