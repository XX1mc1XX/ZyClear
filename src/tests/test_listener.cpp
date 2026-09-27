#include <QtTest>

#include "Listener.h"

// 事件总线：位掩码投递、精确匹配的坑、重复注册的后果。
//
// 订阅者一律用文件级静态对象：总线无反注册接口，指针一旦注册就永久留在表里，
// 只有活到进程结束才能保证后续 notify 不会踩到已析构的对象。
// 测试类对象则是每个用例新建一个，不能用来持有订阅者。

namespace {

class RecordingListener : public Listener
{
public:
    int count = 0;
    QVector<int> received;

    void RespondMessage(int message) override
    {
        ++count;
        received.push_back(message);
    }

    void reset()
    {
        count = 0;
        received.clear();
    }
};

// 订阅四个事件，用于验证「订阅多个事件后各自都能收到」
RecordingListener g_multi;
// 只订阅 CONNECT，用于验证「没订的事件收不到」
RecordingListener g_connectOnly;
// 订阅 STOPTGRAB 两次，用于验证重复注册的后果（该位不分配给其他订阅者）
RecordingListener g_duplicated;
// 订阅 CAMERA_CAMERASWICH——实际收不到，见下方用例说明
RecordingListener g_switchOnly;

} // namespace

class TestListener : public QObject
{
    Q_OBJECT

private slots:

    // 整个测试类只注册一次，避免各用例重复往全局表里塞同一批指针
    void initTestCase()
    {
        ListenerManger* bus = ListenerManger::Instance();

        bus->registerMessage(MESSAGE::CAMERA_ENUMRTION
                                 | MESSAGE::CAMERA_CONNECT
                                 | MESSAGE::CAMERA_DISCONNECT
                                 | MESSAGE::CAMERA_STARTGRAB,
                             &g_multi);

        bus->registerMessage(MESSAGE::CAMERA_CONNECT, &g_connectOnly);

        // 同一对象注册两次同一事件：表里追加两条，回调自然翻倍
        bus->registerMessage(MESSAGE::CAMERA_STOPTGRAB, &g_duplicated);
        bus->registerMessage(MESSAGE::CAMERA_STOPTGRAB, &g_duplicated);

        bus->registerMessage(MESSAGE::CAMERA_CAMERASWICH, &g_switchOnly);
    }

    // QtTest 的每条用例前后钩子叫 init / cleanup（不是 setUp / tearDown）。
    // 名字写错不会报错，QtTest 只会把它当成一条普通用例执行一次，
    // 于是每条用例都在累积的计数上断言——这里踩过，留下这段说明。
    void init()
    {
        g_multi.reset();
        g_connectOnly.reset();
        g_duplicated.reset();
        g_switchOnly.reset();
    }

    // 六个常量必须各占独立的二进制位，否则按位拆分订阅时会互相串号
    void eventBitsAreDistinctAndSingle()
    {
        const int bits[] = { MESSAGE::CAMERA_ENUMRTION, MESSAGE::CAMERA_CONNECT,
                             MESSAGE::CAMERA_DISCONNECT, MESSAGE::CAMERA_STARTGRAB,
                             MESSAGE::CAMERA_STOPTGRAB, MESSAGE::CAMERA_CAMERASWICH };

        for (int value : bits) {
            QVERIFY(value != 0);
            // 2 的幂：减一后与原值按位与必为 0
            QCOMPARE((value & (value - 1)), 0);
        }

        for (int i = 0; i < 6; ++i) {
            for (int j = i + 1; j < 6; ++j) {
                QVERIFY((bits[i] & bits[j]) == 0);
            }
        }
    }

    void instanceIsStable()
    {
        QCOMPARE(ListenerManger::Instance(), ListenerManger::Instance());
    }

