#ifndef VIRTUALCAMERA_H
#define VIRTUALCAMERA_H

#include "../CameraInterface/CameraInterface.h"
#include <QObject>

class VirtualCamera
    : public CameraInterface {
public:
    // 与真机共用同一套相机契约的仿真实现：枚举恒返回一台虚拟设备，
    // 采集由本地线程造图，用于无硬件时跑通“枚举—连接—采集—取帧”全链路。
    static const QString VIRTUAL_CAMERA_NAME;
    static const QString VIRTUAL_CAMERA_SERIAL;
    static const QString VIRTUAL_CAMERA_VENDER;

    explicit VirtualCamera(const CameraMetaInfo& info);
    ~VirtualCamera();

    static uint32_t EnumCamera(QVector<CameraMetaInfo>& cameraInfos);

    uint32_t getParamList(QVector<CameraParam>& paramList) override;

    bool isConnect() override;

    bool isGrabbing() override;

    uint32_t acquire() override;

    uint32_t release() override;

    uint32_t connect() override;

    uint32_t disconnect() override;

    uint32_t creatStream() override;

    uint32_t destroyStream() override;

    uint32_t startGrabbing() override;

    uint32_t stopGrabbing() override;

    // 配置读写与流对象创建这类真机才有的重步骤，这里一律空转返回成功，
    // 让上层调用序列不必为虚拟相机开分支。
    uint32_t loadConfig(const QString path) override;

    uint32_t saveConfig(const QString path) override;

    QString configFormat() override;

    uint32_t readParam(CameraParam& param) override;

    uint32_t writeParam(CameraParam& param) override;

    uint32_t getImageLast(cv::Mat& image) override;

    // 与基类同一队列的显式出口：真机由 SDK 回调投帧，这里由内部采集线程投帧，
    // 两种来源共用同一份契约，取帧侧无需区分。
    CameraImageQueue& getImageQueue()
    {
        return m_imageQueue;
    }

private:
    // 只为满足 isConnect()/isGrabbing() 的契约而设，背后没有真实设备；
    // 采集线程读、UI 线程写，靠 bool 读写不撕裂侥幸成立，换成复合类型就要加锁。
    bool m_connect = false;
    bool m_starGrabbing = false;
};

#endif

