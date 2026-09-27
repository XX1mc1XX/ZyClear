#include "AiSettingsDialog.h"

#ifdef ZYCLEAR_HAS_AI

#include "AiConfig.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// 三家都是 OpenAI 兼容接口，同一份客户端代码通吃
struct ProviderPreset {
    const char* name;
    const char* baseUrl;
    const char* model;
    const char* hint;
};

const ProviderPreset kPresets[] = {
    { "DeepSeek", "https://api.deepseek.com/v1", "deepseek-chat",
        "OpenAI 兼容，国内直连，支持工具调用" },
    { "智谱 GLM", "https://open.bigmodel.cn/api/paas/v4", "glm-4-plus",
        "OpenAI 兼容，国内直连，支持工具调用" },
    { "本地 Ollama", "http://localhost:11434/v1", "qwen2.5",
        "完全离线，不需要 Key（随便填一个即可），但小模型的工具调用能力有限" },
    { "自定义", "", "", "手动填写任意 OpenAI 兼容服务的地址与模型名" },
};

const int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace

AiSettingsDialog::AiSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_pProviderCombo(new QComboBox(this))
    , m_pBaseUrlEdit(new QLineEdit(this))
    , m_pModelEdit(new QLineEdit(this))
    , m_pApiKeyEdit(new QLineEdit(this))
    , m_pMaxTokensSpin(new QSpinBox(this))
{
    setupUi();
    LoadFromConfig();
}

AiSettingsDialog::~AiSettingsDialog()
{
}

void AiSettingsDialog::setupUi()
{
    setWindowTitle(QStringLiteral("AI 助手设置"));
    setMinimumWidth(460);

    for (int index = 0; index < kPresetCount; ++index) {
        m_pProviderCombo->addItem(QString::fromUtf8(kPresets[index].name));
    }

    m_pBaseUrlEdit->setPlaceholderText(QStringLiteral("https://api.example.com/v1"));
    m_pModelEdit->setPlaceholderText(QStringLiteral("deepseek-chat"));

    m_pApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_pApiKeyEdit->setPlaceholderText(QStringLiteral("sk-..."));

    m_pMaxTokensSpin->setRange(256, 8192);
    m_pMaxTokensSpin->setSingleStep(256);
    m_pMaxTokensSpin->setToolTip(QStringLiteral("单次回复的最大长度，也直接决定单次费用"));

    QLabel* hintLabel = new QLabel(this);
    hintLabel->setWordWrap(true);
    hintLabel->setObjectName("aiSettingsHint");

    QFormLayout* form = new QFormLayout();
    form->addRow(QStringLiteral("服务商"), m_pProviderCombo);
    form->addRow(QStringLiteral("接口地址"), m_pBaseUrlEdit);
    form->addRow(QStringLiteral("模型名"), m_pModelEdit);
    form->addRow(QStringLiteral("API Key"), m_pApiKeyEdit);
    form->addRow(QStringLiteral("最大回复长度"), m_pMaxTokensSpin);

    QLabel* securityLabel = new QLabel(
        QStringLiteral("API Key 只保存在本机配置中，不会写入源码或随项目提交。"), this);
    securityLabel->setWordWrap(true);
    securityLabel->setObjectName("aiSettingsSecurity");

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(hintLabel);
    layout->addWidget(securityLabel);
    layout->addWidget(buttons);

    connect(m_pProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &AiSettingsDialog::onProviderChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &AiSettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &AiSettingsDialog::reject);

    onProviderChanged(m_pProviderCombo->currentIndex());
}

void AiSettingsDialog::LoadFromConfig()
{
    const AiConfig config = AiConfig::Load();

    m_pBaseUrlEdit->setText(config.baseUrl);
    m_pModelEdit->setText(config.model);
    m_pApiKeyEdit->setText(config.apiKey);
    m_pMaxTokensSpin->setValue(config.maxTokens);

    // 按地址全等反查预设：用户手工改过地址就老实地落在「自定义」，不去猜最接近的那家
    for (int index = 0; index < kPresetCount - 1; ++index) {
        if (config.baseUrl == QString::fromUtf8(kPresets[index].baseUrl)) {
            m_pProviderCombo->setCurrentIndex(index);
            return;
        }
    }
    m_pProviderCombo->setCurrentIndex(kPresetCount - 1);
}

void AiSettingsDialog::onProviderChanged(int index)
{
    if (index < 0 || index >= kPresetCount) {
        return;
    }

    const ProviderPreset& preset = kPresets[index];

    // 最后一档「自定义」不动输入框，保留用户已填的内容；只有选中具体服务商才覆盖
    if (index < kPresetCount - 1) {
        m_pBaseUrlEdit->setText(QString::fromUtf8(preset.baseUrl));
        m_pModelEdit->setText(QString::fromUtf8(preset.model));
    }

    setToolTip(QString::fromUtf8(preset.hint));
    if (QLabel* hint = findChild<QLabel*>("aiSettingsHint")) {
        hint->setText(QString::fromUtf8(preset.hint));
    }
}

void AiSettingsDialog::onAccepted()
{
    const QString baseUrl = m_pBaseUrlEdit->text().trimmed();
    const QString model = m_pModelEdit->text().trimmed();
    const QString apiKey = m_pApiKeyEdit->text().trimmed();

    if (baseUrl.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置不完整"),
            QStringLiteral("接口地址不能为空。"));
        return;
    }
    if (model.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置不完整"),
            QStringLiteral("模型名不能为空。"));
        return;
    }
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置不完整"),
            QStringLiteral("API Key 不能为空。本地 Ollama 不需要真实 Key，随便填一个即可。"));
        return;
    }

    // 先读回磁盘再逐项改：本对话框没暴露温度和超时，直接新建配置会把它们抹成默认值
    AiConfig config = AiConfig::Load();
    config.baseUrl = baseUrl;
    config.model = model;
    config.apiKey = apiKey;
    config.maxTokens = m_pMaxTokensSpin->value();
    config.Save();

    accept();
}

#endif // ZYCLEAR_HAS_AI
