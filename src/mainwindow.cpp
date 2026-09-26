#include "mainwindow.h"
#include "AppStyle/AppStyle.h"
#include "Extension/BuiltinPanels.h"
#include "Extension/ExtensionHost.h"
#include "Extension/PanelRegistry.h"

#ifdef ZYCLEAR_HAS_AI
#include "Integration/CameraToolProvider.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

namespace {

// 从程序目录下的 extensions/ 加载外部面板插件。
//
// 「一切皆可导入」的入口就在这里：往这个目录丢一个按约定导出两个 C 函数的
// DLL，重启程序就多一个面板，主程序不需要重新编译、也不需要认识它。
//
// 目录不存在、或者某个 DLL 不合规，都只记一条日志继续走 ——
// 扩展永远不该让主程序起不来。
void LoadExternalExtensions(PanelRegistry* registry)
{
    const QDir pluginDir(QCoreApplication::applicationDirPath()
        + QStringLiteral("/extensions"));
    if (!pluginDir.exists()) {
        return;
    }

    const QFileInfoList plugins = pluginDir.entryInfoList(
        QStringList { QStringLiteral("*.dll") }, QDir::Files);

    for (const QFileInfo& plugin : plugins) {
        registry->LoadFromLibrary(plugin.absoluteFilePath());
    }
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_pSplitter(nullptr)
    , m_pControlContainer(nullptr)
    , m_pViewContainer(nullptr)
    , m_pParamContainer(nullptr)
    , m_pControlWidget(new ControlWidget())
    , m_pParamWidget(new ParamWidget())
    , m_pViewWidget(new ViewWidget())
    , m_pErrorInfoLabel(new QLabel(""))
{
    setupUi();

    m_pControlContainer->layout()->addWidget(m_pControlWidget);
    m_pParamContainer->layout()->addWidget(m_pParamWidget);
    m_pViewContainer->layout()->addWidget(m_pViewWidget);
    statusBar()->addWidget(m_pErrorInfoLabel);
    statusBar()->setSizeGripEnabled(false);

    // 窗口按屏幕「可用区域」开窗，而不是写死一个尺寸。
    //
    // 为什么必须这样：系统缩放 175% 时，屏幕物理 1463x914 对应的逻辑尺寸
    // 只有 836x522。写死 resize(1000, 600) 会让窗口的物理尺寸变成 1750x1050，
    // 比屏幕还大 —— 右侧的参数面板、AI 助手会被整块推到屏幕外，
    // 用户只看到左边两栏，却没有任何报错。
    // 窗口默认尺寸。
    //
    // 【为什么不用 availableGeometry 自己算】不同 DPI 配置下 Qt 报回的
    //   可用区域单位并不一致（本次实测：系统层面看到 1463x914，
    //   Qt 看到 2560x1600）。照着算反而可能把窗口开得比屏幕还大，
    //   或者小得离谱。给一个在 1080p 及以上都合适的固定值，
    //   用户拖一次边框就够了，比猜坐标系可靠。
    resize(1000, 600);

    // QSplitter 里只放三栏常驻内容。AI 助手和日志是可关闭面板，走 QDockWidget，
    // 不占 splitter 的份额 —— 否则图像区要一直给它们让出一块宽度，关都关不掉。
    // QSplitter 默认按 sizeHint 分配，图像视图的 sizeHint 很大，会把参数面板
    // 压成 0 宽，所以这里显式规定只有图像区吃掉多余空间。
    m_pSplitter->setStretchFactor(0, 0);
    m_pSplitter->setStretchFactor(1, 1);
    m_pSplitter->setStretchFactor(2, 0);
    m_pSplitter->setSizes({ 170, 300, 250 });

    connect(m_pControlWidget, &ControlWidget::SigUpdateErrorInfo, this, &MainWindow::OnUpdateErrorInfo);
    connect(m_pParamWidget, &ParamWidget::SigUpdateErrorInfo, this, &MainWindow::OnUpdateErrorInfo);
    connect(m_pViewWidget, &ViewWidget::SigUpdateErrorInfo, this, &MainWindow::OnUpdateErrorInfo);

    setWindowTitle(QStringLiteral("智澈-机器视觉上位机软件(海康工业相机SDK二次开发且兼容所有别的品牌相机)-https://github.com/XX1mc1XX/"));
    setWindowIcon(QIcon(":/favicon.ico"));

    // 装配可扩展面板。
    //
    // 主窗口到这里为止只认识 PanelRegistry 这个抽象，不认识任何一个具体面板：
    // AI 助手挂右侧、日志挂底部，都是注册处说了算。以后加面板只动注册处一行，
    // 或者干脆只丢一个 DLL 进 extensions/ 目录。
#ifdef ZYCLEAR_HAS_AI
    // 宿主能力 → 通用层。
    // 这是整个扩展体系里唯一知道「相机」二字的地方：换一个客户端时只改这一段，
    // 把 CameraToolProvider 换成本客户端的实现即可，Extension/ 整个目录原样可用。
    static CameraToolProvider cameraTools;
    const QList<IToolProvider*> providers { &cameraTools };
#else
    const QList<IToolProvider*> providers;
#endif

    PanelRegistry* registry = PanelRegistry::Instance();
    RegisterBuiltinPanels(registry, providers);
    LoadExternalExtensions(registry);
    ExtensionHost::Attach(this, registry);

    AppStyle::Polish();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    resize(1000, 600);
    setWindowTitle(tr("MainWindow"));

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralwidget");
    setCentralWidget(centralWidget);

    QHBoxLayout* centralLayout = new QHBoxLayout(centralWidget);
    centralLayout->setObjectName("horizontalLayout");
    centralLayout->setSpacing(6);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    m_pSplitter = new QSplitter(centralWidget);
    m_pSplitter->setObjectName("splitter");
    m_pSplitter->setOrientation(Qt::Horizontal);
    m_pSplitter->setHandleWidth(2);
    centralLayout->addWidget(m_pSplitter);

    m_pControlContainer = new QWidget(m_pSplitter);
    m_pControlContainer->setObjectName("ControlWidget");
    QVBoxLayout* controlLayout = new QVBoxLayout(m_pControlContainer);
    controlLayout->setObjectName("verticalLayout_2");
    controlLayout->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout* controlInnerLayout = new QVBoxLayout();
    controlInnerLayout->setObjectName("verticalLayout");
    controlLayout->addLayout(controlInnerLayout);
    m_pSplitter->addWidget(m_pControlContainer);

    m_pViewContainer = new QWidget(m_pSplitter);
    m_pViewContainer->setObjectName("ViewWidget");
    QVBoxLayout* viewLayout = new QVBoxLayout(m_pViewContainer);
    viewLayout->setObjectName("verticalLayout_5");
    viewLayout->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout* viewInnerLayout = new QVBoxLayout();
    viewInnerLayout->setObjectName("verticalLayout_3");
    viewLayout->addLayout(viewInnerLayout);
    m_pSplitter->addWidget(m_pViewContainer);

    m_pParamContainer = new QWidget(m_pSplitter);
    m_pParamContainer->setObjectName("ParamWidget");
    QVBoxLayout* paramLayout = new QVBoxLayout(m_pParamContainer);
    paramLayout->setObjectName("verticalLayout_6");
    paramLayout->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout* paramInnerLayout = new QVBoxLayout();
    paramInnerLayout->setObjectName("verticalLayout_4");
    paramLayout->addLayout(paramInnerLayout);
    m_pSplitter->addWidget(m_pParamContainer);

    QMenuBar* menuBar = new QMenuBar(this);
    menuBar->setObjectName("menubar");
    menuBar->setGeometry(QRect(0, 0, 1000, 26));
    setMenuBar(menuBar);

    QStatusBar* statusBar = new QStatusBar(this);
    statusBar->setObjectName("statusbar");
    setStatusBar(statusBar);

    // 给三栏各设一个最小宽度。
    // 这几个值要保证在高 DPI 的小逻辑屏上（本例逻辑宽度只有 836）三栏仍能并存，
    // 否则 QSplitter 会把最后一栏压成 0 宽 —— 压成 0 就再也拖不回来了。
    m_pControlContainer->setMinimumWidth(150);
    m_pViewContainer->setMinimumWidth(170);
    m_pParamContainer->setMinimumWidth(170);
}

void MainWindow::OnUpdateErrorInfo(QString strErrorInfo)
{
    m_pErrorInfoLabel->setText(strErrorInfo);
    if (!strErrorInfo.isEmpty()) {

        QMessageBox::critical(this, "Error", strErrorInfo, QMessageBox::Close);
    }
}

