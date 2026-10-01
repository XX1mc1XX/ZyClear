#include "HikCamera.h"
#include "opencv2/opencv.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

const QString HikCamera::HIK_CAMERA_VENDER = "Hikrobot";

// 像素格式判定只服务一件事：给 HikConvert2Mat 选目标格式，不参与业务逻辑。
// Bayer 系一并算作彩色，是因为硬件转换通道会顺带做去马赛克，转换后即得真彩。
// 收录范围要与转换目标互补：本层把结果统一成 Mono8 与 RGB8_Packed，
// 因此相机若把 OutputFormat 就设成 RGB8_Packed，必须在这里认出来 ——
// 否则两个判定同时落空，帧会走到下游的 unsupported 分支被静默丢弃。
bool IsColor(MvGvspPixelType enType)
{
    switch (enType) {
    case PixelType_Gvsp_BGR8_Packed:
    case PixelType_Gvsp_RGB8_Packed:
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

// 与 IsColor 必须互补且不留缝：漏掉任何一种在用格式，整条链路就会丢帧。
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

// 本层“关差异”的核心：把千奇百怪的采集格式归一成 Mono8 / RGB8 两种，
// 上层只认 CV_8UC1 与 CV_8UC3，位深、Bayer、YUV 的差异全在这里消化。
// 转换交给 SDK 的 MV_CC_ConvertPixelType，而不是自己逐像素写：
// 相机侧/CPU 侧的加速路径由 SDK 挑，且手写要为每种格式各来一遍。
// 任何失败都返回 false 且不产出半成品 Mat：上层只会看到“这一帧没来”，
// 不会拿到尺寸正确但内容还是上一帧的图。
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

    // 目标缓冲由 Mat 自持，create() 在尺寸与类型不变时不会重新分配
    // 源长度必须取帧自带的 nFrameLen，不能按宽高乘位深推算：
    // Packed 格式行尾有补齐，推算值会偏小，SDK 直接判转换失败。
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

// __stdcall 是 MVS 回调的 ABI 硬要求，调用约定不符会在栈上出问题。
// 回调运行在 SDK 的内部采集线程而非 UI 线程，整段函数体（含像素转换）都在那个
// 线程上跑，所以这里的耗时直接顶到 SDK 的收帧节奏上，不能做重活。
// pUser 是 connect() 注册时透传的 this；相机对象生命周期长于回调注册期，
// 前提是反注册必须先于对象析构。
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

// 析构刻意不碰句柄：句柄由 acquire() 建、release() 销，与 connect()/disconnect()
// 一起构成两段式生命周期，由 CameraContext 成对驱动。
// 代价是上层若漏调 release()，句柄不会随对象消失而回收——这里是泄漏点而非兜底点。
HikCamera::~HikCamera()
{
}

// 日志目录是 SDK 的进程级全局设置，只在首次枚举时写一次：重复设置会被 SDK 忽略，
// 用 static 打点省掉每次枚举的 mkpath 开销。
// 枚举失败按“当前没有相机”处理，不向调用方报错，返回值恒为成功。
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

    // 只枚举 GigE 与 USB3 两类传输层，其余接口类型不在支持范围。
    // 列表结构体必须清零后传入，否则各 SDK 版本对未初始化计数字段的容错不同。
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    auto nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet) {
        return ZYCLEAR_OK;
    }

    // 两种传输层的设备信息共用一个 union 的不同分支，必须按 nTLayerType 各取其分支。
    // VenderName 取的是设备自报的厂商串，工厂之后正是拿它查注册表，
    // 所以该串必须与注册键逐字对上，否则设备枚举得到却创建不出来。
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

// 这份清单是 UI 参数面板的静态骨架：group 只是展示用的分组标题，name 才是
// 下发时用的 GenICam 节点名，tips 纯文案，三者都不参与 SDK 调用。
// 用 static 让它只构造一次；参数真实的可读写性要到 readParam 时由
// getFeatureAccessMode 覆写，这里的声明不预设权限。
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
        { "UserSetControl", "UserSetSelector", ENUM, "", QStringLiteral("用户参数组选择") },
        { "UserSetControl", "UserSetLoad", CMD, "", QStringLiteral("参数组加载") },
        { "UserSetControl", "UserSetSave", CMD, "", QStringLiteral("参数组保存") },
        { "UserSetControl", "UserSetDefault", ENUM, "", QStringLiteral("默认用户参数组设置") }
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

// acquire 只建句柄不连设备，与 connect() 合起来才是一次完整接入，
// 对应 CameraContext 里 acquire→connect、disconnect→release 的两段式驱动。
uint32_t HikCamera::acquire()
{

    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    auto nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet) {
        return CAMERA_ACQUIRE_FAILED;
    }

    // 每次接入都重新枚举，是为了拿一份新鲜的设备信息：插拔过的设备旧信息已失效。
    // 按序列号精确回找目标机，找不到就失败——不退化成“连第一台”，避免多机时连错。
    // GigE 与 USB3 的设备信息共用 SpecialInfo 这个 union 的不同分支，必须按
    // nTLayerType 各取各的：两种设备该字段只是偏移恰好一致，依赖它是自找麻烦。
    bool found = false;
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* cameraInfo = stDeviceList.pDeviceInfo[i];

        QString serial;
        if (cameraInfo->nTLayerType == MV_GIGE_DEVICE) {
            serial = (char*)cameraInfo->SpecialInfo.stGigEInfo.chSerialNumber;
        } else if (cameraInfo->nTLayerType == MV_USB_DEVICE) {
            serial = (char*)cameraInfo->SpecialInfo.stUsb3VInfo.chSerialNumber;
        }

        if (serial == Serial()) {
            // 值拷贝进本对象持有：枚举列表的内存归 SDK 管，下一次枚举（例如依次
            // 连接多台时另一台的 acquire）就会覆写它；继续引用原节点的话，
            // 早先那台的 m_pDeviceInfo 会变成悬垂指针，而 connect() 还要用它。
            m_deviceInfo = *cameraInfo;
            m_pDeviceInfo = &m_deviceInfo;
            found = true;
            break;
        }
    }

    if (!found)
        return CAMERA_ACQUIRE_FAILED;

    // m_pDeviceInfo 指向的是上面那份自有副本，CreateHandle 会把设备信息拷进句柄
    nRet = MV_CC_CreateHandle(&m_cameraHandle, m_pDeviceInfo);
    if (MV_OK != nRet) {
        return INVALID_CAMERA_HANDLE;
    }

    return ZYCLEAR_OK;
}

