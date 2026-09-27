#include "ControlWidget.h"
#include "CameraInterface/ZCCameraMetaInfo.h"
#include "CameraInterface/CameraContext.h"
#include "LoadingDialog/LoadingDialog.h"
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <algorithm>
#include <iostream>

ControlWidget::ControlWidget(QWidget* parent)
    : QWidget(parent)
    , Listener()
    , m_pControlFrame(nullptr)
    , m_pEnumerationButton(nullptr)
    , m_pConnectButton(nullptr)
    , m_pSaveConfigButton(nullptr)
    , m_pLoadConfigButton(nullptr)
    , m_pCameraListWidget(nullptr)
    , m_lastCameraIndex(-1)
{
    setupUi();

    // 订阅在构造时一次性交出去，之后不再变更。
    // 这里按位或订了 6 个事件，但总线只认得其中 5 个（CAMERA_CAMERASWICH
    // 在 Listener::registerMessage 里没有分支），实际收到的是 5 类。
    ListenerManger::Instance()->registerMessage(MESSAGE::CAMERA_CONNECT
            | MESSAGE::CAMERA_DISCONNECT
            | MESSAGE::CAMERA_ENUMRTION
            | MESSAGE::CAMERA_STARTGRAB
            | MESSAGE::CAMERA_STOPTGRAB
            | MESSAGE::CAMERA_CAMERASWICH,
        this);
}

