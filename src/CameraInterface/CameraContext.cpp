#include "CameraContext.h"
#include "../CameraFactory/CameraFactory.h"
#include "../CameraFactory/HikCamera.h"
#include "../CameraFactory/VirtualCamera.h"
#include "../Utils/ImageConver.h"
#include "ZCCameraMetaInfo.h"
#include "ZCCameraParam.h"
#include "CameraImageQueue.h"
#include "CameraInterface.h"
#include <QApplication>
#include <QDebug>
#include <QDir>

CameraContext* CameraContext::m_pContext = Q_NULLPTR;

CameraContext* CameraContext::Instance()
{
    if (Q_NULLPTR == m_pContext) {
        m_pContext = new CameraContext();
    }
    return m_pContext;
}

void CameraContext::Release()
{
    if (Q_NULLPTR != m_pContext) {
        delete m_pContext;
        m_pContext = Q_NULLPTR;
    }
}

CameraContext::CameraContext()
{

}

CameraContext::~CameraContext()
{

    QMap<QString, CameraInterface*>::iterator iter;
    for (iter = m_serialCamMap.begin(); iter != m_serialCamMap.end(); ++iter) {
        iter.value()->stopGrabbing();
        iter.value()->disconnect();
    }
    m_serialCamMap.clear();
}

uint32_t CameraContext::EnumerationCamera(QVector<CameraMetaInfo>& cameraInfos)
{

    QMap<QString, CameraInterface*>::iterator iter;
    for (iter = m_serialCamMap.begin(); iter != m_serialCamMap.end(); ++iter) {
        iter.value()->stopGrabbing();
        iter.value()->disconnect();
    }
    m_serialCamMap.clear();

    QVector<CameraMetaInfo> infos;
    VirtualCamera::EnumCamera(infos);
    HikCamera::EnumCamera(infos);

    for (auto info : infos) {
        QVector<CameraMetaInfo>::iterator it = std::find(cameraInfos.begin(), cameraInfos.end(), info);

        if (it == cameraInfos.end()) {
            cameraInfos.push_back(info);
            QString serial = info.Serial;

            CameraInterface* camera = CameraFactory::instance()->createCamera(info);
            if (camera) {
                m_serialCamMap[info.Serial] = camera;
                qDebug() << "创建相机成功:" << info.VenderName << info.Serial;
            } else {
                qWarning() << "创建相机失败，不支持的厂商:" << info.VenderName;
            }
        }
    }
    return ZYCLEAR_OK;
}

uint32_t CameraContext::getParamList(const QString serial, QVector<CameraParam>& paramList)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    QVector<CameraParam> paramListTemp;
    auto camera = m_serialCamMap[serial];
    camera->getParamList(paramListTemp);

    for (auto var : paramListTemp) {
        camera->readParam(var);
        paramList.push_back(var);
    }

    return ZYCLEAR_OK;
}

uint32_t CameraContext::isConnect(const QString serial, bool& state)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];
    state = camera->isConnect();

    return ZYCLEAR_OK;
}

uint32_t CameraContext::isGrabbing(const QString serial, bool& state)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];
    state = camera->isGrabbing();

    return ZYCLEAR_OK;
}

uint32_t CameraContext::connect(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    auto ret = camera->acquire();
    if (ret != ZYCLEAR_OK)
        return ret;

    ret = camera->connect();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::disconnect(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    auto ret = camera->disconnect();
    if (ret != ZYCLEAR_OK)
        return ret;

    ret = camera->release();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::startGrabbing(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() != true)
        return CAMERA_NOT_CONNECTED;

    if (camera->isGrabbing() == true)
        return ZYCLEAR_OK;

    auto ret = camera->creatStream();
    if (ret != ZYCLEAR_OK)
        return ret;

    camera->startGrabbing();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::stopGrabbing(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() == false || camera->isGrabbing() == false)
        return ZYCLEAR_OK;

    auto ret = camera->stopGrabbing();
    if (ret != ZYCLEAR_OK)
        return ret;

    camera->destroyStream();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::loadConfig(const QString serial, const QString path)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() == false || camera->isGrabbing() == true)
        return CAMERA_NOT_CONNECTED;

    auto ret = camera->loadConfig(path);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::saveConfig(const QString serial, const QString path)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() == false || camera->isGrabbing() == true)
        return CAMERA_NOT_CONNECTED;

    auto ret = camera->saveConfig(path);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

QString CameraContext::getConfigFormat(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return "";

    CameraInterface* camera = m_serialCamMap[serial];
    QString format = camera->configFormat();

    return QString(format.data());
}

uint32_t CameraContext::readParam(const QString serial, CameraParam& param)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    CameraInterface* camera = m_serialCamMap[serial];
    auto ret = camera->readParam(param);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::writeParam(const QString serial, CameraParam& param)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    CameraInterface* camera = m_serialCamMap[serial];
    auto ret = camera->writeParam(param);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::getImageLast(const QString serial, QImage& image)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    CameraInterface* camera = m_serialCamMap[serial];
    cv::Mat cvImage;
    auto ret = camera->getImageLast(cvImage);
    if (ret != ZYCLEAR_OK)
        return GETIAMGE_TIMEOUT;

    image = ImageConver::cvMat2QImage(cvImage);

    return ZYCLEAR_OK;
}

