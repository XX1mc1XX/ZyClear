#include "HikCamera.h"
#include "opencv2/opencv.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

const QString HikCamera::HIK_CAMERA_VENDER = "Hikrobot";

bool IsColor(MvGvspPixelType enType)
{
    switch (enType) {
    case PixelType_Gvsp_BGR8_Packed:
    case PixelType_Gvsp_YUV422_Packed:
    case PixelType_Gvsp_YUV422_YUYV_Packed:
    case PixelType_Gvsp_BayerGR8:
    case PixelType_Gvsp_BayerRG8:
    case PixelType_Gvsp_BayerGB8:
    case PixelType_Gvsp_BayerBG8:
    case PixelType_Gvsp_BayerGB10:
    case PixelType_Gvsp_BayerGB10_Packed:
    case PixelType_Gvsp_BayerBG10:
    case PixelType_Gvsp_BayerBG10_Packed:
    case PixelType_Gvsp_BayerRG10:
    case PixelType_Gvsp_BayerRG10_Packed:
    case PixelType_Gvsp_BayerGR10:
    case PixelType_Gvsp_BayerGR10_Packed:
    case PixelType_Gvsp_BayerGB12:
    case PixelType_Gvsp_BayerGB12_Packed:
    case PixelType_Gvsp_BayerBG12:
    case PixelType_Gvsp_BayerBG12_Packed:
    case PixelType_Gvsp_BayerRG12:
    case PixelType_Gvsp_BayerRG12_Packed:
    case PixelType_Gvsp_BayerGR12:
    case PixelType_Gvsp_BayerGR12_Packed:
    case PixelType_Gvsp_BayerRBGG8:
    case PixelType_Gvsp_BayerGR16:
    case PixelType_Gvsp_BayerRG16:
    case PixelType_Gvsp_BayerGB16:
    case PixelType_Gvsp_BayerBG16:
        return true;
    default:
        return false;
    }
}

bool IsMono(MvGvspPixelType enType)
{
    switch (enType) {
    case PixelType_Gvsp_Mono8:
    case PixelType_Gvsp_Mono10:
    case PixelType_Gvsp_Mono10_Packed:
    case PixelType_Gvsp_Mono12:
    case PixelType_Gvsp_Mono12_Packed:
    case PixelType_Gvsp_Mono14:
    case PixelType_Gvsp_Mono16:
        return true;
    default:
        return false;
    }
}

bool HikConvert2Mat(void* handle, MV_FRAME_OUT_INFO_EX* pstImageInfo, unsigned char* pData, cv::Mat& dstImage)
{
    if (NULL == pstImageInfo || NULL == pData) {
        qWarning() << "HikConvert2Mat: null frame info or data";
        return false;
    }

    MvGvspPixelType enDstPixelType = PixelType_Gvsp_Undefined;
    int nDstType = 0;

    if (IsMono(pstImageInfo->enPixelType)) {
        enDstPixelType = PixelType_Gvsp_Mono8;
        nDstType = CV_8UC1;
    }

    else if (IsColor(pstImageInfo->enPixelType)) {
        enDstPixelType = PixelType_Gvsp_RGB8_Packed;
        nDstType = CV_8UC3;
    }

    if (enDstPixelType == PixelType_Gvsp_Undefined) {
        qWarning() << "Unsupported pixel format:" << pstImageInfo->enPixelType;
        return false;
    }

    // 目标缓冲区交给 cv::Mat 自持：create() 在尺寸与类型不变时不重新分配，
    // 转换结果直接写入 Mat 内存，不再手工 malloc/free，避免每帧堆泄漏
    dstImage.create(pstImageInfo->nHeight, pstImageInfo->nWidth, nDstType);
    if (!dstImage.isContinuous()) {
        qWarning() << "HikConvert2Mat: dst mat is not continuous";
        return false;
    }

    MV_CC_PIXEL_CONVERT_PARAM stConvertParam = { 0 };

    stConvertParam.nWidth = pstImageInfo->nWidth;
    stConvertParam.nHeight = pstImageInfo->nHeight;
    stConvertParam.pSrcData = pData;
    stConvertParam.nSrcDataLen = pstImageInfo->nFrameLen;
    stConvertParam.enSrcPixelType = pstImageInfo->enPixelType;
    stConvertParam.enDstPixelType = enDstPixelType;
    stConvertParam.pDstBuffer = dstImage.data;
    stConvertParam.nDstBufferSize = static_cast<unsigned int>(dstImage.total() * dstImage.elemSize());

    auto nRet = MV_CC_ConvertPixelType(handle, &stConvertParam);
    if (MV_OK != nRet) {
        qWarning() << "Convert Pixel Type fail! ret =" << nRet;
        return false;
    }

    return true;
}

