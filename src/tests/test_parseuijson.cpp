#include <QtTest>

#include "ParseUiJson/ParseUiJson.h"

/**
 * 参数 Schema 解析器测试。
 *
 * 「新增型号不编译、不发版」成立的前提是这份 JSON 真的能被可靠解析。
 * 因此测试着重两件事：合法文档要完整还原分组与类型；
 * 非法文档必须被拒并且错误信息能指到具体是哪个分组的第几个参数——
 * 配置由技术支持维护，报错指不到位置等于没法自查。
 */
class TestParseUiJson : public QObject
{
    Q_OBJECT

private slots:

    void init()
    {
        // 解析器是单例，用例之间必须清干净，否则上一个用例的状态会串到下一个
        ParseUiJson::instance()->clear();
    }

    void parseValidDocument()
    {
        const QString json = QStringLiteral(R"([
            {"group":"DeviceControl","params":[
                {"name":"DeviceUserID","type":"STRING","relative_list":"","tips":"设备名称"}
            ]},
            {"group":"AnalogControl","params":[
                {"name":"Gain","type":"DOUBLE","relative_list":"","tips":"增益"},
                {"name":"Sharpness","type":"INT","relative_list":"","tips":"锐度"}
            ]}
        ])");

        QVERIFY(ParseUiJson::instance()->loadFromString(json));
        QVERIFY(ParseUiJson::instance()->isValid());
        QCOMPARE(ParseUiJson::instance()->getParamList().size(), qsizetype(3));
        QCOMPARE(ParseUiJson::instance()->getAllGroups(),
            QStringList({ QStringLiteral("DeviceControl"), QStringLiteral("AnalogControl") }));
    }

    void emitsParseFinishedWithResult()
    {
        QSignalSpy spy(ParseUiJson::instance(), &ParseUiJson::parseFinished);

        const QString json = QStringLiteral(R"([
            {"group":"G","params":[{"name":"A","type":"INT","tips":""}]}
        ])");
        QVERIFY(ParseUiJson::instance()->loadFromString(json));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void rejectNonArrayRoot()
    {
        QVERIFY(!ParseUiJson::instance()->loadFromString(QStringLiteral(R"({"group":"A"})")));
        QVERIFY(!ParseUiJson::instance()->isValid());
        QVERIFY(!ParseUiJson::instance()->getLastError().isEmpty());
    }

    void rejectMalformedJson()
    {
        QVERIFY(!ParseUiJson::instance()->loadFromString(QStringLiteral(R"([{"group":])")));
        QVERIFY(!ParseUiJson::instance()->isValid());
    }

    void rejectGroupWithoutParams()
    {
        const QString json = QStringLiteral(R"([{"group":"DeviceControl"}])");

        QVERIFY(!ParseUiJson::instance()->loadFromString(json));
        QVERIFY(ParseUiJson::instance()->getLastError().contains(QStringLiteral("params")));
    }

    void rejectGroupWithoutName()
    {
        const QString json = QStringLiteral(R"([{"params":[]}])");

        QVERIFY(!ParseUiJson::instance()->loadFromString(json));
        QVERIFY(ParseUiJson::instance()->getLastError().contains(QStringLiteral("group")));
    }

    void rejectParamMissingRequiredField()
    {
        // tips 缺失时要能报出是哪个分组的哪一个参数
        const QString json = QStringLiteral(R"([
            {"group":"AnalogControl","params":[
                {"name":"Gain","type":"DOUBLE","relative_list":""}
            ]}
        ])");

        QVERIFY(!ParseUiJson::instance()->loadFromString(json));

        const QString error = ParseUiJson::instance()->getLastError();
        QVERIFY(error.contains(QStringLiteral("tips")));
        QVERIFY(error.contains(QStringLiteral("AnalogControl")));
    }

    void rejectParamWithWrongFieldType()
    {
        const QString json = QStringLiteral(R"([
            {"group":"AnalogControl","params":[
                {"name":123,"type":"DOUBLE","tips":"增益"}
            ]}
        ])");

        QVERIFY(!ParseUiJson::instance()->loadFromString(json));
        QVERIFY(!ParseUiJson::instance()->isValid());
    }

    void mapsEverySupportedParamType()
    {
        const QString json = QStringLiteral(R"([
            {"group":"Types","params":[
                {"name":"A","type":"INT","tips":""},
                {"name":"B","type":"DOUBLE","tips":""},
                {"name":"C","type":"ENUM","tips":""},
                {"name":"D","type":"BOOL","tips":""},
                {"name":"E","type":"CMD","tips":""},
                {"name":"F","type":"STRING","tips":""}
            ]}
        ])");

        QVERIFY(ParseUiJson::instance()->loadFromString(json));

        const QList<CameraParamMetaInfo> params = ParseUiJson::instance()->getParamList();
        QCOMPARE(params.size(), qsizetype(6));
        QVERIFY(params.at(0).type == INT);
        QVERIFY(params.at(1).type == DOUBLE);
        QVERIFY(params.at(2).type == ENUM);
        QVERIFY(params.at(3).type == BOOL);
        QVERIFY(params.at(4).type == CMD);
        QVERIFY(params.at(5).type == STRING);
    }

    void unknownTypeFallsBackToUnknown()
    {
        // 型号用了新类型而程序版本较旧时，应退化为 UNKNOWN 而不是丢弃整个参数
        const QString json = QStringLiteral(R"([
            {"group":"Types","params":[{"name":"A","type":"COMPLEX","tips":""}]}
        ])");

        QVERIFY(ParseUiJson::instance()->loadFromString(json));
        QVERIFY(ParseUiJson::instance()->getParamList().at(0).type == UNKNOWN);
    }

    void groupQueryReturnsOnlyMatchingGroup()
    {
        const QString json = QStringLiteral(R"([
            {"group":"DeviceControl","params":[{"name":"A","type":"STRING","tips":""}]},
            {"group":"AnalogControl","params":[
                {"name":"B","type":"INT","tips":""},
                {"name":"C","type":"INT","tips":""}
            ]}
        ])");

        QVERIFY(ParseUiJson::instance()->loadFromString(json));

        const QList<CameraParamMetaInfo> analog =
            ParseUiJson::instance()->getParamListByGroup(QStringLiteral("AnalogControl"));

        QCOMPARE(analog.size(), qsizetype(2));
        QCOMPARE(analog.at(0).name, QStringLiteral("B"));
        QCOMPARE(analog.at(1).name, QStringLiteral("C"));
    }

    void clearResetsParserState()
    {
        const QString json = QStringLiteral(R"([
            {"group":"G","params":[{"name":"A","type":"INT","tips":""}]}
        ])");
        QVERIFY(ParseUiJson::instance()->loadFromString(json));

        ParseUiJson::instance()->clear();

        QVERIFY(!ParseUiJson::instance()->isValid());
        QVERIFY(ParseUiJson::instance()->getParamList().isEmpty());
    }
};

QTEST_MAIN(TestParseUiJson)
#include "test_parseuijson.moc"
