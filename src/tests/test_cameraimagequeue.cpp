#include <QtTest>

#include "CameraInterface/CameraError.h"
#include "CameraInterface/CameraImageQueue.h"

/**
 * 图像缓冲队列的行为测试。
 *
 * 这一层是采集线程与显示线程之间唯一的同步点，出错形式都是难复现的偶发问题，
 * 因此逐条把契约固定下来：取帧超时要回超时码、满队列保新弃旧、
 * 缓冲经 Recycle 归还后必须被复用（否则「稳态零分配」只是说法）。
 */
class TestCameraImageQueue : public QObject
{
    Q_OBJECT

private slots:

    void takeOnEmptyQueueReportsTimeout()
    {
        CameraImageQueue queue;

        cv::Mat out;
        QElapsedTimer timer;
        timer.start();

        QCOMPARE(queue.Take(out), static_cast<uint32_t>(GETIAMGE_TIMEOUT));

        // 空队列必须等满一个超时周期才返回，否则消费线程会退化成忙等
        QVERIFY(timer.elapsed() >= TIME_OUT_MS - 20);
    }

    void putThenTakeDeliversFrame()
    {
        CameraImageQueue queue(4);

        cv::Mat frame(8, 8, CV_8UC1, cv::Scalar(42));
        QCOMPARE(queue.Put(frame), static_cast<uint32_t>(ZYCLEAR_OK));

        cv::Mat out;
        QCOMPARE(queue.Take(out), static_cast<uint32_t>(ZYCLEAR_OK));
        QCOMPARE(out.rows, 8);
        QCOMPARE(out.cols, 8);
        QCOMPARE(out.at<uchar>(0, 0), static_cast<uchar>(42));
    }

    void dropsOldestFrameWhenFull()
    {
        // 容量 2：连投 3 帧且不取走，最旧的一帧应被丢弃，队列保留最新两帧
        CameraImageQueue queue(2);

        for (int value = 1; value <= 3; ++value) {
            cv::Mat frame(4, 4, CV_8UC1, cv::Scalar(value));
            queue.Put(frame);
        }

        cv::Mat out;
        QCOMPARE(queue.Take(out), static_cast<uint32_t>(ZYCLEAR_OK));
        QCOMPARE(out.at<uchar>(0, 0), static_cast<uchar>(2)); // 值为 1 的最旧帧已被丢弃

        QCOMPARE(queue.Take(out), static_cast<uint32_t>(ZYCLEAR_OK));
        QCOMPARE(out.at<uchar>(0, 0), static_cast<uchar>(3));

        QCOMPARE(queue.Take(out), static_cast<uint32_t>(GETIAMGE_TIMEOUT)); // 队列已空
    }

    void recycledBufferIsReused()
    {
        // 容量 1 让池中只剩一个缓冲槽，便于断言"归还后确实被复用"
        CameraImageQueue queue(1);

        cv::Mat first(16, 16, CV_8UC1, cv::Scalar(1));
        queue.Put(first);

        cv::Mat taken;
        QCOMPARE(queue.Take(taken), static_cast<uint32_t>(ZYCLEAR_OK));
        const uchar* firstAddress = taken.data;

        queue.Recycle(taken);

        cv::Mat second(16, 16, CV_8UC1, cv::Scalar(2));
        queue.Put(second);

        cv::Mat takenAgain;
        QCOMPARE(queue.Take(takenAgain), static_cast<uint32_t>(ZYCLEAR_OK));

        // 同尺寸同类型必须原地复用，而不是每帧重新分配
        QCOMPARE(takenAgain.data, firstAddress);
        QCOMPARE(takenAgain.at<uchar>(0, 0), static_cast<uchar>(2));
    }

    void reallocatesWhenFrameSizeChanges()
    {
        CameraImageQueue queue(1);

        cv::Mat small(8, 8, CV_8UC1, cv::Scalar(1));
        queue.Put(small);
        cv::Mat taken;
        queue.Take(taken);
        const uchar* smallAddress = taken.data;
        queue.Recycle(taken);

        cv::Mat big(64, 64, CV_8UC1, cv::Scalar(5));
        queue.Put(big);

        cv::Mat takenBig;
        QCOMPARE(queue.Take(takenBig), static_cast<uint32_t>(ZYCLEAR_OK));

        // 分辨率变化时旧缓冲容纳不下，必须重新分配并按新尺寸交付
        QVERIFY(takenBig.data != smallAddress);
        QCOMPARE(takenBig.rows, 64);
        QCOMPARE(takenBig.at<uchar>(0, 0), static_cast<uchar>(5));
    }

    void bufferAddressStaysStableAcrossCycles()
    {
        // 采集—显示循环往复时，每帧都应落在同一块缓冲上，堆分配只发生在首帧。
        // 这是「稳态零动态分配」这一说法的可验证形式
        CameraImageQueue queue(1);
        const uchar* address = nullptr;

        for (int round = 0; round < 5; ++round) {
            cv::Mat frame(16, 16, CV_8UC1, cv::Scalar(round + 1));
            queue.Put(frame);

            cv::Mat taken;
            QCOMPARE(queue.Take(taken), static_cast<uint32_t>(ZYCLEAR_OK));
            QCOMPARE(taken.at<uchar>(0, 0), static_cast<uchar>(round + 1));

            if (address == nullptr) {
                address = taken.data;
            } else {
                QCOMPARE(taken.data, address);
            }

            queue.Recycle(taken);
        }
    }
};

QTEST_MAIN(TestCameraImageQueue)
#include "test_cameraimagequeue.moc"
