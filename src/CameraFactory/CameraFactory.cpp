#include "CameraFactory.h"
#include "HikCamera.h"
#include "VirtualCamera.h"
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
            CameraFactory::instance()->registerCamera<HikCamera>(
                HikCamera::VIRTUAL_CAMERA_VENDER);
            CameraFactory::instance()->registerCamera<VirtualCamera>(
                VirtualCamera::VIRTUAL_CAMERA_VENDER);
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

QStringList CameraFactory::getSupportedVenders() const
{
    return m_creatorMap.keys();
}

bool CameraFactory::isVenderSupported(const QString& venderName) const
{
    return m_creatorMap.contains(venderName);
}