void ControlWidget::setupUi()
{
    resize(254, 493);
    setWindowTitle(tr("Form"));

    QVBoxLayout* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName("verticalLayout");
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    m_pControlFrame = new QFrame(this);
    m_pControlFrame->setObjectName("Control_Frame");
    m_pControlFrame->setFrameShape(QFrame::StyledPanel);
    m_pControlFrame->setFrameShadow(QFrame::Raised);

    QHBoxLayout* horizontalLayout = new QHBoxLayout(m_pControlFrame);
    horizontalLayout->setObjectName("horizontalLayout");
    horizontalLayout->setContentsMargins(5, 5, 5, 5);

    m_pEnumerationButton = new QPushButton(m_pControlFrame);
    m_pEnumerationButton->setObjectName("Enumeration_Button");
    m_pEnumerationButton->setToolTip(QStringLiteral("枚举相机操作"));
    horizontalLayout->addWidget(m_pEnumerationButton);

    horizontalLayout->addItem(new QSpacerItem(33, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_pConnectButton = new QPushButton(m_pControlFrame);
    m_pConnectButton->setObjectName("Connect_Button");
    m_pConnectButton->setToolTip(QStringLiteral("连接/断连相机"));
    m_pConnectButton->setCheckable(true);
    horizontalLayout->addWidget(m_pConnectButton);

    m_pSaveConfigButton = new QPushButton(m_pControlFrame);
    m_pSaveConfigButton->setObjectName("SaveConfig_Button");
    m_pSaveConfigButton->setToolTip(QStringLiteral("保存配置文件"));
    horizontalLayout->addWidget(m_pSaveConfigButton);

    m_pLoadConfigButton = new QPushButton(m_pControlFrame);
    m_pLoadConfigButton->setObjectName("LoadConfig_Button");
    m_pLoadConfigButton->setToolTip(QStringLiteral("加载配置文件"));
    horizontalLayout->addWidget(m_pLoadConfigButton);

    verticalLayout->addWidget(m_pControlFrame);

    m_pCameraListWidget = new QListWidget(this);
    m_pCameraListWidget->setObjectName("Camera_listWidget");
    m_pCameraListWidget->setAlternatingRowColors(true);
    verticalLayout->addWidget(m_pCameraListWidget);

    connect(m_pEnumerationButton, &QPushButton::clicked, this, &ControlWidget::on_Enumeration_Button_clicked);
    connect(m_pConnectButton, &QPushButton::toggled, this, &ControlWidget::on_Connect_Button_toggled);
    connect(m_pSaveConfigButton, &QPushButton::clicked, this, &ControlWidget::on_SaveConfig_Button_clicked);
    connect(m_pLoadConfigButton, &QPushButton::clicked, this, &ControlWidget::on_LoadConfig_Button_clicked);
    connect(m_pCameraListWidget, &QListWidget::currentRowChanged, this, &ControlWidget::on_Camera_listWidget_currentRowChanged);
}

ControlWidget::~ControlWidget()
{
    // 总线没有反注册接口，本对象的指针会一直留在订阅表里。
    // 之所以敢不退订：ControlWidget 与主窗口同生命周期，析构意味着程序要退了，
    // 退订之后不会再有任何 notify。若将来把它改成可关闭面板，
    // 必须先给总线补上退订能力，否则就是对着野指针回调。
    CameraContext::Release();
}

void ControlWidget::on_Enumeration_Button_clicked()
{
    // 枚举是同步阻塞的 SDK 调用，可能几百毫秒。先把加载框画出来并
    // processEvents 一次，否则主窗口整块假死、连重绘都没有。
    // 与之配对的 HideLoading 在本函数所有出口上都必须执行。
    LoadingDialog::Loading();

    QVector<CameraMetaInfo> cameras;
    CameraContext::Instance()->EnumerationCamera(cameras);

    // 枚举会重建相机注册表，旧的选中序列号随之失效；
    // 清在前，列表重建若自动选中首项会重新写入
    CameraContext::Instance()->setCurrentSerial(QString());

    m_pCameraListWidget->clear();
    m_cameraMetaInfos.clear();
    m_cameraMetaInfos = cameras;
    for (CameraMetaInfo cameraInfo : m_cameraMetaInfos) {
        QString name = QString(cameraInfo.UserDefineID.data());
        QString serial = QString(cameraInfo.Serial.data());

        m_pCameraListWidget->addItem(name + "(" + serial + ")");
    }

    // 广播必须放在列表重建之后：订阅方（本类、ParamWidget、ViewWidget）
    // 都以「列表已就绪」为前提，提前发会读到空列表。
    // 本类自己也订阅了枚举事件，所以这一行会同步回调进自己的 RespondMessage。
    ListenerManger::Instance()->notify(MESSAGE::CAMERA_ENUMRTION);
    LoadingDialog::HideLoading();
}

void ControlWidget::on_SaveConfig_Button_clicked()
{
    QString serial = GetCurrentCameraInfo().Serial;
    QString format = CameraContext::Instance()->getConfigFormat(serial);
    QString filter = tr("config files(*.%1)").arg(format);

    QString filePath = QFileDialog::getSaveFileName(this, "Save Config",
        "", filter);
    if (filePath.isEmpty())
        return;

    // CHECK_RETURN 展开成「错误码非成功就 emit SigUpdateErrorInfo 再 return」。
    // 宏里引用的信号名是本类自己声明的，所以它只能出现在定义了同名信号的类中 ——
    // 换个部件用这个宏，得先有那个信号，否则编译不过。
    CHECK_RETURN(CameraContext::Instance()->saveConfig(serial, filePath));
}

void ControlWidget::on_LoadConfig_Button_clicked()
{
    QString serial = GetCurrentCameraInfo().Serial;
    QString format = CameraContext::Instance()->getConfigFormat(serial);
    QString filter = tr("config files(*.%1)").arg(format);

    QString filePath = QFileDialog::getOpenFileName(this, "Load Config",
        "", filter);
    if (filePath.isEmpty())
        return;

    CHECK_RETURN(CameraContext::Instance()->loadConfig(serial, filePath));
}

void ControlWidget::on_Camera_listWidget_currentRowChanged(int currentRow)
{
    // 列表 clear() 时 Qt 会先发一次 currentRowChanged(-1)，那不是用户的选择。
    // 直接忽略，否则清列表的动作会被当成「切到空相机」而去断连正在用的设备。
    // 副作用：重新枚举时清空列表同样走这一支，于是旧连接被保留下来、
    // 只是再没有选中的序列号指向它，要靠下一次显式连接或程序退出来收尾。
    if (currentRow == -1)
        return;

    // 切换 = 先释放上一台。不看它当前是否真的连着就无脑 stop/disconnect，
    // 依赖驱动对「未连接时断连」返回成功（当前实现如此），省掉一次状态查询。
    // 上一台不存在时 GetCameraInfo(-1) 给出 null Serial，同理由驱动吞掉。
    CameraMetaInfo lastCameraInfo = GetCameraInfo(m_lastCameraIndex);
    CameraContext::Instance()->stopGrabbing(lastCameraInfo.Serial);
    CameraContext::Instance()->disconnect(lastCameraInfo.Serial);
    ListenerManger::Instance()->notify(MESSAGE::CAMERA_DISCONNECT);
    // 先取上一台的元信息、再更新游标，顺序颠倒会把上一台认成新选中的那台。
    m_lastCameraIndex = currentRow;

    CameraMetaInfo currentCameraInfo = GetCurrentCameraInfo();

    // 选中项写回门面，供各面板寻址
    CameraContext::Instance()->setCurrentSerial(currentCameraInfo.Serial);

    bool connectState;
    // isConnect 的返回值是错误码、不是连接状态，状态经出参回填 ——
    // 这里有意忽略返回码：查不到就按未连接处理，把按钮复位总比留成错误状态强。
    CameraContext::Instance()->isConnect(currentCameraInfo.Serial, connectState);
    if (connectState == true) {
        // 图标直接写在控件的 setStyleSheet 上，而不是加一个 checked 状态选择器：
        // 全工程的 qss 走资源文件统一加载，只有这个按钮的开关图标放在代码里，
        // 换肤时这里是唯一需要同步改的一处。
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/DisConnect.png);}");
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_CONNECT);
    } else {
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/Connect.png);}");
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_DISCONNECT);
    }
}

