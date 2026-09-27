#ifndef CAMERACONTEXT_H
#define CAMERACONTEXT_H

#include "CameraError.h"
#include <QImage>
#include <QMap>
#include <QString>
#include <QVector>
#include <map>

// 门面：上层只认序列号，由它把序列号翻译成具体厂商设备对象并托管这些对象的生命周期，
// 厂商差异对上层完全不可见
// 该宏在调用点展开，依赖调用类自身声明了 SigUpdateErrorInfo 信号：
// 它定义在这张门面头上，却只能用在 QObject 派生类（各 Widget）里，
// CameraContext 自身没有该信号也没有 Q_OBJECT，放进来会编译不过
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

    // 单例入口；Release 与之配对，负责销毁门面（内部会先断开所有相机）
    static CameraContext* Instance();

    static void Release();

    // 以下接口一律以 serial 寻址；序列号未注册时统一返回 NOCAMERA_ERROR
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

    // 当前选中的相机序列号，由门面统管
    uint32_t setCurrentSerial(const QString& serial);

    QString currentSerial() const;

private:

    CameraContext();
    ~CameraContext();

private:
    static CameraContext* m_pContext;
    // 序列号 → 设备对象，是本层唯一的持有者；无锁，约定只在 GUI 线程访问
    QMap<QString, CameraInterface*> m_serialCamMap;
    QString m_currentSerial;
};

#endif

