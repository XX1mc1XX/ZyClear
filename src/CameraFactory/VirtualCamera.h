#ifndef VIRTUALCAMERA_H
#define VIRTUALCAMERA_H

#include "../CameraInterface/CameraInterface.h"
#include <QObject>

class VirtualCamera
    : public CameraInterface {
public:
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

    uint32_t loadConfig(const QString path) override;

    uint32_t saveConfig(const QString path) override;

    QString configFormat() override;

    uint32_t readParam(CameraParam& param) override;

    uint32_t writeParam(CameraParam& param) override;

    uint32_t getImageLast(cv::Mat& image) override;

    CameraImageQueue& getImageQueue()
    {
        return m_imageQueue;
    }

private:
    bool m_connect = false;
    bool m_starGrabbing = false;
};

#endif

