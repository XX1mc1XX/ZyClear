#include "AiConfig.h"

#include <QSettings>

namespace {

// 配置键名集中在这里。散落在各处写字符串，改键名时必然漏改一处
const char* kKeyBaseUrl = "ai/baseUrl";
const char* kKeyModel = "ai/model";
const char* kKeyApiKey = "ai/apiKey";
const char* kKeyTemperature = "ai/temperature";
const char* kKeyMaxTokens = "ai/maxTokens";
const char* kKeyTimeoutMs = "ai/timeoutMs";

// 显式指定组织名和应用名，不依赖 QCoreApplication 里设的值，
// 这样这个类在任何启动阶段都能安全使用
QSettings MakeSettings()
{
    return QSettings(QStringLiteral("ZyClear"), QStringLiteral("ZyClear"));
}

} // namespace

AiConfig AiConfig::Default()
{
    AiConfig config;
    // 默认给 DeepSeek：OpenAI 兼容接口，支持 Function Calling，
    // 国内直连不需要代理。换成智谱或本地 Ollama 只需在设置里改这两项
    config.baseUrl = QStringLiteral("https://api.deepseek.com/v1");
    config.model = QStringLiteral("deepseek-chat");
    config.apiKey = QString();
    return config;
}

AiConfig AiConfig::Load()
{
    AiConfig config = Default();
    QSettings settings = MakeSettings();

    config.baseUrl = settings.value(kKeyBaseUrl, config.baseUrl).toString();
    config.model = settings.value(kKeyModel, config.model).toString();
    config.apiKey = settings.value(kKeyApiKey, config.apiKey).toString();
    config.temperature = settings.value(kKeyTemperature, config.temperature).toDouble();
    config.maxTokens = settings.value(kKeyMaxTokens, config.maxTokens).toInt();
    config.timeoutMs = settings.value(kKeyTimeoutMs, config.timeoutMs).toInt();

    return config;
}

void AiConfig::Save() const
{
    QSettings settings = MakeSettings();

    settings.setValue(kKeyBaseUrl, baseUrl);
    settings.setValue(kKeyModel, model);
    settings.setValue(kKeyApiKey, apiKey);
    settings.setValue(kKeyTemperature, temperature);
    settings.setValue(kKeyMaxTokens, maxTokens);
    settings.setValue(kKeyTimeoutMs, timeoutMs);

    settings.sync();
}

bool AiConfig::IsValid() const
{
    return !baseUrl.trimmed().isEmpty()
        && !model.trimmed().isEmpty()
        && !apiKey.trimmed().isEmpty();
}

QString AiConfig::ApiKeyEnvName()
{
    return QStringLiteral("ZYCLEAR_AI_API_KEY");
}

QString AiConfig::ApplyApiKeyToEnv() const
{
    const QString name = ApiKeyEnvName();

    // qputenv 改的是当前进程的环境变量，不影响系统设置，
    // 也不会被别的程序看到。进程退出即消失。
    qputenv(name.toUtf8().constData(), apiKey.trimmed().toUtf8());

    return name;
}
