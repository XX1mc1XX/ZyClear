#include <QtTest>

#include "CameraInterface/ZCCameraParam.h"

// 参数模型：六种类型的显示文本、访问权限三态、原型复制与 QVariant 装箱
class TestCameraParam : public QObject
{
    Q_OBJECT

private:
    static CameraParam makeParam(ZCParamType type, const QString& name = QStringLiteral("TestParam"))
    {
        CameraParamMetaInfo meta { QStringLiteral("TestGroup"), name, type, QString(), QStringLiteral("说明") };
        return CameraParam(meta);
    }

private slots:

    void displayTextForInt()
    {
        CameraParam param = makeParam(INT);
        IntParam value;
        value.value = 30;
        param.SetValue(QVariant::fromValue(value));

        QCOMPARE(param.displayText(), QStringLiteral("30"));
    }

    void displayTextForDouble()
    {
        CameraParam param = makeParam(DOUBLE);
        DoubleParam value;
        value.value = 1.5;
        param.SetValue(QVariant::fromValue(value));

        QCOMPARE(param.displayText(), QStringLiteral("1.5"));
    }

    void displayTextForBool()
    {
        CameraParam param = makeParam(BOOL);
        BoolParam value;
        value.value = true;
        param.SetValue(QVariant::fromValue(value));

        QCOMPARE(param.displayText(), QStringLiteral("True"));
    }

    void displayTextForEnum()
    {
        CameraParam param = makeParam(ENUM);
        EnumParam value;
        value.value = QStringLiteral("Continuous");
        param.SetValue(QVariant::fromValue(value));

        QCOMPARE(param.displayText(), QStringLiteral("Continuous"));
    }

    void displayTextForString()
    {
        CameraParam param = makeParam(STRING);
        StringParam value;
        value.value = QStringLiteral("abc");
        param.SetValue(QVariant::fromValue(value));

        QCOMPARE(param.displayText(), QStringLiteral("abc"));
    }

    void displayTextForCommandIsPlaceholder()
    {
        // 命令型参数只呈现可点击的占位文本
        CameraParam param = makeParam(CMD);
        param.SetValue(QVariant::fromValue(CmdParam()));

        QCOMPARE(param.displayText(), QStringLiteral("{Command}"));
    }

    void displayTextForUnknownTypeIsSafe()
    {
        // 未知类型不应崩溃或返回空串
        CameraParam param = makeParam(UNKNOWN);
        QCOMPARE(param.displayText(), QStringLiteral("unknow"));
    }

    void accessModeDefaultsToAllFalse()
    {
        // 默认为不可用，避免权限未上报时误开放编辑
        CameraParam param = makeParam(INT);

        QVERIFY(!param.isValid());
        QVERIFY(!param.isReadable());
        QVERIFY(!param.isWriteable());
    }

    void accessModeIsStoredPerFlag()
    {
        CameraParam param = makeParam(INT);
        param.setValid(true);
        param.setReadable(true);
        param.setWriteable(false);

        QVERIFY(param.isValid());
        QVERIFY(param.isReadable());
        QVERIFY(!param.isWriteable());
    }

    void metadataIsAccessibleThroughAccessors()
    {
        CameraParamMetaInfo meta { QStringLiteral("AnalogControl"), QStringLiteral("Gain"),
            DOUBLE, QStringLiteral("ExposureTime"), QStringLiteral("增益设置") };
        CameraParam param(meta);

        QCOMPARE(param.group(), QStringLiteral("AnalogControl"));
        QCOMPARE(param.name(), QStringLiteral("Gain"));
        QVERIFY(param.type() == DOUBLE);
        QCOMPARE(param.relativeList(), QStringLiteral("ExposureTime"));
        QCOMPARE(param.tips(), QStringLiteral("增益设置"));
    }

    void cloneProducesIndependentCopy()
    {
        // 副本改动不应回写原件
        IntParam original;
        original.value = 10;
        original.min = 0;
        original.max = 100;

        ZCParam* copy = original.clone();
        static_cast<IntParam*>(copy)->value = 99;

        QCOMPARE(original.value, static_cast<int64_t>(10));
        QCOMPARE(static_cast<IntParam*>(copy)->value, static_cast<int64_t>(99));

        delete copy;
    }

    void variantRoundTripPreservesValue()
    {
        // 装箱拆箱后字段应完好
        IntParam source;
        source.value = 123;
        source.increment = 5;

        QVariant boxed = QVariant::fromValue(source);
        IntParam restored = boxed.value<IntParam>();

        QCOMPARE(restored.value, static_cast<int64_t>(123));
        QCOMPARE(restored.increment, static_cast<int64_t>(5));
    }
};

QTEST_MAIN(TestCameraParam)
#include "test_cameraparam.moc"
