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
    if ((message & MESSAGE::CAMERA_DISCONNECT) == MESSAGE::CAMERA_DISCONNECT) {
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
    if (serial.isNull())
        return;

    bool state { false };
    CameraContext::Instance()->isGrabbing(serial, state);
    if (state == false)
    {
        connect(m_pImageProcess, &AcquireImageProcess::sigUpdateImage, m_pViewBox, &GraphicsView::SetImage);
        CHECK_RETURN(CameraContext::Instance()->startGrabbing(serial));

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
    if (state == true) {
        m_pGrabbingButton->setStyleSheet("QPushButton{image:url(:/StopGrab.png);}");
    } else {
        m_pGrabbingButton->setStyleSheet("QPushButton{image:url(:/StratGrab.png);}");
    }
}