void __stdcall ImageCallBack(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    if (!pFrameInfo || !pData)
        return;

    HikCamera* pCamera = static_cast<HikCamera*>(pUser);
    if (!pCamera)
        return;

    cv::Mat cvImage;
    if (!HikConvert2Mat(pCamera->CameraHandle(), pFrameInfo, pData, cvImage))
        return;

    pCamera->ImageQueue().Put(cvImage);
}

HikCamera::HikCamera(const CameraMetaInfo& info)
    : CameraInterface(info)
{
}

HikCamera::~HikCamera()
{
}

uint32_t HikCamera::EnumCamera(QVector<CameraMetaInfo>& cameraInfos)
{

    static bool logPathSet = false;
    if (!logPathSet) {
        const QString logDir = QCoreApplication::applicationDirPath() + QStringLiteral("/MvSDKLog");
        QDir().mkpath(logDir);
        QByteArray logPath = logDir.toLocal8Bit();
        MV_CC_SetSDKLogPath(logPath.constData());
        logPathSet = true;
    }

    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    auto nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet) {
        return ZYCLEAR_OK;
    }

    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* cameraInfo = stDeviceList.pDeviceInfo[i];
        if (cameraInfo->nTLayerType == MV_GIGE_DEVICE) {
            CameraMetaInfo info;
            info.Serial = (char*)cameraInfo->SpecialInfo.stGigEInfo.chSerialNumber;
            info.UserDefineID = (char*)cameraInfo->SpecialInfo.stGigEInfo.chUserDefinedName;
            info.VenderName = (char*)cameraInfo->SpecialInfo.stGigEInfo.chManufacturerName;
            cameraInfos.push_back(info);
        } else if (cameraInfo->nTLayerType == MV_USB_DEVICE) {
            CameraMetaInfo info;
            info.Serial = (char*)cameraInfo->SpecialInfo.stUsb3VInfo.chSerialNumber;
            info.UserDefineID = (char*)cameraInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName;
            info.VenderName = (char*)cameraInfo->SpecialInfo.stUsb3VInfo.chManufacturerName;
            cameraInfos.push_back(info);
        }
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::getParamList(QVector<CameraParam>& paramList)
{
    static QList<CameraParamMetaInfo> paramMetaInfoList = {
        { "DeviceControl", "DeviceVendorName", STRING, "", QStringLiteral("设备制造商名称") },
        { "DeviceControl", "DeviceUserID", STRING, "", QStringLiteral("设备名称，默认为空，可自行设置") },
        { "DeviceControl", "DeviceSerialNumber", STRING, "", QStringLiteral("设备序列号") },
        { "ImageFormatControl", "WidthMax", INT, "", QStringLiteral("最大宽度") },
        { "ImageFormatControl", "HeightMax", INT, "", QStringLiteral("最大高度") },
        { "ImageFormatControl", "Width", INT, "", QStringLiteral("ROI 区域横向的分辨率") },
        { "ImageFormatControl", "Height", INT, "", QStringLiteral("ROI 区域纵向的分辨率") },
        { "ImageFormatControl", "OffsetX", INT, "", QStringLiteral("ROI 区域左上角起点位置的横坐标") },
        { "ImageFormatControl", "OffsetY", INT, "", QStringLiteral("ROI 区域左上角起点位置的纵坐标") },
        { "ImageFormatControl", "ReverseX", BOOL, "", QStringLiteral("相机图像左右翻转") },
        { "ImageFormatControl", "ReverseY", BOOL, "", QStringLiteral("相机图像上下翻转") },
        { "ImageFormatControl", "PixelFormat", ENUM, "", QStringLiteral("相机支持多种像素格式，用户可根据需要自行设置像素格式") },
        { "AcquisitionControl", "AcquisitionMode", ENUM, "", QStringLiteral("采集模式") },
        { "AcquisitionControl", "AcquisitionStart", CMD, "", QStringLiteral("开始采集") },
        { "AcquisitionControl", "AcquisitionStop", CMD, "", QStringLiteral("停止采集") },
        { "AcquisitionControl", "AcquisitionFrameRateEnable", BOOL, "", QStringLiteral("帧率使能") },
        { "AcquisitionControl", "AcquisitionFrameRate", DOUBLE, "", QStringLiteral("需求帧率") },
        { "AcquisitionControl", "TriggerSelector", ENUM, "", QStringLiteral("触发选项") },
        { "AcquisitionControl", "TriggerMode", ENUM, "", QStringLiteral("触发模式") },
        { "AcquisitionControl", "TriggerSoftware", CMD, "", QStringLiteral("软触发") },
        { "AcquisitionControl", "TriggerSource", ENUM, "", QStringLiteral("触发源设置") },
        { "AcquisitionControl", "ExposureTime", DOUBLE, "", QStringLiteral("曝光设置") },
        { "AnalogControl", "Gain", DOUBLE, "", QStringLiteral("增益设置") },
        { "AnalogControl", "BlackLevel", INT, "", QStringLiteral("黑电平设置") },
        { "AnalogControl", "BlackLevelEnable", BOOL, "", QStringLiteral("黑电平设置使能") },
        { "AnalogControl", "BalanceWhiteAuto", BOOL, "", QStringLiteral("自动白平衡") },
        { "AnalogControl", "Gamma", DOUBLE, "", QStringLiteral("Gamma校正") },
        { "AnalogControl", "GammaSelector", ENUM, "", QStringLiteral("Gamma校正") },
        { "AnalogControl", "GammaEnable", BOOL, "", QStringLiteral("Gamma校正使能") },
        { "AnalogControl", "Sharpness", INT, "", QStringLiteral("锐度设置") },
        { "AnalogControl", "SharpnessEnable", BOOL, "", QStringLiteral("锐度设置使能") },
        { "AnalogControl", "ContrastRatio", DOUBLE, "", QStringLiteral("对比度设置") },
        { "AnalogControl", "ContrastRatioEnable", BOOL, "", QStringLiteral("对比度设置使能") },
        { "UserSetControl ", "UserSetSelector", ENUM, "", QStringLiteral("用户参数组选择") },
        { "UserSetControl ", "UserSetLoad", CMD, "", QStringLiteral("参数组加载") },
        { "UserSetControl ", "UserSetSave", CMD, "", QStringLiteral("参数组保存") },
        { "UserSetControl ", "UserSetDefault", ENUM, "", QStringLiteral("默认用户参数组设置") }
    };

    for (auto var : paramMetaInfoList) {
        paramList.push_back(CameraParam(var));
    }

    return ZYCLEAR_OK;
}

