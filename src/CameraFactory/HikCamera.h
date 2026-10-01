#ifndef HIKCAMERA_H
#define HIKCAMERA_H

#include "../CameraInterface/CameraInterface.h"
#include "MvCameraControl.h"

class HikCamera
    : public CameraInterface {
public:
    static const QString HIK_CAMERA_VENDER;
    HikCamera(const CameraMetaInfo& info);
    ~HikCamera();

    // 静态枚举：在产出设备列表前先设一次全局 SDK 日志目录，带进程级副作用。
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

    uint32_t loadConfig(const QString path) override;

    uint32_t saveConfig(const QString path) override;

    QString configFormat() override;

    uint32_t readParam(CameraParam& param) override;

    uint32_t writeParam(CameraParam& param) override;

    uint32_t getImageLast(cv::Mat& image) override;

    // 裸句柄出口，只给 SDK 回调这类必须回传句柄的内部路径用，
    // 业务层不要拿它绕过本类封装直接下发 SDK 调用。
    void* CameraHandle()
    {
        return m_cameraHandle;
    }

private:

    // 依据 GenICam 节点访问模式回填 param 的 valid/readable/writeable，
    // 使 readParam/writeParam 能对无权限节点静默跳过。
    uint32_t getFeatureAccessMode(CameraParam& param);

private:
    // 由 acquire() 里的 MV_CC_CreateHandle 建、release() 里的 MV_CC_DestroyHandle 销，
    // 生命周期短于本对象；析构不做兜底释放，上层漏调 release() 就会漏句柄。
    void* m_cameraHandle = NULL;

    // 设备信息的自有副本。acquire() 从 SDK 的枚举列表里值拷贝进来，m_pDeviceInfo
    // 指向它 —— 枚举列表的内存归 SDK 管且会被下一次枚举覆写，直接引用原节点，
    // 多机依次连接时早先那台的指针就会悬垂，而 connect() 还要拿它判独占。
    MV_CC_DEVICE_INFO m_deviceInfo {};
    MV_CC_DEVICE_INFO* m_pDeviceInfo = NULL;

    // 本地采集状态镜像，只为 isGrabbing() 省一次 SDK 往返；
    // 真值仍在 SDK 侧，掉线时它会与 MV_CC_IsDeviceConnected 短暂不一致。
    bool isStartGrabbing = false;
};

#endif

