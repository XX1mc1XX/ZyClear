#include "AiAgentService.h"

#include "AiConfig.h"

#include "agent4cpp/agent.h"
#include "agent4cpp/knowledge_store.h"
#include "agent4cpp/llm_client.h"
#include "agent4cpp/openai_compatible_llm_client.h"
#include "agent4cpp/tool_registry.h"

#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

#include <string>
#include <vector>

namespace {

// 给模型定身份和纪律。
//
// 这段文案直接决定行为边界，尤其是第 2 条：
// 设备状态必须来自工具，不能让模型凭常识编 ——
// 「相机曝光大概 5000 微秒」这种话听起来合理，但和真实设备毫无关系。
const char* kBaseSystemPrompt =
    "你是这个上位机软件的操作助手。几条纪律："
    "1) 用户会用日常说法描述问题，你需要把它翻译成具体的操作；"
    "2) 凡是能用工具确认的事实（设备状态、参数当前值、画面指标），"
    "必须先调工具再回答，不要凭常识推测设备状态；"
    "3) 改参数前先看清取值范围，改完以回读值为准；"
    "4) 最后用一句中文总结你做了什么、结果如何。回答简短，不要长篇解释。";

// 从完整对话记录里把「模型要求调用哪些工具」抽出来。
// 界面把这串显示给用户看，AI 就不再是个黑盒。
QStringList BuildTrace(const std::vector<agent4cpp::ChatMessage>& transcript)
{
    QStringList trace;
    for (const agent4cpp::ChatMessage& message : transcript) {
        if (message.role != agent4cpp::ChatRole::kAssistant || !message.tool_call.has_value()) {
            continue;
        }
        const agent4cpp::ToolCall& call = *message.tool_call;
        trace << QStringLiteral("%1 %2")
                     .arg(QString::fromStdString(call.name),
                         QString::fromStdString(call.arguments_json));
    }
    return trace;
}

} // namespace

AiAgentService::AiAgentService(QObject* parent)
    : QObject(parent)
    , m_watcher(new QFutureWatcher<AiResult>(this))
{
    m_session = HistoryStore::CreateNew();

    // watcher 在主线程收到完成通知，再转成自己的信号发出去
    connect(m_watcher, &QFutureWatcher<AiResult>::finished, this, [this]() {
        const AiResult result = m_watcher->result();

        // 把这一轮写进当前会话并落盘。
        // 只有成功或失败都记 —— 失败也是「这次我问过什么」的一部分。
        AiTurn turn;
        turn.question = m_pendingQuestion;
        turn.answer = result.answer;
        turn.error = result.error;
        turn.trace = result.trace;
        m_session.turns.append(turn);

        if (m_session.title.isEmpty()) {
            // 用第一句问题当标题，够认出是哪次对话
            m_session.title = m_pendingQuestion.left(24);
        }
        HistoryStore::Save(m_session);

        m_pendingQuestion.clear();
        m_busy = false;

        emit SigBusyChanged(false);
        emit SigSessionsChanged();
        emit SigFinished(result);
    });

    BuildAgent();
}

AiAgentService::~AiAgentService() = default;

void AiAgentService::SetToolProviders(const QList<IToolProvider*>& providers)
{
    m_providers = providers;
    BuildAgent();
}

void AiAgentService::BuildAgent()
{
    // 工具登记处每次重建：配置或提供者变化后不残留上一次的注册状态
    m_registry = std::make_unique<agent4cpp::ToolRegistry>();

    for (IToolProvider* provider : m_providers) {
        if (provider != nullptr) {
            provider->RegisterTools(*m_registry);
        }
    }

    const AiConfig config = AiConfig::Load();

    // 没有可用工具、或者模型配置不全，就不建 Agent。
    // 界面据此禁用输入框并给出提示，而不是让用户对着没反应的按钮。
    if (!config.IsValid() || m_providers.isEmpty()) {
        m_agent.reset();
        return;
    }

    // ★ agent4cpp 不接收 Key 本身，只接收「环境变量名」。
    //   所以这里把界面填的 Key 注入进程内环境变量，再把名字交给它。
    //   这样 Key 不会出现在源码、配置文件或日志里。
    const QString envName = config.ApplyApiKeyToEnv();

    agent4cpp::OpenAICompatibleLLMConfig llmConfig;
    llmConfig.base_url = config.baseUrl.trimmed().toStdString();
    llmConfig.model = config.model.trimmed().toStdString();
    llmConfig.api_key_env = envName.toStdString();
    llmConfig.temperature = config.temperature;
    llmConfig.max_tokens = config.maxTokens;
    llmConfig.timeout_ms = config.timeoutMs;

    auto client = std::make_shared<agent4cpp::OpenAICompatibleLLMClient>(llmConfig);

    // 系统提示词由「通用纪律」+「本客户端能操作什么」拼成。
    // 后半段来自注入的工具提供者，所以换个客户端这里自动就变了。
    QString systemPrompt = QString::fromUtf8(kBaseSystemPrompt);
    for (IToolProvider* provider : m_providers) {
        if (provider == nullptr) {
            continue;
        }
        systemPrompt += QStringLiteral("\n- 可操作对象「%1」：%2")
                            .arg(provider->ProviderName(), provider->ProviderDescription());
    }

    agent4cpp::AgentConfig agentConfig;
    agentConfig.system_prompt = systemPrompt.toStdString();
    agentConfig.max_steps = 8; // 兜底，防止模型反复调工具不收敛
    agentConfig.knowledge_store = m_knowledge.get(); // 没导入过文档就是 nullptr，会自动跳过
    agentConfig.knowledge_top_k = 3;

    m_agent = std::make_shared<agent4cpp::Agent>(agentConfig, m_registry.get(), client);
}

