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
    CameraContext::Release();
}

void ControlWidget::on_Enumeration_Button_clicked()
{
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
    if (currentRow == -1)
        return;

    CameraMetaInfo lastCameraInfo = GetCameraInfo(m_lastCameraIndex);
    CameraContext::Instance()->stopGrabbing(lastCameraInfo.Serial);
    CameraContext::Instance()->disconnect(lastCameraInfo.Serial);
    ListenerManger::Instance()->notify(MESSAGE::CAMERA_DISCONNECT);
    m_lastCameraIndex = currentRow;

    CameraMetaInfo currentCameraInfo = GetCurrentCameraInfo();

    // 选中项写回门面，供各面板寻址
    CameraContext::Instance()->setCurrentSerial(currentCameraInfo.Serial);

    bool connectState;
    CameraContext::Instance()->isConnect(currentCameraInfo.Serial, connectState);
    if (connectState == true) {
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/DisConnect.png);}");
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_CONNECT);
    } else {
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/Connect.png);}");
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_DISCONNECT);
    }
}

void ControlWidget::on_Connect_Button_toggled(bool checked)
{
    Q_UNUSED(checked)

    CameraMetaInfo currentCameraInfo = GetCurrentCameraInfo();
    QString serial = currentCameraInfo.Serial;
    if (serial.isNull())
    {
        m_pConnectButton->setStyleSheet("QPushButton{image:url(:/Connect.png);}");
        return;
    }

    bool connectState;
    CameraContext::Instance()->isConnect(serial, connectState);
    if (connectState == true) {
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
    if (index == -1 || index >= m_cameraMetaInfos.size()) {
        return CameraMetaInfo();
    } else {
        return m_cameraMetaInfos.at(index);
    }
}

void ControlWidget::RespondMessage(int message)
{
    if ((message & MESSAGE::CAMERA_ENUMRTION) == MESSAGE::CAMERA_ENUMRTION) {
        on_Connect_Button_toggled(true);
    }
}