void ControlWidget::on_Connect_Button_toggled(bool checked)
{
    // checked 用不上：真实状态以 CameraContext 的查询结果为准。
    // 按钮是可 check 的，外观由 manual setStyleSheet + qss 决定，
    // 回读 checked 只会在外部已改过状态时拿到过期值。
    Q_UNUSED(checked)

    CameraMetaInfo currentCameraInfo = GetCurrentCameraInfo();
    QString serial = currentCameraInfo.Serial;
    // 没选相机就当按下无效，顺手把按钮复位。
    // 判据用 isNull 而不是 isEmpty：默认构造的 CameraMetaInfo 里 Serial 是
    // null QString，「取到了占位元信息」正对应 isNull；换成 isEmpty 会把这个
    // 判据和「真有一台相机、只是序列号为空串」混在一起。
    if (serial.isNull())
    {
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/Connect.png);}");
        return;
    }

    bool connectState;
    // 返回值是错误码、状态走引用出参，这里有意忽略错误码：
    // 查询失败与「未连接」在这里归为同一处理，随后的连接尝试才是权威判定。
    CameraContext::Instance()->isConnect(serial, connectState);
    if (connectState == true) {
        // 停流必须在断连之前，顺序不能反：断连会使采集句柄失效，
        // 之后再调 stopGrabbing 就是对着失效句柄操作。
        CHECK_RETURN(CameraContext::Instance()->stopGrabbing(serial));
        CHECK_RETURN(CameraContext::Instance()->disconnect(serial));
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/Connect.png);}");
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_DISCONNECT);
    } else {
        CHECK_RETURN(CameraContext::Instance()->connect(serial));
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/DisConnect.png);}");
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_CONNECT);
    }
}

CameraMetaInfo ControlWidget::GetCurrentCameraInfo()
{
    int index = m_pCameraListWidget->currentRow();
    if (index == -1 || index >= m_cameraMetaInfos.size()) {
        return CameraMetaInfo();
    } else {
        return m_cameraMetaInfos.at(index);
    }
}

CameraMetaInfo ControlWidget::GetCameraInfo(int index)
{
    // 与 GetCurrentCameraInfo 的规则一致，只差索引来源：越界一律给出默认
    // 元信息，因此 null Serial 就是「这个索引上没有相机」的通用判据。
    if (index == -1 || index >= m_cameraMetaInfos.size()) {
        return CameraMetaInfo();
    } else {
        return m_cameraMetaInfos.at(index);
    }
}

void ControlWidget::RespondMessage(int message)
{
    // 订了 5 类事件却只处理这一种：其余四类由各面板自行消费，本类不需要反应。
    //
    // 收到枚举完成 = 相机注册表刚被重建，按钮图标可能还停在上一轮的连接态上，
    // 这里重推一遍让它与设备实际状态对齐。
    // 之所以直接调槽而不 setChecked：setChecked 只在状态真的翻转时才发 toggled，
    // 按钮若已处于该状态就什么都不会发生，走不到那段「按真实状态纠正」的逻辑。
    if ((message & MESSAGE::CAMERA_ENUMRTION) == MESSAGE::CAMERA_ENUMRTION) {
        on_Connect_Button_toggled(true);
    }
}

