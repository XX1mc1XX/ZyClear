#include "mainwindow.h"
#include "AppStyle/AppStyle.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

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
    this->resize(1000, 600);

    connect(m_pControlWidget, &ControlWidget::SigUpdateErrorInfo, this, &MainWindow::OnUpdateErrorInfo);
    connect(m_pParamWidget, &ParamWidget::SigUpdateErrorInfo, this, &MainWindow::OnUpdateErrorInfo);
    connect(m_pViewWidget, &ViewWidget::SigUpdateErrorInfo, this, &MainWindow::OnUpdateErrorInfo);

    setWindowTitle(QStringLiteral("智澈-机器视觉上位机软件(海康工业相机SDK二次开发且兼容所有别的品牌相机)-https://github.com/XX1mc1XX/"));
    setWindowIcon(QIcon(":/favicon.ico"));

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
}

void MainWindow::OnUpdateErrorInfo(QString strErrorInfo)
{
    m_pErrorInfoLabel->setText(strErrorInfo);
    if (!strErrorInfo.isEmpty()) {

        QMessageBox::critical(this, "Error", strErrorInfo, QMessageBox::Close);
    }
}

