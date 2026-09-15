#ifndef CAMERACONTEXT_H
#define CAMERACONTEXT_H

#include "CameraError.h"
#include <QImage>
#include <QMap>
#include <QString>
#include <QVector>
#include <map>

#define CHECK_RETURN(Ret)                    \
    if (Ret != ZYCLEAR_OK) {               \
        QString error = getErrorInfoEn(Ret); \
        emit SigUpdateErrorInfo(error);      \
        return;                              \
    }

struct CameraMetaInfo;
class CameraParam;
class CameraInterface;
class CameraImageQueue;
class CameraContext {
public:

    static CameraContext* Instance();

    static void Release();

    uint32_t EnumerationCamera(QVector<CameraMetaInfo>& cameraInfos);

    uint32_t getParamList(const QString serial, QVector<CameraParam>& paramList);

    uint32_t isConnect(const QString serial, bool& state);

    uint32_t isGrabbing(const QString serial, bool& state);

    uint32_t connect(const QString serial);

    uint32_t disconnect(const QString serial);

    uint32_t startGrabbing(const QString serial);

    uint32_t stopGrabbing(const QString serial);

    uint32_t loadConfig(const QString serial, const QString path);

    uint32_t saveConfig(const QString serial, const QString path);

    QString getConfigFormat(const QString serial);

    uint32_t readParam(const QString serial, CameraParam& param);

    uint32_t writeParam(const QString serial, CameraParam& param);

    uint32_t getImageLast(const QString serial, QImage& qImage);

private:

    CameraContext();
    ~CameraContext();

private:
    static CameraContext* m_pContext;
    QMap<QString, CameraInterface*> m_serialCamMap;
};

#endif

