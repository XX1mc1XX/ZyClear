#ifndef CAMERAFACTORY_H
#define CAMERAFACTORY_H

#include "../CameraInterface/CameraInterface.h"
#include <QMap>
#include <QMutex>
#include <QString>
#include <QVector>
#include <functional>

class CameraFactory {
public:

    static CameraFactory* instance();

    // 登记一个品牌的创建器与枚举器
    template <typename T>
    void registerVendor(const QString& venderName)
    {
        m_creatorMap[venderName] = [](const CameraMetaInfo& info) -> CameraInterface* {
            return new T(info);
        };
        m_enumeratorMap[venderName] = [](QVector<CameraMetaInfo>& cameraInfos) -> uint32_t {
            return T::EnumCamera(cameraInfos);
        };
    }

    CameraInterface* createCamera(const CameraMetaInfo& info);

    // 依次调用各品牌的枚举器，结果追加到 cameraInfos
    uint32_t enumCameras(QVector<CameraMetaInfo>& cameraInfos) const;

    QStringList getSupportedVenders() const;

    bool isVenderSupported(const QString& venderName) const;

private:
    CameraFactory() = default;
    ~CameraFactory() = default;

    CameraFactory(const CameraFactory&) = delete;
    CameraFactory& operator=(const CameraFactory&) = delete;

private:
    using CameraCreator = std::function<CameraInterface*(const CameraMetaInfo&)>;
    using CameraEnumerator = std::function<uint32_t(QVector<CameraMetaInfo>&)>;
    QMap<QString, CameraCreator> m_creatorMap;
    QMap<QString, CameraEnumerator> m_enumeratorMap;
    static CameraFactory* m_instance;
    static QMutex m_mutex;
};

#endif

