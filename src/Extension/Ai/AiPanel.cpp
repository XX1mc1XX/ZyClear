#include "AiPanel.h"

#include "AiDialogs.h"
#include "AiSettingsDialog.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// 三种角色用三种颜色，一眼分清谁在说话
const char* kColorUser = "#1a73e8";
const char* kColorAssistant = "#188038";
const char* kColorError = "#d93025";
const char* kColorMuted = "#7a7a7a";

} // namespace

AiPanel::AiPanel(QWidget* parent)
    : QWidget(parent)
    , m_pService(new AiAgentService(this))
    , m_pHistory(new QTextBrowser(this))
    , m_pInput(new QPlainTextEdit(this))
    , m_pSendButton(new QPushButton(QStringLiteral("发送"), this))
    , m_pSettingsButton(new QToolButton(this))
    , m_pNewSessionButton(new QToolButton(this))
    , m_pHistoryButton(new QToolButton(this))
    , m_pKnowledgeButton(new QToolButton(this))
    , m_pStatusLabel(new QLabel(this))
    , m_pTitleLabel(new QLabel(QStringLiteral("AI 助手"), this))
{
    setupUi();

    connect(m_pSendButton, &QPushButton::clicked, this, &AiPanel::onSendClicked);
    connect(m_pSettingsButton, &QToolButton::clicked, this, &AiPanel::onSettingsClicked);
    connect(m_pNewSessionButton, &QToolButton::clicked, this, &AiPanel::onNewSessionClicked);
    connect(m_pHistoryButton, &QToolButton::clicked, this, &AiPanel::onHistoryClicked);
    connect(m_pKnowledgeButton, &QToolButton::clicked, this, &AiPanel::onKnowledgeClicked);

    connect(m_pService, &AiAgentService::SigFinished, this, &AiPanel::onFinished);
    connect(m_pService, &AiAgentService::SigBusyChanged, this, &AiPanel::onBusyChanged);
    connect(m_pService, &AiAgentService::SigSessionsChanged, this, &AiPanel::onSessionsChanged);
    connect(m_pService, &AiAgentService::SigKnowledgeChanged, this, &AiPanel::onKnowledgeChanged);

    appendWelcome();
    refreshHeader();
    onBusyChanged(false);
}

AiPanel::~AiPanel()
{
}

void AiPanel::SetToolProviders(const QList<IToolProvider*>& providers)
{
    m_pService->SetToolProviders(providers);

    refreshHeader();
    onBusyChanged(false);
}