bool HikCamera::isConnect()
{
    if (m_cameraHandle == NULL) {
        return false;
    }

    return MV_CC_IsDeviceConnected(m_cameraHandle);
}

bool HikCamera::isGrabbing()
{
    if (m_cameraHandle == NULL) {
        return false;
    }

    return isStartGrabbing;
}

uint32_t HikCamera::acquire()
{

    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    auto nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet) {
        return CAMERA_ACQUIRE_FAILED;
    }

    for (int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* cameraInfo = stDeviceList.pDeviceInfo[i];
        QString serial = (char*)cameraInfo->SpecialInfo.stGigEInfo.chSerialNumber;
        if (serial == Serial()) {
            m_pDeviceInfo = cameraInfo;
            break;
        }
    }

    if (m_pDeviceInfo == NULL)
        return CAMERA_ACQUIRE_FAILED;

    nRet = MV_CC_CreateHandle(&m_cameraHandle, m_pDeviceInfo);
    if (MV_OK != nRet) {
        return INVALID_CAMERA_HANDLE;
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::release()
{

    MV_CC_DestroyHandle(m_cameraHandle);
    m_cameraHandle = NULL;

    // delete m_pDeviceInfo;
    m_pDeviceInfo = NULL;

    return ZYCLEAR_OK;
}

uint32_t HikCamera::connect()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    if (MV_CC_IsDeviceAccessible(m_pDeviceInfo, MV_ACCESS_Exclusive) == false) {
        return DEVICE_NOT_ACCESSIBLE;
    }

    auto nRet = MV_CC_OpenDevice(m_cameraHandle);
    if (MV_OK != nRet)
        return CONNECT_ERROR;

    nRet = MV_CC_RegisterImageCallBackEx(m_cameraHandle, ImageCallBack, this);
    if (MV_OK != nRet)
        return CONNECT_ERROR;

    return ZYCLEAR_OK;
}

