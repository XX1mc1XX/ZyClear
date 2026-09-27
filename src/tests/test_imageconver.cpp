#include <QtTest>

#include "Utils/ImageConver.h"

// 图像转换桥：cv::Mat 与 QImage 之间的唯一通道。
//
// 这里盯的是三件容易出错的事：
//   · 行跨距：OpenCV 的行间可能有对齐填充，忘了传 mat.step 会整幅错位；
//   · 通道方向：cvMat2QImage 与 QImage2cvMat 的交换方向正好相反；
//   · 借用语义：clone=false 时两边共用同一块缓冲，谁先没谁遭殃。

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 3;

} // namespace

class TestImageConver : public QObject
{
    Q_OBJECT

private slots:

    void grayscaleMapsToGrayscale8()
    {
        cv::Mat mat(kHeight, kWidth, CV_8UC1, cv::Scalar(42));

        const QImage image = ImageConver::cvMat2QImage(mat);

        QCOMPARE(image.format(), QImage::Format_Grayscale8);
        QCOMPARE(image.width(), kWidth);
        QCOMPARE(image.height(), kHeight);
        QCOMPARE(image.constScanLine(0)[0], static_cast<uchar>(42));
    }

    void threeChannelMapsToRgb888()
    {
        cv::Mat mat(kHeight, kWidth, CV_8UC3, cv::Scalar(1, 2, 3));

        const QImage image = ImageConver::cvMat2QImage(mat);

        QCOMPARE(image.format(), QImage::Format_RGB888);
        QCOMPARE(image.width(), kWidth);
        QCOMPARE(image.height(), kHeight);
    }

    // 四通道一律按 ARGB32 解释，不做通道交换
    void fourChannelMapsToArgb32()
    {
        cv::Mat mat(kHeight, kWidth, CV_8UC4, cv::Scalar(1, 2, 3, 4));

        const QImage image = ImageConver::cvMat2QImage(mat);

        QCOMPARE(image.format(), QImage::Format_ARGB32);
        QCOMPARE(image.width(), kWidth);
        QCOMPARE(image.height(), kHeight);
    }

    // 只认三通道、四通道与单通道三种，其余给空 QImage 由调用方判空。
    // 这条路径在界面上表现为「相机有图却一片空白」，值得钉住
    void unsupportedTypeYieldsNullImage()
    {
        cv::Mat sixteenBit(kHeight, kWidth, CV_16UC1, cv::Scalar(1));
        QVERIFY(ImageConver::cvMat2QImage(sixteenBit).isNull());

        cv::Mat twoChannel(kHeight, kWidth, CV_8UC2, cv::Scalar(1, 2));
        QVERIFY(ImageConver::cvMat2QImage(twoChannel).isNull());
    }

    // 默认 clone=true：转换后两者各自独立，改动原图不影响已转换的 QImage
    void clonedImageIsIndependentOfSource()
    {
        cv::Mat mat(kHeight, kWidth, CV_8UC1, cv::Scalar(5));

        const QImage image = ImageConver::cvMat2QImage(mat, true, false);
        QVERIFY(image.constBits() != mat.data);

        mat.at<uchar>(0, 0) = 99;
        QCOMPARE(image.constScanLine(0)[0], static_cast<uchar>(5));
    }

    // clone=false 是借用：QImage 直接指向 mat 的像素缓冲，不做深拷贝
    void unclonedImageSharesTheSourceBuffer()
    {
        cv::Mat mat(kHeight, kWidth, CV_8UC1, cv::Scalar(7));

        const QImage image = ImageConver::cvMat2QImage(mat, false, false);

        QCOMPARE(image.constBits(), mat.data);
    }

