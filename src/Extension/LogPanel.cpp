#include "LogPanel.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMutex>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTabBar>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// 应用日志的内存缓冲。这是个环形缓冲：只保留最近若干行，
// 否则程序跑一整天会把内存吃光。
QStringList g_applicationLog;
QMutex g_applicationLogMutex;
const int kMaxApplicationLogLines = 2000;

// 读文件尾部时最多读多少字节
const qint64 kMaxTailBytes = 256 * 1024;

// 原消息处理器。装钩子之后要把它转发回去，
// 否则调试器的输出窗口里就再也看不到 qDebug 了
QtMessageHandler g_previousHandler = nullptr;

const char* LevelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "debug";
    case QtInfoMsg:
        return "info ";
    case QtWarningMsg:
        return "warn ";
    case QtCriticalMsg:
        return "error";
    case QtFatalMsg:
        return "fatal";
    }
    return "-----";
}

void MessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = QStringLiteral("[%1] [%2] %3")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                 QString::fromLatin1(LevelName(type)), message);

    {
        QMutexLocker locker(&g_applicationLogMutex);
        g_applicationLog.append(line);
        while (g_applicationLog.size() > kMaxApplicationLogLines) {
            g_applicationLog.removeFirst();
        }
    }

    // 继续交给原来的处理器，别把调试器输出截断
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, message);
    }
}

} // namespace

void LogPanel::AppendApplicationLog(const QString& line)
{
    QMutexLocker locker(&g_applicationLogMutex);
    g_applicationLog.append(line);
    while (g_applicationLog.size() > kMaxApplicationLogLines) {
        g_applicationLog.removeFirst();
    }
}

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
    , m_pSourceTabs(new QTabBar(this))
    , m_pView(new QPlainTextEdit(this))
    , m_pAutoScrollCheck(new QCheckBox(QStringLiteral("自动滚动"), this))
    , m_pRefreshTimer(new QTimer(this))
{
    setupUi();
    InstallMessageHandler();

    connect(m_pSourceTabs, &QTabBar::currentChanged, this, &LogPanel::OnSourceChanged);
    connect(m_pRefreshTimer, &QTimer::timeout, this, &LogPanel::OnRefresh);

    // 1.5 秒刷一次。日志是给人看的，不需要更实时
    m_pRefreshTimer->start(1500);
    OnRefresh();
}

LogPanel::~LogPanel()
{
}

void LogPanel::setupUi()
{
    setObjectName("LogPanel");

    // 同理：底部面板不设最小高度会缩成一条线；给一个能看出内容的下限，
    // 再小就只剩标签栏了
    setMinimumHeight(60);

    m_pSourceTabs->setObjectName("logSourceTabs");
    m_pSourceTabs->setExpanding(false);
    m_pSourceTabs->addTab(QStringLiteral("AI 助手"));
    m_pSourceTabs->addTab(QStringLiteral("相机 SDK"));
    m_pSourceTabs->addTab(QStringLiteral("应用日志"));

    m_pView->setObjectName("logView");
    m_pView->setReadOnly(true);
    m_pView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_pView->setMaximumBlockCount(kMaxApplicationLogLines * 2);

    m_pAutoScrollCheck->setChecked(true);

    QPushButton* clearButton = new QPushButton(QStringLiteral("清空显示"), this);
    clearButton->setObjectName("logClearButton");
    connect(clearButton, &QPushButton::clicked, this, &LogPanel::OnClearClicked);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(4, 2, 4, 0);
    headerLayout->addWidget(m_pSourceTabs);
    headerLayout->addStretch();
    headerLayout->addWidget(m_pAutoScrollCheck);
    headerLayout->addWidget(clearButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(headerLayout);
    layout->addWidget(m_pView);
}

void LogPanel::InstallMessageHandler()
{
    // 只装一次。装完 qDebug / qWarning 的内容就同时进内存缓冲，
    // 界面里那个「应用日志」标签才有东西可看。
    if (g_previousHandler == nullptr) {
        g_previousHandler = qInstallMessageHandler(MessageHandler);
    }
}

QString LogPanel::NewestFileIn(const QString& directory, const QString& suffix)
{
    QDir dir(directory);
    if (!dir.exists()) {
        return QString();
    }

    // 不传后缀表示「任意文件」，传了后缀就按后缀过滤。
    // 两种情况的 entryInfoList 重载不同，所以分开写而不是塞进一个三元表达式。
    const QFileInfoList entries = suffix.isEmpty()
        ? dir.entryInfoList(QDir::Files, QDir::Time)
        : dir.entryInfoList(QStringList { QStringLiteral("*.") + suffix }, QDir::Files, QDir::Time);

    return entries.isEmpty() ? QString() : entries.first().absoluteFilePath();
}

QString LogPanel::ReadTail(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("(打不开文件：%1)").arg(filePath);
    }

    // 只读尾部：日志可能很大，全读进来会卡住界面
    const qint64 size = file.size();
    if (size > kMaxTailBytes) {
        file.seek(size - kMaxTailBytes);
    }

    return QString::fromUtf8(file.readAll());
}

void LogPanel::OnSourceChanged(int index)
{
    m_currentSource = index;
    m_pView->clear();
    OnRefresh();
}

void LogPanel::OnRefresh()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QString content;
    QString caption;

    switch (m_currentSource) {
    case 0: {
        // AI 助手日志：agent4cpp 每一步决定调什么工具都会写在这里
        const QString file = NewestFileIn(appDir + QStringLiteral("/agent4cpp_log"),
            QStringLiteral("txt"));
        if (file.isEmpty()) {
            content = QStringLiteral(
                "这里会显示 AI 的决策过程（每一步调了哪个工具、参数是什么、结果如何）。\n"
                "先用右侧的 AI 助手问一句，日志就会生成。");
        } else {
            caption = QFileInfo(file).fileName();
            content = ReadTail(file);
        }
        break;
    }
    case 1: {
        // 相机 SDK 自己的运行日志，落盘位置由适配器设置
        const QString dir = appDir + QStringLiteral("/MvSDKLog");
        const QString file = NewestFileIn(dir, QString());
        if (file.isEmpty()) {
            content = QStringLiteral("没有找到相机 SDK 日志（目录：%1）。\n"
                                     "接入真实海康相机并运行后，SDK 会在这里留下日志。")
                          .arg(dir);
        } else {
            caption = QFileInfo(file).fileName();
            content = ReadTail(file);
        }
        break;
    }
    default: {
        // 应用日志：由 qInstallMessageHandler 钩子收集
        QMutexLocker locker(&g_applicationLogMutex);
        content = g_applicationLog.isEmpty()
            ? QStringLiteral("程序自身的 qDebug / qWarning 会显示在这里。")
            : g_applicationLog.join(QLatin1Char('\n'));
        break;
    }
    }

    if (m_pView->toPlainText() == content) {
        return; // 内容没变就不动，免得滚动条来回跳
    }

    QScrollBar* bar = m_pView->verticalScrollBar();
    const bool wasAtBottom = bar->value() >= bar->maximum() - 4;

    m_pView->setPlainText(content);

    if (m_pAutoScrollCheck->isChecked() && wasAtBottom) {
        bar->setValue(bar->maximum());
    }
}

void LogPanel::OnClearClicked()
{
    // 只清显示，不动磁盘上的日志文件 —— 清文件是不可逆操作，不该由一个按钮代劳
    m_pView->clear();
}