uint32_t HikCamera::release()
{

    // 句柄销毁后立刻置空，后续任何 SDK 调用都会被各入口的 NULL 前置检查挡住。
    MV_CC_DestroyHandle(m_cameraHandle);
    m_cameraHandle = NULL;

    // 设备信息是上面那份自有副本，随对象析构回收，这里只断掉本对象的引用。
    // 它曾被写成指向 SDK 枚举列表的裸指针，那句被注释掉的 delete 就是当年
    // 误删 SDK 内存留下的；注意 m_deviceInfo 是成员，不能 delete。
    m_pDeviceInfo = NULL;

    return ZYCLEAR_OK;
}

uint32_t HikCamera::connect()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    // 先判独占再打开：IsDeviceAccessible(MV_ACCESS_Exclusive) 反映“此刻能否被独占访问”，
    // 别的进程占着设备时这里就失败，把冲突提前到打开之前暴露。
    // 该判断依据枚举时缓存的访问模式，GigE 下可能滞后，真正权威的仍是 OpenDevice
    // 的返回值，所以两步都要查，少一步都可能在多进程抢机时误判。
    if (MV_CC_IsDeviceAccessible(m_pDeviceInfo, MV_ACCESS_Exclusive) == false) {
        return DEVICE_NOT_ACCESSIBLE;
    }

    auto nRet = MV_CC_OpenDevice(m_cameraHandle);
    if (MV_OK != nRet)
        return CONNECT_ERROR;

    // 回调在 OpenDevice 之后、StartGrabbing 之前注册，并把 this 透传成 pUser，
    // 采集线程就能顺着它回到本对象的句柄与队列，不必维护全局映射表。
    nRet = MV_CC_RegisterImageCallBackEx(m_cameraHandle, ImageCallBack, this);
    if (MV_OK != nRet)
        return CONNECT_ERROR;

    return ZYCLEAR_OK;
}

// 先反注册回调再关设备，两步之间 SDK 不会再回调进来。
// 前置条件是调用方已 stopGrabbing：否则可能仍有帧正在回调里执行，
// CloseDevice 会在回调线程仍持有句柄时把它收掉。
uint32_t HikCamera::disconnect()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    // 传 NULL 即反注册，是 SDK 约定的摘除方式，并没有另一对 Unregister 接口。
    auto nRet = MV_CC_RegisterImageCallBackEx(m_cameraHandle, NULL, NULL);
    if (MV_OK != nRet)
        return DISCONNECT_ERROR;

    nRet = MV_CC_CloseDevice(m_cameraHandle);
    if (MV_OK != nRet)
        return DISCONNECT_ERROR;

    return ZYCLEAR_OK;
}

// 起采前的两项初始化，都必须在 StartGrabbing 之前设，采起来后再调不生效。
// OneByOne 表示 SDK 内部不丢帧、按序交付，丢帧策略统一交给我们自己的有界队列，
// 两边都丢会让“丢最旧”的语义变得不可预测。
uint32_t HikCamera::creatStream()
{
    if (m_cameraHandle == NULL) {
        return INVALID_CAMERA_HANDLE;
    }

    MV_CC_SetGrabStrategy(m_cameraHandle, MV_GRAB_STRATEGY::MV_GrabStrategy_OneByOne);

    // SDK 内部缓存节点数取与下游队列等深：太少会在上层来不及取时反压采集，
    // 太多则停采后残留的帧变多、丢帧也更迟才发生。
    MV_CC_SetImageNodeNum(m_cameraHandle, ImageQueueSize);

    return ZYCLEAR_OK;
}

