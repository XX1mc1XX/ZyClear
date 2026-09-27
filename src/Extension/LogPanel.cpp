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

// 环形缓冲，只留最近若干行，避免长时间运行吃光内存
QStringList g_applicationLog;
QMutex g_applicationLogMutex;
const int kMaxApplicationLogLines = 2000;

const qint64 kMaxTailBytes = 256 * 1024;

// 装钩子后必须转发给原处理器，否则调试器里看不到 qDebug
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

// 时间戳与级别前缀在这里补：Qt 的默认输出不带时间，日志已经打出去就再也补不回来了。
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

    // 1.5 秒刷一次，日志不需要更实时
    m_pRefreshTimer->start(1500);
    OnRefresh();
}

LogPanel::~LogPanel()
{
}

void LogPanel::setupUi()
{
    setObjectName("LogPanel");

    // 不设最小高度，底部面板会缩成只剩一条标签栏
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
    // 只装一次：钩子装上后，重复调用等于把 MessageHandler 自己再接一层，
    // 每条日志会被记两遍。
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

    // 两种情况的 entryInfoList 重载不同，没法合成一个三元表达式
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

    const qint64 size = file.size();
    // 按字节切尾，可能正好切进一个多字节 UTF-8 字符中间，fromUtf8 会在开头留一个
    // 替换字符 —— 换取的是不必把整个大文件读进内存。
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
    // 只有应用日志是本进程内存里的环形缓冲；AI 决策日志与相机 SDK 日志
    // 由别的模块/库直接写在磁盘上，拿不到它们的写入时机，只能轮询读文件尾部。
    const QString appDir = QCoreApplication::applicationDirPath();
    QString content;
    QString caption;

    switch (m_currentSource) {
    case 0: {
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
    // 留几像素余量：贴底判定常因像素取整差一两格，用严格相等反而在真正贴底时不跟滚。
    const bool wasAtBottom = bar->value() >= bar->maximum() - 4;

    m_pView->setPlainText(content);

    if (m_pAutoScrollCheck->isChecked() && wasAtBottom) {
        bar->setValue(bar->maximum());
    }
}

void LogPanel::OnClearClicked()
{
    // 只清显示，不动磁盘上的日志文件
    m_pView->clear();
}
