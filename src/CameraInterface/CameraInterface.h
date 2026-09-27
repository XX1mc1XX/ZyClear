#ifndef CAMERAINTERFACE_H
#define CAMERAINTERFACE_H

#include "ZCCameraMetaInfo.h"
#include "ZCCameraParam.h"
#include "CameraError.h"
#include "CameraImageQueue.h"
#include "opencv2/core.hpp"
#include <QtPlugin>

// 厂商无关的相机契约：上层与门面只依赖这里，一切厂商差异都被关进实现层。
// 约定：返回 uint32_t 的方法 0（ZYCLEAR_OK）为成功、非 0 为错误码，错误走返回码而非异常
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

    // 只返回参数骨架（名字/分组/类型），值尚未填充；
    // 调用方需对每个元素再调 readParam，才算拿到可用参数
    virtual uint32_t getParamList(QVector<CameraParam>& paramList) = 0;

    virtual bool isConnect() = 0;

    virtual bool isGrabbing() = 0;

    // 顺序契约：acquire（建句柄，不占用设备）→ connect；断开时 disconnect → release，二者成对
    virtual uint32_t acquire() = 0;

    virtual uint32_t release() = 0;

    // 打开设备并挂上取帧回调；disconnect 注销回调、关闭设备，但保留句柄（句柄留给 release）
    virtual uint32_t connect() = 0;

    virtual uint32_t disconnect() = 0;

    // 配置流参数（缓冲深度、取帧策略等），不是采集开关；
    // creatStream 须在 startGrabbing 前、destroyStream 在 stopGrabbing 后调用
    virtual uint32_t creatStream() = 0;

    virtual uint32_t destroyStream() = 0;

    virtual uint32_t startGrabbing() = 0;

    virtual uint32_t stopGrabbing() = 0;

    virtual uint32_t loadConfig(const QString path) = 0;

    virtual uint32_t saveConfig(const QString path) = 0;

    virtual QString configFormat() = 0;

    // param 既是入参又是出参：靠 name/type 定位，按 type 解包回填或下发，type 须与相机参数一致
    virtual uint32_t readParam(CameraParam& param) = 0;

    virtual uint32_t writeParam(CameraParam& param) = 0;

    // 取最近一帧；队列无帧时返回超时码（GETIAMGE_TIMEOUT），不产出空 Mat。
    // 取出的 Mat 缓冲归调用方，用完须经 ImageQueue().Recycle 归还空闲池
    virtual uint32_t getImageLast(cv::Mat& image) = 0;

    // 供取帧回调线程 Put、上层 Take/Recycle；内部自带锁，可跨线程访问
    virtual CameraImageQueue& ImageQueue()
    {
        return m_imageQueue;
    }

protected:
    CameraImageQueue m_imageQueue;
    // 预留的接口层参数缓存，当前各厂商实现各自维护静态参数表，此处未被使用
    QVector<CameraParam> m_cameraParams;
    CameraMetaInfo m_cameraInfo;
};

#endif