bool AiAgentService::IsReady() const
{
    return m_agent != nullptr;
}

QString AiAgentService::ConfigHint() const
{
    if (m_agent != nullptr) {
        return QString();
    }
    if (m_providers.isEmpty()) {
        return QStringLiteral("还没有可操作的对象：宿主程序需要先注册工具提供者。");
    }
    return QStringLiteral("AI 尚未配置：请点「设置」填写接口地址、模型名和 API Key。");
}

QStringList AiAgentService::ExamplePrompts() const
{
    QStringList prompts;
    for (IToolProvider* provider : m_providers) {
        if (provider != nullptr) {
            prompts << provider->ExamplePrompts();
        }
    }
    return prompts;
}

void AiAgentService::ReloadConfig()
{
    // 正在问答时不动：后台线程正握着 Agent，重建会让它读到半截状态
    if (m_busy) {
        return;
    }
    BuildAgent();
}

void AiAgentService::Ask(const QString& question)
{
    if (m_busy) {
        return; // 上一轮还没回来
    }

    const QString trimmed = question.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (m_agent == nullptr) {
        AiResult result;
        result.error = ConfigHint();
        emit SigFinished(result);
        return;
    }

    m_busy = true;
    m_pendingQuestion = trimmed;
    emit SigBusyChanged(true);

    // 用一份智能指针快照跑这一轮：即使主线程随后重建了 Agent，
    // 这一轮仍然操作原来的对象，不会读到悬空的指针
    const std::shared_ptr<agent4cpp::Agent> agent = m_agent;

    m_watcher->setFuture(QtConcurrent::run([agent, trimmed]() -> AiResult {
        AiResult result;

        const agent4cpp::AgentResponse response = agent->Run(trimmed.toStdString());

        result.trace = BuildTrace(response.transcript);

        if (response.status.ok()) {
            result.answer = QString::fromStdString(response.content);
            if (result.answer.trimmed().isEmpty()) {
                result.answer = QStringLiteral("(模型没有给出文字答复)");
            }
        } else {
            result.error = QString::fromStdString(response.status.ToString());
        }

        return result;
    }));
}

// =============================================================================
// 会话历史
// =============================================================================

void AiAgentService::StartNewSession()
{
    if (m_busy) {
        return;
    }

    m_session = HistoryStore::CreateNew();

    if (m_agent != nullptr) {
        m_agent->Reset(); // 清空模型侧上下文，开一段新对话
    } else {
        BuildAgent();
    }

    emit SigSessionsChanged();
}

QList<AiSession> AiAgentService::Sessions() const
{
    return HistoryStore::List();
}

bool AiAgentService::DeleteSession(const QString& id)
{
    if (id == m_session.id) {
        // 删的正好是当前会话：删完顺手开一个新的，免得界面停在已删状态
        const bool removed = HistoryStore::Remove(id);
        if (removed) {
            m_session = HistoryStore::CreateNew();
            if (m_agent != nullptr) {
                m_agent->Reset();
            }
            emit SigSessionsChanged();
        }
        return removed;
    }

    const bool removed = HistoryStore::Remove(id);
    if (removed) {
        emit SigSessionsChanged();
    }
    return removed;
}

// =============================================================================
// 知识库（轻量 RAG）
// =============================================================================

void AiAgentService::EnsureKnowledgeStore()
{
    if (m_knowledge == nullptr) {
        m_knowledge = std::make_unique<agent4cpp::InMemoryKnowledgeStore>();
    }
}

int AiAgentService::ImportKnowledge(const QStringList& files)
{
    if (m_busy || files.isEmpty()) {
        return 0;
    }

    EnsureKnowledgeStore();

    int imported = 0;
    for (const QString& path : files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QString text = QString::fromUtf8(file.readAll());
        if (text.trimmed().isEmpty()) {
            continue;
        }

        // 自己切块而不是调 LoadTextFile，是为了能数出到底进了多少片段，
        // 界面要显示这个数字让用户确认导入真的生效了。
        const QString name = QFileInfo(path).fileName();
        const std::vector<agent4cpp::DocumentChunk> chunks
            = agent4cpp::ChunkText(text.toStdString(), name.toStdString());

        for (const agent4cpp::DocumentChunk& chunk : chunks) {
            m_knowledge->Add(chunk);
        }

        m_knowledgeChunkCount += static_cast<int>(chunks.size());
        if (!m_knowledgeFiles.contains(name)) {
            m_knowledgeFiles << name;
        }
        ++imported;
    }

    if (imported > 0) {
        // 重建 Agent 让它挂上新知识库。
        // 注意这会重置模型侧的对话上下文 —— 因为 agent4cpp 没有「把历史消息
        // 注入回 Agent」的入口，所以这里顺手开一段新会话，语义上更诚实：
        // 用户看得见「导入资料后开了一段新对话」，而不是以为上下文还在。
        m_session = HistoryStore::CreateNew();
        BuildAgent();
        emit SigKnowledgeChanged();
        emit SigSessionsChanged();
    }

    return imported;
}

void AiAgentService::ClearKnowledge()
{
    if (m_busy) {
        return;
    }

    m_knowledge.reset();
    m_knowledgeFiles.clear();
    m_knowledgeChunkCount = 0;

    BuildAgent();
    emit SigKnowledgeChanged();
}
