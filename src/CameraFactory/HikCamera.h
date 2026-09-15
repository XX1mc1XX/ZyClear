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

    void* CameraHandle()
    {
        return m_cameraHandle;
    }

private:

    uint32_t getFeatureAccessMode(CameraParam& param);

private:
    void* m_cameraHandle = NULL;
    MV_CC_DEVICE_INFO* m_pDeviceInfo = NULL;

    bool isStartGrabbing = false;
};

#endif