void AiPanel::setupUi()
{
    setObjectName("AiPanel");

    // 允许被拖到很窄。上限由内部控件决定，所以这里统一用「图标按钮」而不是
    // 文字按钮 —— 文字一多，最小宽度就下不去了。
    setMinimumWidth(120);

    m_pTitleLabel->setObjectName("aiTitle");
    m_pStatusLabel->setObjectName("aiStatus");

    m_pHistory->setObjectName("aiHistory");
    m_pHistory->setOpenExternalLinks(false);
    m_pHistory->setReadOnly(true);
    m_pHistory->setMinimumWidth(0);

    m_pInput->setObjectName("aiInput");
    m_pInput->setPlaceholderText(QStringLiteral("用一句话描述你要做什么，回车发送"));
    m_pInput->setFixedHeight(56);
    m_pInput->setMinimumWidth(0);
    m_pInput->installEventFilter(this);

    // 图标用 Qt 自带的标准图标，不额外往资源里加图片
    m_pNewSessionButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_pNewSessionButton->setToolTip(QStringLiteral("开始新会话（清空当前上下文）"));
    m_pNewSessionButton->setAutoRaise(true);

    m_pHistoryButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_pHistoryButton->setToolTip(QStringLiteral("对话历史"));
    m_pHistoryButton->setAutoRaise(true);

    m_pKnowledgeButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    m_pKnowledgeButton->setToolTip(QStringLiteral("知识库：导入本地文档，提问时自动附带相关内容"));
    m_pKnowledgeButton->setAutoRaise(true);

    m_pSettingsButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_pSettingsButton->setToolTip(QStringLiteral("模型设置"));
    m_pSettingsButton->setAutoRaise(true);

    m_pSendButton->setMinimumWidth(0);

    // 第一行：标题 + 状态
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(m_pTitleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_pStatusLabel);

    // 第二行：全部操作入口，图标化后排在一行，窄了也不会换行错位
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(2);
    actionLayout->addWidget(m_pNewSessionButton);
    actionLayout->addWidget(m_pHistoryButton);
    actionLayout->addWidget(m_pKnowledgeButton);
    actionLayout->addWidget(m_pSettingsButton);
    actionLayout->addStretch();

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->addWidget(m_pInput, 1);
    inputLayout->addWidget(m_pSendButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addLayout(titleLayout);
    layout->addLayout(actionLayout);
    layout->addWidget(m_pHistory, 1);
    layout->addLayout(inputLayout);
}

void AiPanel::appendWelcome()
{
    const QStringList examples = m_pService->ExamplePrompts();

    QString listHtml;
    for (const QString& example : examples) {
        listHtml += QStringLiteral("· %1<br/>").arg(example.toHtmlEscaped());
    }

    if (listHtml.isEmpty()) {
        listHtml = QStringLiteral("· 这个客户端还没有注册可操作的能力<br/>");
    }

    m_pHistory->append(QStringLiteral(
        "<div style='margin:8px 0;color:%1'>"
        "用日常说法描述你要做什么，AI 会先读真实状态再动手。<br/><br/>"
        "%2<br/>"
        "每次调用的工具都会显示在下面 —— 它不会凭空猜设备状态。"
        "</div>")
                           .arg(QString::fromLatin1(kColorMuted), listHtml));

    m_pHistory->append(QStringLiteral("<hr/>"));
}

void AiPanel::appendBubble(const QString& title, const QString& body, const QString& color)
{
    // toHtmlEscaped 是必须的：模型输出里可能带 < > 之类字符，
    // 直接拼进 HTML 会被当成标签，轻则显示错乱，重则执行脚本
    QString escaped = body.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));

    m_pHistory->append(QStringLiteral("<div style='margin:8px 0;'>"
                                      "<span style='color:%1;font-weight:bold;'>%2</span>"
                                      "<div style='margin-top:2px;'>%3</div>"
                                      "</div>")
                           .arg(color, title.toHtmlEscaped(), escaped));

    QScrollBar* bar = m_pHistory->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void AiPanel::appendTrace(const QStringList& trace)
{
    QStringList lines;
    for (const QString& item : trace) {
        lines << QStringLiteral("· %1").arg(item.toHtmlEscaped());
    }

    m_pHistory->append(QStringLiteral("<div style='margin:2px 0 8px 0;color:%1;font-size:11px;'>"
                                      "调用了 %2 次工具：<br/>%3</div>")
                           .arg(QString::fromLatin1(kColorMuted))
                           .arg(trace.size())
                           .arg(lines.join(QStringLiteral("<br/>"))));

    QScrollBar* bar = m_pHistory->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void AiPanel::setStatus(const QString& text)
{
    m_pStatusLabel->setText(text);
}

void AiPanel::refreshHeader()
{
    // 停靠面板的标题栏已经写着「AI 助手」了，这里不再重复，
    // 改成显示更有用的信息：当前会不会带参考资料一起问。
    const int files = m_pService->KnowledgeFileCount();
    if (files > 0) {
        m_pTitleLabel->setText(QStringLiteral("资料 %1").arg(files));
        m_pTitleLabel->setToolTip(QStringLiteral("已导入 %1 个文档 / %2 个片段")
                                      .arg(files)
                                      .arg(m_pService->KnowledgeChunkCount()));
    } else {
        m_pTitleLabel->setText(QStringLiteral("会话 %1").arg(m_pService->CurrentSessionId().right(6)));
        m_pTitleLabel->setToolTip(QStringLiteral("当前会话 id：%1").arg(m_pService->CurrentSessionId()));
    }
}

bool AiPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_pInput && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        const bool isEnter
            = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
        // 回车发送，Shift+回车换行 —— 和常见的聊天工具一致
        if (isEnter && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            onSendClicked();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AiPanel::onSendClicked()
{
    if (m_pService->IsBusy()) {
        return;
    }

    const QString question = m_pInput->toPlainText().trimmed();
    if (question.isEmpty()) {
        return;
    }

    if (!m_pService->IsReady()) {
        appendBubble(QStringLiteral("提示"), m_pService->ConfigHint(), kColorError);
        return;
    }

    appendBubble(QStringLiteral("你"), question, kColorUser);
    m_pInput->clear();
    m_pService->Ask(question);
}

void AiPanel::onSettingsClicked()
{
    if (m_pService->IsBusy()) {
        appendBubble(QStringLiteral("提示"),
            QStringLiteral("AI 正在处理上一句，等它答完再改设置。"), kColorMuted);
        return;
    }

    AiSettingsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_pService->ReloadConfig();
    onBusyChanged(false);
    appendBubble(QStringLiteral("提示"), QStringLiteral("配置已保存。"), kColorMuted);
}

void AiPanel::onNewSessionClicked()
{
    if (m_pService->IsBusy()) {
        appendBubble(QStringLiteral("提示"), QStringLiteral("等这一轮答完再开新会话。"), kColorMuted);
        return;
    }

    m_pService->StartNewSession();
    m_pHistory->clear();
    appendWelcome();
    refreshHeader();
}

void AiPanel::onHistoryClicked()
{
    SessionDialog dialog(m_pService, this);
    dialog.exec();
}

void AiPanel::onKnowledgeClicked()
{
    KnowledgeDialog dialog(m_pService, this);
    dialog.exec();
}

void AiPanel::onFinished(AiResult result)
{
    if (!result.trace.isEmpty()) {
        appendTrace(result.trace);
    }

    if (result.ok()) {
        appendBubble(QStringLiteral("AI"), result.answer, kColorAssistant);
    } else {
        appendBubble(QStringLiteral("出错"), result.error, kColorError);
    }
}

void AiPanel::onBusyChanged(bool busy)
{
    const bool ready = m_pService->IsReady();

    m_pSendButton->setEnabled(!busy && ready);
    m_pInput->setEnabled(!busy);

    if (busy) {
        setStatus(QStringLiteral("思考中…"));
    } else if (ready) {
        setStatus(QStringLiteral("就绪"));
    } else {
        setStatus(QStringLiteral("未配置"));
    }
}

void AiPanel::onSessionsChanged()
{
    refreshHeader();
}

void AiPanel::onKnowledgeChanged()
{
    refreshHeader();

    const int files = m_pService->KnowledgeFileCount();
    if (files == 0) {
        appendBubble(QStringLiteral("提示"), QStringLiteral("知识库已清空。"), kColorMuted);
    } else {
        appendBubble(QStringLiteral("提示"),
            QStringLiteral("已导入 %1 个文档（%2 个片段），之后的提问会自动附带相关内容。")
                .arg(files)
                .arg(m_pService->KnowledgeChunkCount()),
            kColorMuted);
    }
}