// 海康没有独立的“销毁流”步骤：抓取策略与缓存节点数都挂在句柄上、随停采回收，
// 所以这里保持空实现，而不是去复位那两项设置。
uint32_t HikCamera::destroyStream()
{
    return ZYCLEAR_OK;
}

// isStartGrabbing 是本地状态镜像，只为 isGrabbing() 省一次 SDK 往返；
// 它仅在起采成功后置位，失败必须保持 false，否则上层会误以为已在采集。
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

// 停采是同步的：SDK 保证返回后不再进新帧，但取帧侧的最坏等待仍是队列超时上限，
// 上层若要卡住界面，得自己把它放到后台线程。
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

// mfs 是海康私有的参数文件格式，等价于 UserSet 的导入导出。
// 路径按本地 8bit 编码传给 SDK，中文路径在部分 SDK 版本会失败，尽量走 ASCII 路径。
// 前置条件：设备已连接且未在采集中，采集态下调这个接口会直接报错。
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

// 与 loadConfig 的约束一致，同样只能在已连接、未采集时落盘。
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

// 交给 UI 决定文件对话框的过滤后缀，必须与实际写盘格式一致，改一处要同步另一处。
QString HikCamera::configFormat()
{
    return "mfs";
}

// 前置条件：句柄已建（acquire 之后），本函数直接拿 m_cameraHandle 下发，不做空句柄兜底。
// UI 是按 getParamList 的清单全量刷新的，所以对不可读节点直接按成功返回、不产出值，
// 而不是报错——否则面板会被红字刷屏。
uint32_t HikCamera::readParam(CameraParam& param)
{

    getFeatureAccessMode(param);
    if (param.isReadable() == false) {
        return ZYCLEAR_OK;
    }

    // 各分支失败一律回 READ_PARAM_FAILED：读路径与写路径的错误码不再混用，
    // 调用方可据返回码判断失败方向。
    if (param.type() == INT) {
        QString name = param.name();
        MVCC_INTVALUE value {};
        auto nRet = MV_CC_GetIntValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return READ_PARAM_FAILED;

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
            return READ_PARAM_FAILED;

        DoubleParam varParam = param.GetValue().value<DoubleParam>();
        varParam.value = value.fCurValue;
        varParam.min = value.fMin;
        varParam.max = value.fMax;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == ENUM) {
        QString name = param.name();

        // nSupportValue 是 SDK 定长数组，遍历上界只能取 SDK 回填的 nSupportedNum，
        // 既不能假设它一定等于数组容量，也不能把容量写死。
        MVCC_ENUMVALUE value {};
        memset(&value, 0, sizeof(MVCC_ENUMVALUE));

        auto nRet = MV_CC_GetEnumValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return READ_PARAM_FAILED;

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
            return READ_PARAM_FAILED;

        BoolParam varParam = param.GetValue().value<BoolParam>();
        varParam.value = bValue;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == CMD) {

        // 命令节点没有可读的“值”，读操作本就无意义，留空即可。
    } else if (param.type() == STRING) {
        QString name = param.name();
        MVCC_STRINGVALUE value {};
        auto nRet = MV_CC_GetStringValue(m_cameraHandle, name.toLocal8Bit().data(), &value);
        if (nRet != MV_OK)
            return READ_PARAM_FAILED;

        StringParam varParam = param.GetValue().value<StringParam>();
        varParam.value = value.chCurValue;
        varParam.nMaxLength = value.nMaxLength;
        param.SetValue(QVariant::fromValue(varParam));
    }

    return ZYCLEAR_OK;
}

// 与 readParam 对称：先刷权限，不可写节点静默跳过，让全量“应用参数”不至于
// 因为个别只读项而整体失败。写入是同步生效的，相机侧立即改状态。
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
        // SDK 只有 float 接口，double 转 float 会丢精度；整数项走 int64 透传，位深足够。
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

// Take 内部最多等队列超时上限，超时与空队列都折算成 GETIAMGE_TIMEOUT。
// 取出的 Mat 所有权归调用方，用完必须经 ImageQueue().Recycle() 归还：
// 空闲池一旦耗尽，队列就退化成持续新建缓冲，长跑下内存会来回抖。
uint32_t HikCamera::getImageLast(cv::Mat& image)
{

    auto ret = m_imageQueue.Take(image);
    if (ret != ZYCLEAR_OK) {
        return GETIAMGE_TIMEOUT;
    }

    return ZYCLEAR_OK;
}

// 借 GenICam XML 节点的 AccessMode 反推参数能力并回填到 param 上，
// 让 UI 能直接隐藏/置灰不支持的项，而不必为每个品牌维护一份硬编码黑名单。
uint32_t HikCamera::getFeatureAccessMode(CameraParam& param)
{
    QString name = param.name();

    // 未实现(NI)、不可用(NA) 与未定义都按“无此参数”处理：三者对 UI 而言没有区别。
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

