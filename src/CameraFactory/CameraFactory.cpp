#include "CameraFactory.h"
#include "VirtualCamera.h"
#ifdef ZYCLEAR_HAS_HIK_SDK
#include "HikCamera.h"
#endif
#include <QMutex>
#include <QMutexLocker>

// 静态存储期、零初始化；实例与头文件里的单例策略一致，同样不提供释放路径。
CameraFactory* CameraFactory::m_instance = nullptr;
// 只守护首次构造，不保护两张表的读写：注册全发生在实例化那一刻，
// 此后它们视为只读，createCamera 才敢不加锁直接查。
QMutex CameraFactory::m_mutex;

CameraFactory* CameraFactory::instance()
{
    if (!m_instance) {
        QMutexLocker locker(&m_mutex);
        if (!m_instance) {
            // 正确顺序应是先把注册表填满再对外发布指针；此处先行赋值存在可见性缺口——
            // 并发下无锁的那层判空可能看到非空指针，却拿到一张还没登记完的表。
            m_instance = new CameraFactory();
            m_instance->registerVendor<VirtualCamera>(
                VirtualCamera::VIRTUAL_CAMERA_VENDER);
            // 未接入 SDK 时不注册该品牌
#ifdef ZYCLEAR_HAS_HIK_SDK
            m_instance->registerVendor<HikCamera>(
                HikCamera::HIK_CAMERA_VENDER);
#endif
        }
    }
    return m_instance;
}

// 品牌名来自设备枚举结果，必须与注册键逐字一致；查不到只告警并回 nullptr，
// 不吞成“没相机”，是为了能把“设备自报的厂商串对不上”这种情况暴露出来。
CameraInterface* CameraFactory::createCamera(const CameraMetaInfo& info)
{
    QString venderName = info.VenderName;

    if (!m_creatorMap.contains(venderName)) {
        qWarning() << "不支持的相机厂商:" << venderName;
        return nullptr;
    }

    return m_creatorMap[venderName](info);
}

// QMap 按 key 字典序遍历，枚举顺序因此是稳定的、与注册先后无关；
// 各枚举器的返回值被丢弃，本函数永远报成功，“没找到相机”与“枚举失败”在此无法区分。
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

