#include <QtTest>

#include "CameraInterface/CameraError.h"
#include "CameraInterface/CameraImageQueue.h"

// 图像缓冲队列：取帧超时、保新弃旧、缓冲归还与复用
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

        // 空队列应等待一个超时周期再返回
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
        // 容量 2 连投 3 帧，最旧的应被丢弃
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
        // 容量 1 便于断言缓冲复用
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

        // 同尺寸同类型应原地复用
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

        // 分辨率变化时应重新分配
        QVERIFY(takenBig.data != smallAddress);
        QCOMPARE(takenBig.rows, 64);
        QCOMPARE(takenBig.at<uchar>(0, 0), static_cast<uchar>(5));
    }

    void bufferAddressStaysStableAcrossCycles()
    {
        // 循环往复时每帧应落在同一块缓冲上，堆分配只发生在首帧
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
