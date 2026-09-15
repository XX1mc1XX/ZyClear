#include "CameraFactory.h"
#include "VirtualCamera.h"
#ifdef ZYCLEAR_HAS_HIK_SDK
#include "HikCamera.h"
#endif
#include <QMutex>
#include <QMutexLocker>

CameraFactory* CameraFactory::m_instance = nullptr;
QMutex CameraFactory::m_mutex;

CameraFactory* CameraFactory::instance()
{
    if (!m_instance) {
        QMutexLocker locker(&m_mutex);
        if (!m_instance) {
            m_instance = new CameraFactory();
            m_instance->registerVendor<VirtualCamera>(
                VirtualCamera::VIRTUAL_CAMERA_VENDER);
            // 未接入海康 SDK 时不注册该品牌：枚举不到、也创建不了，
            // 程序以纯虚拟相机形态照常运行
#ifdef ZYCLEAR_HAS_HIK_SDK
            m_instance->registerVendor<HikCamera>(
                HikCamera::HIK_CAMERA_VENDER);
#endif
        }
    }
    return m_instance;
}

CameraInterface* CameraFactory::createCamera(const CameraMetaInfo& info)
{
    QString venderName = info.VenderName;

    if (!m_creatorMap.contains(venderName)) {
        qWarning() << "不支持的相机厂商:" << venderName;
        return nullptr;
    }

    return m_creatorMap[venderName](info);
}

uint32_t CameraFactory::enumCameras(QVector<CameraMetaInfo>& cameraInfos) const
{
    for (auto it = m_enumeratorMap.begin(); it != m_enumeratorMap.end(); ++it) {
        it.value()(cameraInfos);
    }

    return ZYCLEAR_OK;
}

QStringList CameraFactory::getSupportedVenders() const
{
    return m_creatorMap.keys();
}

bool CameraFactory::isVenderSupported(const QString& venderName) const
{
    return m_creatorMap.contains(venderName);
}

