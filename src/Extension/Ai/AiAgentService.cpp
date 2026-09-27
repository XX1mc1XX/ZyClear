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

// 尤其第 2 条：设备状态必须来自工具，不能让模型凭常识编
const char* kBaseSystemPrompt =
    "你是这个上位机软件的操作助手。几条纪律："
    "1) 用户会用日常说法描述问题，你需要把它翻译成具体的操作；"
    "2) 凡是能用工具确认的事实（设备状态、参数当前值、画面指标），"
    "必须先调工具再回答，不要凭常识推测设备状态；"
    "3) 改参数前先看清取值范围，改完以回读值为准；"
    "4) 最后用一句中文总结你做了什么、结果如何。回答简短，不要长篇解释。";

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

    connect(m_watcher, &QFutureWatcher<AiResult>::finished, this, [this]() {
        const AiResult result = m_watcher->result();

        AiTurn turn;
        turn.question = m_pendingQuestion;
        turn.answer = result.answer;
        turn.error = result.error;
        turn.trace = result.trace;
        m_session.turns.append(turn);

        if (m_session.title.isEmpty()) {
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
    m_registry = std::make_unique<agent4cpp::ToolRegistry>();

    for (IToolProvider* provider : m_providers) {
        if (provider != nullptr) {
            provider->RegisterTools(*m_registry);
        }
    }

    const AiConfig config = AiConfig::Load();

    if (!config.IsValid() || m_providers.isEmpty()) {
        m_agent.reset();
        return;
    }

    // agent4cpp 只接收环境变量名而非 Key 本身，这里注入进程内环境变量再把名字交给它
    const QString envName = config.ApplyApiKeyToEnv();

    agent4cpp::OpenAICompatibleLLMConfig llmConfig;
    llmConfig.base_url = config.baseUrl.trimmed().toStdString();
    llmConfig.model = config.model.trimmed().toStdString();
    llmConfig.api_key_env = envName.toStdString();
    llmConfig.temperature = config.temperature;
    llmConfig.max_tokens = config.maxTokens;
    llmConfig.timeout_ms = config.timeoutMs;

    auto client = std::make_shared<agent4cpp::OpenAICompatibleLLMClient>(llmConfig);

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
    agentConfig.knowledge_store = m_knowledge.get();
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
    if (m_busy) {
        return;
    }
    BuildAgent();
}

void AiAgentService::Ask(const QString& question)
{
    if (m_busy) {
        return;
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

    // 用快照跑这一轮：主线程随后重建 Agent 也不会让本轮读到悬空指针
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

void AiAgentService::StartNewSession()
{
    if (m_busy) {
        return;
    }

    m_session = HistoryStore::CreateNew();

    if (m_agent != nullptr) {
        m_agent->Reset();
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

        // 自己切块而不用 LoadTextFile，是为了数出片段数给界面显示
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
        // 重建 Agent 会重置模型侧对话上下文（agent4cpp 没有注入历史消息的入口），
        // 所以顺手开一段新会话，让用户看得见上下文已重置
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
