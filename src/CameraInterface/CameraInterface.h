#ifndef CAMERAINTERFACE_H
#define CAMERAINTERFACE_H

#include "ZCCameraMetaInfo.h"
#include "ZCCameraParam.h"
#include "CameraError.h"
#include "CameraImageQueue.h"
#include "opencv2/core.hpp"
#include <QtPlugin>

class CameraInterface {
public:
    CameraInterface(const CameraMetaInfo& info)
    {
        m_cameraInfo = info;
    }
    virtual ~CameraInterface() { }

    virtual QString UserName()
    {
        return m_cameraInfo.UserDefineID;
    }

    virtual QString Serial()
    {
        return m_cameraInfo.Serial;
    }

    virtual uint32_t getParamList(QVector<CameraParam>& paramList) = 0;

    virtual bool isConnect() = 0;

    virtual bool isGrabbing() = 0;

    virtual uint32_t acquire() = 0;

    virtual uint32_t release() = 0;

    virtual uint32_t connect() = 0;

    virtual uint32_t disconnect() = 0;

    virtual uint32_t creatStream() = 0;

    virtual uint32_t destroyStream() = 0;

    virtual uint32_t startGrabbing() = 0;

    virtual uint32_t stopGrabbing() = 0;

    virtual uint32_t loadConfig(const QString path) = 0;

    virtual uint32_t saveConfig(const QString path) = 0;

    virtual QString configFormat() = 0;

    virtual uint32_t readParam(CameraParam& param) = 0;

    virtual uint32_t writeParam(CameraParam& param) = 0;

    virtual uint32_t getImageLast(cv::Mat& image) = 0;

    virtual CameraImageQueue& ImageQueue()
    {
        return m_imageQueue;
    }

protected:
    CameraImageQueue m_imageQueue;
    QVector<CameraParam> m_cameraParams;
    CameraMetaInfo m_cameraInfo;
};

#endif