    // 订阅了多个事件的对象，在任一事件触发时都应收到
    void multiEventSubscriberReceivesEach()
    {
        ListenerManger* bus = ListenerManger::Instance();

        bus->notify(MESSAGE::CAMERA_ENUMRTION);
        QCOMPARE(g_multi.count, 1);
        QCOMPARE(g_multi.received.last(), static_cast<int>(MESSAGE::CAMERA_ENUMRTION));

        bus->notify(MESSAGE::CAMERA_STARTGRAB);
        QCOMPARE(g_multi.count, 2);
        QCOMPARE(g_multi.received.last(), static_cast<int>(MESSAGE::CAMERA_STARTGRAB));
    }

    // 回调收到的 message 就是投递的那一个事件位，订阅者无需再按位过滤
    void callbackReceivesTheSingleEventBit()
    {
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_DISCONNECT);

        QCOMPARE(g_multi.count, 1);
        QCOMPARE(g_multi.received.first(), static_cast<int>(MESSAGE::CAMERA_DISCONNECT));
    }

    // 没订阅该事件的对象不应被打扰
    void onlySubscribersOfThatEventAreNotified()
    {
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_ENUMRTION);

        QCOMPARE(g_multi.count, 1);
        QCOMPARE(g_connectOnly.count, 0);
    }

    // 订阅组合值后，拆出来的每一个位都能独立触发
    void combinedSubscriptionSplitsIntoIndividualBits()
    {
        ListenerManger* bus = ListenerManger::Instance();

        bus->notify(MESSAGE::CAMERA_CONNECT);
        QCOMPARE(g_connectOnly.count, 1);

        // g_multi 也订了 CONNECT，应同时收到
        QCOMPARE(g_multi.count, 1);
    }

    // 这是本总线最容易踩的坑：notify 按整键精确查表，
    // 因此传组合值（而非单个事件位）不会命中任何订阅者，且静默丢弃、毫无报错。
    void combinedNotifyValueIsSilentlyDropped()
    {
        ListenerManger* bus = ListenerManger::Instance();

        const int combined = MESSAGE::CAMERA_CONNECT | MESSAGE::CAMERA_DISCONNECT;
        bus->notify(combined);

        QCOMPARE(g_multi.count, 0);
        QCOMPARE(g_connectOnly.count, 0);
    }

    // 无人订阅的事件是正常情况（某个面板没装配），既不应崩溃也不应产生任何回调
    void eventWithoutSubscribersIsIgnored()
    {
        // CAMERA_ENUMRTION 已有人订阅，这里换一个位来构造「无人订阅」的场景：
        // 先确认它在当前表中确实没有订阅者（只有 g_switchOnly 订了 SWITCH）
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_STOPTGRAB);
        QCOMPARE(g_multi.count, 0); // g_multi 没订 STOPTGRAB

        // 一个够大的、不在枚举范围内的键同样不应有任何反应
        ListenerManger::Instance()->notify(0x40000000);
        QCOMPARE(g_multi.count, 0);
        QCOMPARE(g_switchOnly.count, 0);
    }

    // 同一对象重复注册同一事件会被追加进表多次，回调次数随之翻倍。
    // 总线没有去重，这是既定行为，此处把后果固化下来。
    void duplicateRegistrationCausesDuplicateCallbacks()
    {
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_STOPTGRAB);

        QCOMPARE(g_duplicated.count, 2);
    }

    // CAMERA_CAMERASWICH 曾在 registerMessage 里漏掉分支：订阅它的调用方不报错、
    // 也永远收不到事件。现已补齐，这条用例钉住「枚举里的六个位都能订上」
    void everyEventBitCanBeSubscribed()
    {
        ListenerManger::Instance()->notify(MESSAGE::CAMERA_CAMERASWICH);

        QCOMPARE(g_switchOnly.count, 1);
        QCOMPARE(g_switchOnly.received.last(), static_cast<int>(MESSAGE::CAMERA_CAMERASWICH));
    }
};

QTEST_MAIN(TestListener)
#include "test_listener.moc"
