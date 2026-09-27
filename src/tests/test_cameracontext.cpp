#include <QtTest>

#include "CameraFactory/VirtualCamera.h"
#include "CameraInterface/CameraContext.h"
#include "CameraInterface/CameraError.h"
#include "CameraInterface/ZCCameraMetaInfo.h"
#include "CameraInterface/ZCCameraParam.h"

// 门面层：序列号寻址、状态查询、参数取用与错误码口径。
//
// 门面是进程级单例，各用例共享同一份设备注册表，因此这里做了两件事：
//   · 序列号在 initTestCase 里枚举一次后记在文件作用域，用例之间复用；
//   · 每条用例开始前把设备拉回「已断开、未拉流」这个已知状态。
// 本目标不定义 ZYCLEAR_HAS_HIK_SDK，门面里只会注册虚拟相机，与真机和厂商 SDK 无关。

namespace {

QString g_serial;

const QString kUnknownSerial = QStringLiteral("no-such-serial");

} // namespace

class TestCameraContext : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        QVector<CameraMetaInfo> infos;
        QCOMPARE(CameraContext::Instance()->EnumerationCamera(infos),
                 static_cast<uint32_t>(ZYCLEAR_OK));

        QVERIFY(!infos.isEmpty());

        // 序列号从枚举结果里取，不写死具体值：适配器怎么填由它自己决定
        g_serial = infos.first().Serial;
        QVERIFY(!g_serial.isEmpty());
    }

    // 门面持有的设备状态跨用例留存，先恢复到已知状态再开始
    void init()
    {
        CameraContext::Instance()->stopGrabbing(g_serial);
        CameraContext::Instance()->disconnect(g_serial);
    }

    void instanceIsSingleton()
    {
        QCOMPARE(CameraContext::Instance(), CameraContext::Instance());
    }

    // 枚举出的序列号必须能被寻址，否则上层拿到的列表全是无效条目
    void enumeratedSerialIsAddressable()
    {
        bool connected = true;
        QCOMPARE(CameraContext::Instance()->isConnect(g_serial, connected),
                 static_cast<uint32_t>(ZYCLEAR_OK));

        bool grabbing = true;
        QCOMPARE(CameraContext::Instance()->isGrabbing(g_serial, grabbing),
                 static_cast<uint32_t>(ZYCLEAR_OK));
    }

    // 序列号未注册时，所有以序列号寻址的接口统一回 NOCAMERA_ERROR，
    // 调用方据此分支即可，不必再区分「没这台设备」与「设备已拔掉」
    void unknownSerialReportsNoCamera()
    {
        CameraContext* context = CameraContext::Instance();

        bool state = false;
        QVector<CameraParam> params;
        QImage image;

        QCOMPARE(context->isConnect(kUnknownSerial, state), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->isGrabbing(kUnknownSerial, state), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->connect(kUnknownSerial), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->disconnect(kUnknownSerial), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->startGrabbing(kUnknownSerial), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->stopGrabbing(kUnknownSerial), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->getParamList(kUnknownSerial, params), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->getImageLast(kUnknownSerial, image), static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->loadConfig(kUnknownSerial, QStringLiteral("x.mfs")),
                 static_cast<uint32_t>(NOCAMERA_ERROR));
        QCOMPARE(context->saveConfig(kUnknownSerial, QStringLiteral("x.mfs")),
                 static_cast<uint32_t>(NOCAMERA_ERROR));
    }

    // 这一条与上一条的口径不同：序列号不认识时回空串，而不是错误码，
    // 调用方要靠「空」判失败
    void unknownSerialConfigFormatIsEmpty()
    {
        QVERIFY(CameraContext::Instance()->getConfigFormat(kUnknownSerial).isEmpty());
    }

    void connectThenDisconnectTogglesState()
    {
        CameraContext* context = CameraContext::Instance();

        bool connected = false;
        QVERIFY(context->isConnect(g_serial, connected) == ZYCLEAR_OK);
        QVERIFY(!connected);

        QCOMPARE(context->connect(g_serial), static_cast<uint32_t>(ZYCLEAR_OK));
        QVERIFY(context->isConnect(g_serial, connected) == ZYCLEAR_OK);
        QVERIFY(connected);

        QCOMPARE(context->disconnect(g_serial), static_cast<uint32_t>(ZYCLEAR_OK));
        QVERIFY(context->isConnect(g_serial, connected) == ZYCLEAR_OK);
        QVERIFY(!connected);
    }

    // 未连接就开流应被挡下：门面在转发前先查设备状态
    void startGrabbingWithoutConnectionIsRejected()
    {
        QCOMPARE(CameraContext::Instance()->startGrabbing(g_serial),
                 static_cast<uint32_t>(CAMERA_NOT_CONNECTED));
    }

    // 停采是幂等的：没在采集时视作成功，避免上层重复停采还要处理错误
    void stopGrabbingWhenIdleSucceeds()
    {
        QCOMPARE(CameraContext::Instance()->stopGrabbing(g_serial),
                 static_cast<uint32_t>(ZYCLEAR_OK));
        QCOMPARE(CameraContext::Instance()->stopGrabbing(g_serial),
                 static_cast<uint32_t>(ZYCLEAR_OK));
    }

    // 未连接时取帧统一折算成超时码，不区分真实原因（未连接 / 没开流 / 队列空）
    void getImageLastOnIdleDeviceReportsTimeout()
    {
        QImage image;
        QCOMPARE(CameraContext::Instance()->getImageLast(g_serial, image),
                 static_cast<uint32_t>(GETIAMGE_TIMEOUT));
    }

    // 未连接时读写配置文件被拒。注意返回的是 CAMERA_NOT_CONNECTED，
    // 而「正在采集」同样走这条分支——错误码与字面语义并不一致
    void configIoIsRejectedWhileDisconnected()
    {
        QCOMPARE(CameraContext::Instance()->loadConfig(g_serial, QStringLiteral("x.mfs")),
                 static_cast<uint32_t>(CAMERA_NOT_CONNECTED));
        QCOMPARE(CameraContext::Instance()->saveConfig(g_serial, QStringLiteral("x.mfs")),
                 static_cast<uint32_t>(CAMERA_NOT_CONNECTED));
    }

    // 参数骨架由 Schema 驱动：虚拟相机从 Qt 资源里的 JSON 读出来，
    // 项数与业务分组应与 Schema 一致，改动 Schema 时这里会跟着失败
    void parameterListMatchesSchema()
    {
        QVector<CameraParam> params;
        QCOMPARE(CameraContext::Instance()->getParamList(g_serial, params),
                 static_cast<uint32_t>(ZYCLEAR_OK));

        QCOMPARE(params.size(), 37);

        // 每一项都应带上分组与类型，否则界面渲染不出控件
        for (const CameraParam& param : params) {
            QVERIFY(!param.name().isEmpty());
            QVERIFY(!param.group().isEmpty());
            QVERIFY(param.type() != UNKNOWN);
        }
    }

    // 参数名在同一台相机内应唯一，否则按名寻址会撞车
    void parameterNamesAreUnique()
    {
        QVector<CameraParam> params;
        CameraContext::Instance()->getParamList(g_serial, params);

        QSet<QString> names;
        for (const CameraParam& param : params) {
            names.insert(param.name());
        }
        QCOMPARE(names.size(), params.size());
    }

    // 取参数是追加语义，调用方可以自行累积多轮结果
    void getParamListAppendsToCallerVector()
    {
        QVector<CameraParam> params;
        CameraContext::Instance()->getParamList(g_serial, params);
        const int firstRound = params.size();

        CameraContext::Instance()->getParamList(g_serial, params);
        QCOMPARE(params.size(), firstRound * 2);
    }

    // 当前序列号只被记录，不做校验：设一个并不存在的序列号也会成功，
    // 且能原样读回——门面不替调用方判断它是否有效
    void currentSerialIsStoredWithoutValidation()
    {
        CameraContext* context = CameraContext::Instance();

        QCOMPARE(context->setCurrentSerial(kUnknownSerial), static_cast<uint32_t>(ZYCLEAR_OK));
        QCOMPARE(context->currentSerial(), kUnknownSerial);

        QCOMPARE(context->setCurrentSerial(g_serial), static_cast<uint32_t>(ZYCLEAR_OK));
        QCOMPARE(context->currentSerial(), g_serial);
    }

    // 重枚举会拆掉既有设备再重建，因此已有连接状态不会保留
    void enumerationRebuildsRegistry()
    {
        CameraContext* context = CameraContext::Instance();

        QCOMPARE(context->connect(g_serial), static_cast<uint32_t>(ZYCLEAR_OK));

        QVector<CameraMetaInfo> infos;
        context->EnumerationCamera(infos);

        bool connected = true;
        QCOMPARE(context->isConnect(g_serial, connected), static_cast<uint32_t>(ZYCLEAR_OK));
        QVERIFY(!connected);
    }

    // 结果按序列号去重：重复枚举不应让列表越滚越长
    void enumerationDeduplicatesBySerial()
    {
        QVector<CameraMetaInfo> infos;
        CameraContext::Instance()->EnumerationCamera(infos);
        const int firstRound = infos.size();
        QVERIFY(firstRound > 0);

        CameraContext::Instance()->EnumerationCamera(infos);
        QCOMPARE(infos.size(), firstRound);
    }

    // 枚举总数与已注册设备数无关：传入已有元素的容器时，只往里追加新发现的
    void enumerationAppendsToCallerVector()
    {
        CameraMetaInfo preset;
        preset.Serial = QStringLiteral("preset-entry");

        QVector<CameraMetaInfo> infos;
        infos.push_back(preset);

        CameraContext::Instance()->EnumerationCamera(infos);
        QCOMPARE(infos.size(), 2);
        QCOMPARE(infos.first().Serial, QStringLiteral("preset-entry"));
    }

    // Release 与 Instance 配对：释放后再次访问会重建一个空门面。
    // 放在最后一条执行——它会把前面各用例积累的注册表清掉
    void releaseDestroysTheFacade()
    {
        CameraContext* before = CameraContext::Instance();
        CameraContext::Release();

        CameraContext* after = CameraContext::Instance();
        QVERIFY(after != nullptr);
        QVERIFY(after != before);

        // 新建的门面还没枚举过任何设备，此时按序列号寻址应当找不到
        bool connected = true;
        QCOMPARE(after->isConnect(g_serial, connected), static_cast<uint32_t>(NOCAMERA_ERROR));
    }
};

// 用 GUILESS：门面只碰 QImage，不需要真实窗口系统。
// 若用 QTEST_MAIN，链接了 QtWidgets 的目标会被判定为 GUI 测试而构造 QApplication，
// 那在 CI 的无显示服务器环境里要额外准备 offscreen 平台插件，纯属自找麻烦。
QTEST_GUILESS_MAIN(TestCameraContext)
#include "test_cameracontext.moc"
