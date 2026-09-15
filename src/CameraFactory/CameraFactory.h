#ifndef CAMERAFACTORY_H
#define CAMERAFACTORY_H

#include "../CameraInterface/CameraInterface.h"
#include <QMap>
#include <QMutex>
#include <QString>
#include <QVector>

class CameraFactory {
public:

    static CameraFactory* instance();

    template <typename T>
    void registerCamera(const QString& venderName)
    {
        m_creatorMap[venderName] = [](const CameraMetaInfo& info) -> CameraInterface* {
            return new T(info);
        };
    }

    CameraInterface* createCamera(const CameraMetaInfo& info);

    QStringList getSupportedVenders() const;

    bool isVenderSupported(const QString& venderName) const;

private:
    CameraFactory() = default;
    ~CameraFactory() = default;

    CameraFactory(const CameraFactory&) = delete;
    CameraFactory& operator=(const CameraFactory&) = delete;

private:
    using CameraCreator = std::function<CameraInterface*(const CameraMetaInfo&)>;
    QMap<QString, CameraCreator> m_creatorMap;
    static CameraFactory* m_instance;
    static QMutex m_mutex;
};

#endif