uint32_t HikCamera::disconnect()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    auto nRet = MV_CC_RegisterImageCallBackEx(m_cameraHandle, NULL, NULL);
    if (MV_OK != nRet)
        return DISCONNECT_ERROR;

    nRet = MV_CC_CloseDevice(m_cameraHandle);
    if (MV_OK != nRet)
        return DISCONNECT_ERROR;

    return ZYCLEAR_OK;
}

uint32_t HikCamera::creatStream()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    MV_CC_SetGrabStrategy(m_cameraHandle, MV_GRAB_STRATEGY::MV_GrabStrategy_OneByOne);

    MV_CC_SetImageNodeNum(m_cameraHandle, ImageQueueSize);

    return ZYCLEAR_OK;
}

uint32_t HikCamera::destroyStream()
{
    return ZYCLEAR_OK;
}

uint32_t HikCamera::startGrabbing()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    auto nRet = MV_CC_StartGrabbing(m_cameraHandle);
    if (MV_OK != nRet) {
        return STARTGRAB_ERROR;
    }
    isStartGrabbing = true;

    return ZYCLEAR_OK;
}

uint32_t HikCamera::stopGrabbing()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    auto nRet = MV_CC_StopGrabbing(m_cameraHandle);
    if (MV_OK != nRet) {
        return STOPGRAB_ERROR;
    }
    isStartGrabbing = false;

    return ZYCLEAR_OK;
}

uint32_t HikCamera::loadConfig(const QString path)
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    auto nRet = MV_CC_FeatureLoad(m_cameraHandle, path.toLocal8Bit().data());
    if (MV_OK != nRet) {
        return CAMERA_CONFIG_LOAD_FAILED;
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::saveConfig(const QString path)
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    auto nRet = MV_CC_FeatureSave(m_cameraHandle, path.toLocal8Bit().data());
    if (MV_OK != nRet) {
        return CAMERA_CONFIG_SAVE_FAILED;
    }

    return ZYCLEAR_OK;
}

QString HikCamera::configFormat()
{
    return "mfs";
}

