#include "ViewWidget.h"
#include "AcquireImageProcess.h"
#include "CameraInterface/ZCCameraMetaInfo.h"
#include "CameraInterface/ZCCameraParam.h"
#include "CameraInterface/CameraContext.h"
#include "CameraInterface/CameraError.h"
#include "ViewWidget/GraphicsView.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

ViewWidget::ViewWidget(QWidget* parent)
    : QWidget(parent)
    , Listener()
    , m_pGrabbingButton(nullptr)
    , m_pViewBoxContainer(nullptr)
    , m_pViewBox(new GraphicsView())
    , m_pImageProcess(new AcquireImageProcess())
{
    setupUi();
    m_pViewBoxContainer->layout()->addWidget(m_pViewBox);

    // 只订阅不主动查询：开流可能由控制栏、AI 侧栏或相机掉线等第三方触发，
    // 面板靠这份广播对齐开关外观与画面清空，不假设自己知道是谁动的手。
    ListenerManger::Instance()->registerMessage(MESSAGE::CAMERA_CONNECT
            | MESSAGE::CAMERA_DISCONNECT
            | MESSAGE::CAMERA_ENUMRTION
            | MESSAGE::CAMERA_STARTGRAB
            | MESSAGE::CAMERA_STOPTGRAB,
        this);
}

ViewWidget::~ViewWidget()
{
}

void ViewWidget::setupUi()
{
    resize(428, 478);
    setWindowTitle(tr("Form"));

    QVBoxLayout* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName("verticalLayout_2");
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    m_pViewBoxContainer = new QWidget(this);
    m_pViewBoxContainer->setObjectName("ViewBox_widget");
    QVBoxLayout* viewBoxLayout = new QVBoxLayout(m_pViewBoxContainer);
    viewBoxLayout->setObjectName("verticalLayout");
    viewBoxLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->addWidget(m_pViewBoxContainer);

    QFrame* controlFrame = new QFrame(this);
    controlFrame->setObjectName("ViewConrol_Frame");
    controlFrame->setFrameShape(QFrame::StyledPanel);
    controlFrame->setFrameShadow(QFrame::Raised);

    QHBoxLayout* horizontalLayout = new QHBoxLayout(controlFrame);
    horizontalLayout->setObjectName("horizontalLayout");
    horizontalLayout->setContentsMargins(0, 0, 0, 0);

    m_pGrabbingButton = new QPushButton(controlFrame);
    m_pGrabbingButton->setObjectName("Grabbing_Button");
    m_pGrabbingButton->setToolTip(QStringLiteral("开关拉流预览"));
    m_pGrabbingButton->setCheckable(true);
    horizontalLayout->addWidget(m_pGrabbingButton);

    horizontalLayout->addItem(new QSpacerItem(484, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    verticalLayout->addWidget(controlFrame);

    connect(m_pGrabbingButton, &QPushButton::toggled, this, &ViewWidget::on_Grabbing_Button_toggled);
}

void ViewWidget::RespondMessage(int message)
{
    // message 是按位广播，判定必须写成 (message & X) == X：一次 notify 常
    // 把枚举与断开合并投递，用等值比较会把位掩码当成单个枚举而全部漏掉。
    if ((message & MESSAGE::CAMERA_DISCONNECT) == MESSAGE::CAMERA_DISCONNECT) {
        // 清空是必须的：断连或重新枚举后旧画面已不对应任何设备，留着会让
        // 用户把上一台相机的残影当成当前设备的实时图像。
        m_pViewBox->Clear();
        setStarGrabbingState(false);
    }
    if ((message & MESSAGE::CAMERA_ENUMRTION) == MESSAGE::CAMERA_ENUMRTION) {
        m_pViewBox->Clear();
        setStarGrabbingState(false);
    }
    if ((message & MESSAGE::CAMERA_STARTGRAB) == MESSAGE::CAMERA_STARTGRAB) {
        setStarGrabbingState(true);
    }
    if ((message & MESSAGE::CAMERA_STOPTGRAB) == MESSAGE::CAMERA_STOPTGRAB) {
        setStarGrabbingState(false);
    }
}

void ViewWidget::on_Grabbing_Button_toggled(bool checked)
{
    Q_UNUSED(checked);
    QString serial = CameraContext::Instance()->currentSerial();
    // 按钮的最终外观由 notify→RespondMessage 回写，这里刻意不自己设状态，
    // 于是没有当前相机时按钮根本不会被点亮成"正在拉流"。
    if (serial.isNull())
        return;

    bool state { false };
    CameraContext::Instance()->isGrabbing(serial, state);
    if (state == false)
    {
        // 先接信号再启动相机：startGrabbing 之后首帧可能立刻到达，晚接会丢帧；
        // 断开则反过来先于停相机，保证停流之后不会再有帧投进视图。
        connect(m_pImageProcess, &AcquireImageProcess::sigUpdateImage, m_pViewBox, &GraphicsView::SetImage);
        CHECK_RETURN(CameraContext::Instance()->startGrabbing(serial));

        // serial 必须在 start() 之前落定：run() 一进线程就拿它去取帧，
        // 晚写会让线程先以空序列号连吃几次 NOCAMERA_ERROR。
        m_pImageProcess->setSerial(serial);
        m_pImageProcess->start();

        // m_pViewBox->SetImage(QImage(":/zhouxuan.jpg"));
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_STARTGRAB);
    } else {
        disconnect(m_pImageProcess, &AcquireImageProcess::sigUpdateImage, m_pViewBox, &GraphicsView::SetImage);

        // 先退采集线程再停相机，避免线程空等取帧超时
        m_pImageProcess->stop();
        CHECK_RETURN(CameraContext::Instance()->stopGrabbing(serial));

        ListenerManger::Instance()->notify(MESSAGE::CAMERA_STOPTGRAB);
    }
}

void ViewWidget::setStarGrabbingState(bool state)
{
    // 只切图片资源、不碰尺寸：按钮图标的大小与随面板缩放的规则统一由 qss
    // 定义，代码侧再定一份尺寸口径迟早会和主题样式打架。
    if (state == true) {
        m_pGrabbingButton->setStyleSheet("QPushButton{image:url(:/StopGrab.png);}");
    } else {
        m_pGrabbingButton->setStyleSheet("QPushButton{image:url(:/StratGrab.png);}");
    }
}