    // 行跨距必须来自 mat.step：取一块 ROI 出来，它的行长大于「宽 × 通道数」，
    // 若按宽度自己算行长，第二行起就会整幅错位
    void nonContinuousMatRespectsStep()
    {
        cv::Mat big(6, 16, CV_8UC1, cv::Scalar(0));
        cv::Mat roi = big(cv::Rect(3, 1, 5, 4)); // 宽 5、高 4，但行长仍是 16
        roi.setTo(77);

        const QImage image = ImageConver::cvMat2QImage(roi, true, false);

        QCOMPARE(image.width(), 5);
        QCOMPARE(image.height(), 4);
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 5; ++col) {
                QCOMPARE(image.constScanLine(row)[col], static_cast<uchar>(77));
            }
        }
    }

    // OpenCV 按 BGR 存三通道，QImage 按 RGB 解释，因此默认要交换一次
    void channelSwapRunsForThreeChannelByDefault()
    {
        cv::Mat mat(1, 1, CV_8UC3, cv::Scalar(10, 20, 30)); // B=10 G=20 R=30

        const QImage swapped = ImageConver::cvMat2QImage(mat, true, true);
        QCOMPARE(swapped.constScanLine(0)[0], static_cast<uchar>(30)); // R
        QCOMPARE(swapped.constScanLine(0)[1], static_cast<uchar>(20)); // G
        QCOMPARE(swapped.constScanLine(0)[2], static_cast<uchar>(10)); // B

        // 关掉交换时保持原样，此时红蓝是反的
        const QImage kept = ImageConver::cvMat2QImage(mat, true, false);
        QCOMPARE(kept.constScanLine(0)[0], static_cast<uchar>(10));
        QCOMPARE(kept.constScanLine(0)[2], static_cast<uchar>(30));
    }

    void grayscaleImageMapsToSingleChannel()
    {
        QImage image(kWidth, kHeight, QImage::Format_Grayscale8);
        image.fill(64);

        const cv::Mat mat = ImageConver::QImage2cvMat(image);

        QCOMPARE(mat.type(), CV_8UC1);
        QCOMPARE(mat.cols, kWidth);
        QCOMPARE(mat.rows, kHeight);
        QCOMPARE(mat.at<uchar>(0, 0), static_cast<uchar>(64));
    }

    void rgb888ImageMapsToThreeChannel()
    {
        QImage image(kWidth, kHeight, QImage::Format_RGB888);
        image.fill(Qt::red);

        const cv::Mat mat = ImageConver::QImage2cvMat(image);

        QCOMPARE(mat.type(), CV_8UC3);
        QCOMPARE(mat.cols, kWidth);
        QCOMPARE(mat.rows, kHeight);
    }

    void argb32ImageMapsToFourChannel()
    {
        QImage image(kWidth, kHeight, QImage::Format_ARGB32);
        image.fill(Qt::blue);

        const cv::Mat mat = ImageConver::QImage2cvMat(image);

        QCOMPARE(mat.type(), CV_8UC4);
        QCOMPARE(mat.cols, kWidth);
        QCOMPARE(mat.rows, kHeight);
    }

    // 表外的格式落空，返回空 Mat，调用方需自己判 empty()
    void unsupportedFormatYieldsEmptyMat()
    {
        QImage image(kWidth, kHeight, QImage::Format_Mono);

        const cv::Mat mat = ImageConver::QImage2cvMat(image);

        QVERIFY(mat.empty());
    }

    // 两个函数的通道交换方向相反，往返一次应还原成原图
    void roundTripRestoresPixelOrder()
    {
        cv::Mat source(kHeight, kWidth, CV_8UC3, cv::Scalar(10, 20, 30)); // B G R

        QImage image = ImageConver::cvMat2QImage(source, true, true);
        const cv::Mat back = ImageConver::QImage2cvMat(image, true, true);

        QCOMPARE(back.type(), CV_8UC3);
        QCOMPARE(back.at<cv::Vec3b>(0, 0)[0], static_cast<uchar>(10)); // B
        QCOMPARE(back.at<cv::Vec3b>(0, 0)[1], static_cast<uchar>(20)); // G
        QCOMPARE(back.at<cv::Vec3b>(0, 0)[2], static_cast<uchar>(30)); // R
    }

    // 灰度往返不带通道交换，逐像素应完全一致
    void grayscaleRoundTripKeepsPixels()
    {
        cv::Mat source(2, 2, CV_8UC1, cv::Scalar(0));
        source.at<uchar>(0, 0) = 11;
        source.at<uchar>(1, 1) = 22;

        const QImage image = ImageConver::cvMat2QImage(source, true, false);
        QImage mutableImage = image;
        const cv::Mat back = ImageConver::QImage2cvMat(mutableImage, true, false);

        QCOMPARE(back.at<uchar>(0, 0), static_cast<uchar>(11));
        QCOMPARE(back.at<uchar>(1, 1), static_cast<uchar>(22));
    }

    // clone=false 时 QImage 与 Mat 共用缓冲，改动其中一方另一方立刻可见。
    // 这是既定契约，不是缺陷——但意味着被借的一方必须活得更久
    void sharedBufferReflectsWritesFromEitherSide()
    {
        cv::Mat mat(2, 2, CV_8UC1, cv::Scalar(3));

        const QImage image = ImageConver::cvMat2QImage(mat, false, false);
        mat.at<uchar>(0, 0) = 88;

        QCOMPARE(image.constScanLine(0)[0], static_cast<uchar>(88));
    }
};

// 转换只用 QImage，不需要窗口系统，故走 GUILESS 入口
QTEST_GUILESS_MAIN(TestImageConver)
#include "test_imageconver.moc"