uint32_t HikCamera::readParam(CameraParam& param)
{

    getFeatureAccessMode(param);
    if (param.isReadable() == false) {
        return ZYCLEAR_OK;
    }

    if (param.type() == INT) {
        QString name = param.name();
        MVCC_INTVALUE value {};
        auto nRet = MV_CC_GetIntValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;

        IntParam varParam = param.GetValue().value<IntParam>();
        varParam.value = value.nCurValue;
        varParam.increment = value.nInc;
        varParam.min = value.nMin;
        varParam.max = value.nMax;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == DOUBLE) {
        QString name = param.name();
        MVCC_FLOATVALUE value {};
        auto nRet = MV_CC_GetFloatValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;

        DoubleParam varParam = param.GetValue().value<DoubleParam>();
        varParam.value = value.fCurValue;
        varParam.min = value.fMin;
        varParam.max = value.fMax;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == ENUM) {
        QString name = param.name();

        MVCC_ENUMVALUE value {};
        memset(&value, 0, sizeof(MVCC_ENUMVALUE));

        auto nRet = MV_CC_GetEnumValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;

        MVCC_ENUMENTRY entryValue {};
        memset(&entryValue, 0, sizeof(MVCC_ENUMENTRY));
        entryValue.nValue = value.nCurValue;
        MV_CC_GetEnumEntrySymbolic(m_cameraHandle, name.toLocal8Bit().data(), &entryValue);

        QVector<int> intlist {};
        QVector<QString> strlist {};
        for (unsigned int i = 0; i < value.nSupportedNum; i++) {
            int curInt = value.nSupportValue[i];
            intlist.push_back(curInt);

            MVCC_ENUMENTRY entryValue {};
            memset(&entryValue, 0, sizeof(MVCC_ENUMENTRY));
            entryValue.nValue = curInt;
            MV_CC_GetEnumEntrySymbolic(m_cameraHandle, name.toLocal8Bit().data(), &entryValue);
            QString curStr = entryValue.chSymbolic;
            strlist.push_back(curStr);
        }

        EnumParam varParam = param.GetValue().value<EnumParam>();
        varParam.value = entryValue.chSymbolic;
        varParam.valueInt = value.nCurValue;
        varParam.availableInt = intlist;
        varParam.availableValue = strlist;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == BOOL) {
        QString name = param.name();
        bool bValue {};
        auto nRet = MV_CC_GetBoolValue(m_cameraHandle, name.toLocal8Bit().data(), &bValue);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;

        BoolParam varParam = param.GetValue().value<BoolParam>();
        varParam.value = bValue;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == CMD) {

    } else if (param.type() == STRING) {
        QString name = param.name();
        MVCC_STRINGVALUE value {};
        auto nRet = MV_CC_GetStringValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;

        StringParam varParam = param.GetValue().value<StringParam>();
        varParam.value = value.chCurValue;
        varParam.nMaxLength = value.nMaxLength;
        param.SetValue(QVariant::fromValue(varParam));
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::writeParam(CameraParam& param)
{

    getFeatureAccessMode(param);
    if (param.isWriteable() == false) {
        return ZYCLEAR_OK;
    }

    if (param.type() == INT) {
        QString name = param.name();
        IntParam varValue = param.GetValue().value<IntParam>();
        auto nRet = MV_CC_SetIntValue(m_cameraHandle, name.toLocal8Bit().data(), varValue.value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;
    } else if (param.type() == DOUBLE) {
        QString name = param.name();
        DoubleParam varValue = param.GetValue().value<DoubleParam>();
        float value = varValue.value;
        auto nRet = MV_CC_SetFloatValue(m_cameraHandle, name.toLocal8Bit().data(), value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;
    } else if (param.type() == ENUM) {
        QString name = param.name();
        EnumParam varValue = param.GetValue().value<EnumParam>();
        int value = varValue.valueInt;
        auto nRet = MV_CC_SetEnumValue(m_cameraHandle, name.toLocal8Bit().data(), value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;
    } else if (param.type() == BOOL) {
        QString name = param.name();
        BoolParam varValue = param.GetValue().value<BoolParam>();
        bool value = varValue.value;
        auto nRet = MV_CC_SetBoolValue(m_cameraHandle, name.toLocal8Bit().data(), value);
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;
    } else if (param.type() == CMD) {
        QString name = param.name();
        auto nRet = MV_CC_SetCommandValue(m_cameraHandle, name.toLocal8Bit().data());
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;
    } else if (param.type() == STRING) {
        QString name = param.name();
        StringParam varValue = param.GetValue().value<StringParam>();
        QString value = varValue.value;
        auto nRet = MV_CC_SetStringValue(m_cameraHandle, name.toLocal8Bit().data(), value.toLocal8Bit().data());
        if (nRet != MV_OK)
            return WRITE_PARAM_FAILED;
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::getImageLast(cv::Mat& image)
{

    auto ret = m_imageQueue.Take(image);
    if (ret != ZYCLEAR_OK) {
        return GETIAMGE_TIMEOUT;
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::getFeatureAccessMode(CameraParam& param)
{
    QString name = param.name();

    MV_XML_AccessMode mode;
    MV_XML_GetNodeAccessMode(m_cameraHandle, name.toLocal8Bit().data(), &mode);
    if (mode == AM_NI || mode == AM_NA || mode == AM_Undefined) {
        param.setValid(false);
        param.setReadable(false);
        param.setWriteable(false);
    } else if (mode == AM_WO) {
        param.setValid(true);
        param.setReadable(false);
        param.setWriteable(true);
    } else if (mode == AM_RO) {
        param.setValid(true);
        param.setReadable(true);
        param.setWriteable(false);
    } else if (mode == AM_RW) {
        param.setValid(true);
        param.setReadable(true);
        param.setWriteable(true);
    }

    return ZYCLEAR_OK;
}

