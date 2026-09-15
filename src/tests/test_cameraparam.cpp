#include <QtTest>

#include "CameraInterface/ZCCameraParam.h"

/**
 * 统一参数模型的行为测试。
 *
 * 这一层是「厂商差异不外泄」的关键：六种参数类型被压进同一个外观类，
 * 上层只认 displayText / 访问权限三态。测试固定这些对外契约，
 * 并验证原型复制与 QVariant 装箱在跨线程信号投递中不会丢数据。
 */
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
        // 命令型参数没有"值"，界面上只呈现一个可点击的占位文本
        CameraParam param = makeParam(CMD);
        param.SetValue(QVariant::fromValue(CmdParam()));

        QCOMPARE(param.displayText(), QStringLiteral("{Command}"));
    }

    void displayTextForUnknownTypeIsSafe()
    {
        // 未知类型不能崩，也不能返回空串让界面出现空白单元格
        CameraParam param = makeParam(UNKNOWN);
        QCOMPARE(param.displayText(), QStringLiteral("unknow"));
    }

    void accessModeDefaultsToAllFalse()
    {
        // 默认应视为不可用，避免设备未上报权限时界面误开放编辑
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
        // 原型复制用于把参数投递给界面后仍保留原件，副本改动不得回写原件
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
        // 参数值要经 QVariant 跨线程投递，装箱拆箱后字段必须完好
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
