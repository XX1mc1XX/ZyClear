#include <QtTest>

#include "CameraFactory/CameraFactory.h"
#include "CameraFactory/VirtualCamera.h"

// 相机工厂：品牌注册表、按元信息创建、枚举。
//
// 本测试目标不定义 ZYCLEAR_HAS_HIK_SDK，因此工厂里只会注册虚拟相机品牌
// ——这正是想要的：测试不依赖任何硬件，也不依赖厂商 SDK 是否装在本机。
class TestCameraFactory : public QObject
{
    Q_OBJECT

private slots:

    // 进程级单例：注册表只在首次访问时装配一次
    void instanceIsSingleton()
    {
        QCOMPARE(CameraFactory::instance(), CameraFactory::instance());
    }

    void virtualVendorIsRegistered()
    {
        QVERIFY(CameraFactory::instance()->getSupportedVenders()
                    .contains(VirtualCamera::VIRTUAL_CAMERA_VENDER));
    }

    // 品牌名需逐字匹配：大小写或空格不同都不算已支持
    void isVenderSupportedMatchesExactly()
    {
        CameraFactory* factory = CameraFactory::instance();

        QVERIFY(factory->isVenderSupported(VirtualCamera::VIRTUAL_CAMERA_VENDER));
        QVERIFY(!factory->isVenderSupported(QStringLiteral("Nobody")));
        QVERIFY(!factory->isVenderSupported(VirtualCamera::VIRTUAL_CAMERA_VENDER.toLower()));
        QVERIFY(!factory->isVenderSupported(QString()));
    }

    // 未注册品牌回 nullptr 而不抛异常——调用方必须判空
    void createCameraWithUnknownVendorReturnsNull()
    {
        CameraMetaInfo info;
        info.Serial = QStringLiteral("whatever");
        info.VenderName = QStringLiteral("Nobody");

        QCOMPARE(CameraFactory::instance()->createCamera(info), nullptr);
    }

    // 创建出来的对象带着传入的元信息，序列号与显示名都要原样可读回
    void createCameraKeepsMetaInfo()
    {
        CameraMetaInfo info;
        info.Serial = QStringLiteral("SN-0001");
        info.UserDefineID = QStringLiteral("相机甲");
        info.VenderName = VirtualCamera::VIRTUAL_CAMERA_VENDER;

        CameraInterface* camera = CameraFactory::instance()->createCamera(info);
        QVERIFY(camera != nullptr);

        QCOMPARE(camera->Serial(), QStringLiteral("SN-0001"));
        QCOMPARE(camera->UserName(), QStringLiteral("相机甲"));

        delete camera;
    }

    // 序列号由调用方给定而非适配器自报，因此同一个序列号可以造出多个独立对象；
    // 去重是门面层的职责（按序列号收口），不是工厂的
    void createCameraReturnsFreshInstanceEachTime()
    {
        CameraMetaInfo info;
        info.Serial = QStringLiteral("SN-0002");
        info.VenderName = VirtualCamera::VIRTUAL_CAMERA_VENDER;

        CameraInterface* first = CameraFactory::instance()->createCamera(info);
        CameraInterface* second = CameraFactory::instance()->createCamera(info);

        QVERIFY(first != nullptr);
        QVERIFY(second != nullptr);
        QVERIFY(first != second);

        delete first;
        delete second;
    }

    // 各品牌枚举器的返回值被丢弃，本函数恒报成功：
    // 「没找到相机」与「枚举失败」在这一层无法区分
    void enumCamerasAlwaysReportsSuccess()
    {
        QVector<CameraMetaInfo> infos;
        QCOMPARE(CameraFactory::instance()->enumCameras(infos),
                 static_cast<uint32_t>(ZYCLEAR_OK));
    }

    void enumCamerasFindsVirtualCamera()
    {
        QVector<CameraMetaInfo> infos;
        CameraFactory::instance()->enumCameras(infos);

        bool found = false;
        for (const CameraMetaInfo& info : infos) {
            if (info.VenderName == VirtualCamera::VIRTUAL_CAMERA_VENDER) {
                found = true;

                // 这里曾钉住过一处字段顺序写反的现状：源文件的聚合初始化按
                // { NAME, SERIAL, VENDER } 传，而 CameraMetaInfo 声明的是
                // { Serial, UserDefineID, VenderName }，于是序列号与显示名互换。
                // 现已对齐声明顺序，断言也随之改为按各自语义核对
                QCOMPARE(info.Serial, VirtualCamera::VIRTUAL_CAMERA_SERIAL);
                QCOMPARE(info.UserDefineID, VirtualCamera::VIRTUAL_CAMERA_NAME);
            }
        }
        QVERIFY(found);
    }

    // 枚举结果追加到入参容器而不清空它，调用方可以自行累积多轮结果
    void enumCamerasAppendsToExistingResults()
    {
        QVector<CameraMetaInfo> infos;

        CameraMetaInfo preset;
        preset.Serial = QStringLiteral("preset");
        infos.push_back(preset);

        CameraFactory::instance()->enumCameras(infos);

        QCOMPARE(infos.size(), 2);
        QCOMPARE(infos.first().Serial, QStringLiteral("preset"));
    }

    // 多次枚举结果稳定：虚拟相机的序列号是常量，上层才能按序列号稳定索引同一台设备
    void enumCamerasIsRepeatable()
    {
        QVector<CameraMetaInfo> first;
        QVector<CameraMetaInfo> second;
        CameraFactory::instance()->enumCameras(first);
        CameraFactory::instance()->enumCameras(second);

        QCOMPARE(first.size(), second.size());
        for (int i = 0; i < first.size(); ++i) {
            QCOMPARE(first.at(i).Serial, second.at(i).Serial);
        }
    }

    // 相等性只由序列号决定：改显示名不应被当成另一台设备，
    // 否则重命名后的相机会在去重环节重复登记
    void metaInfoEqualityIgnoresDisplayName()
    {
        CameraMetaInfo a;
        a.Serial = QStringLiteral("SN-X");
        a.UserDefineID = QStringLiteral("旧名");

        CameraMetaInfo b;
        b.Serial = QStringLiteral("SN-X");
        b.UserDefineID = QStringLiteral("新名");

        // operator== 未标 const，这里用非 const 左值调用
        QVERIFY(a == b);

        b.Serial = QStringLiteral("SN-Y");
        QVERIFY(!(a == b));
    }
};

QTEST_MAIN(TestCameraFactory)
#include "test_camerafactory.moc"
