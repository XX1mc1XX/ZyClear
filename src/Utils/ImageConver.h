#ifndef IMAGECONVER_H
#define IMAGECONVER_H

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <QtCore/QDebug>
#include <QtGui/QImage>

namespace ImageConver {

// 相机 SDK 交出来的是 cv::Mat，界面要的是 QImage，这里是两者之间唯一的桥。
// 两个函数都写成 static：每个包含本头的编译单元各持一份副本，省掉 ODR 麻烦，
// 代价是代码重复 —— 当前只有 CameraContext 在用，可以接受。
//
// 两者共用一个关键契约：clone=false 时构造出的对象只是「借用」对方的像素缓冲，
// 不做深拷贝。于是被借的那一方必须活得更久，且期间不能被改写或复用。
static QImage cvMat2QImage(const cv::Mat& mat, bool clone = true, bool rb_swap = true)
{
    // 直接指向 mat 的像素首地址。下面构造 QImage 时务必把 mat.step 一并传进去：
    // OpenCV 的行间可能有对齐填充，自己按「宽 × 通道数」算行长会整幅错位。
    const uchar* pSrc = (const uchar*)mat.data;

    if (mat.type() == CV_8UC1) {
        // QImage image(mat.cols, mat.rows, QImage::Format_Grayscale8);
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        if (clone)
            return image.copy();
        return image;
    }

    else if (mat.type() == CV_8UC3) {

        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        if (clone) {
            if (rb_swap)
                return image.rgbSwapped();
            return image.copy();
        } else {
            // 这是就地改写：cvtColor(mat, mat, ...) 把结果写回入参 mat 指向的
            // 那块缓冲。形参写着 const cv::Mat& 也只约束了头部重绑，
            // 拦不住写像素 —— 调用方的原图会被永久改成 RGB 顺序。
            // 需要保留原图就别走这一支，用默认的 clone=true（复制之后再换）。
            if (rb_swap) {
                cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            }
            return image;
        }

    } else if (mat.type() == CV_8UC4) {
        // 四通道一律按 ARGB32 解释，不做通道交换。相机若给的是 BGRA 顺序，
        // 这里不会纠正，画面会红蓝颠倒，只能由调用方先转好再进来。
        qDebug() << "CV_8UC4";
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        if (clone)
            return image.copy();
        return image;
    } else {
        // 只认上面三种类型，其余一律给空 QImage 由调用方判空。
        // 这里仅有一行 qDebug：发布构建若定义了 QT_NO_DEBUG_OUTPUT，
        // 这条路径就完全静默，界面上只表现为一片空白 ——
        // 遇到「相机明明有图但显示不出来」先怀疑是类型没落在上面三种里。
        qDebug() << "ERROR: Mat could not be converted to QImage.";
        return QImage();
    }
}

// 形参有意不是 const 引用：灰度分支要用 QImage::bits()（非 const 版本）
// 才拿得到可写指针，虽然本函数从不写它。
static cv::Mat QImage2cvMat(QImage& image, bool clone = true, bool rb_swap = true)
{
    cv::Mat mat;
    // 打印格式是为「转换后颜色/布局不对」留下第一手线索。无条件下刷屏同前。
    qDebug() << image.format();
    // 只覆盖相机实际会产生的这几种格式，表外的会落空、返回一个空 Mat，
    // 调用方需要自己判 empty()。
    switch (image.format()) {
    // 三十二位分支不做通道交换是有原因的：QImage 的 RGB32/ARGB32 在小端机器上
    // 内存里存的本来就是 BGRA，与 OpenCV 的通道顺序天然一致。
    //
    // 另外这里把 QImage 的缓冲直接当成 Mat 的数据源，默认靠 clone 复制走；
    // clone=false 时两者共用同一块内存，QImage 必须活得更久，
    // 而且此时写这个 Mat 等于绕过 const 改掉那张 QImage。
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
        if (clone)
            mat = mat.clone();
        break;
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
        if (clone)
            mat = mat.clone();
        // 名字有误导性：缓冲里本来已经是 RGB，套上 BGR2RGB 的重排之后
        // 实际得到的是 BGR，为的是迁就下游 OpenCV 默认的 BGR 约定。
        // 与上面 cvMat2QImage 里的交换方向正好相反。
        if (rb_swap)
            cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
        break;
    case QImage::Format_Indexed8:
    case QImage::Format_Grayscale8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.bits(), image.bytesPerLine());
        if (clone)
            mat = mat.clone();
        break;
    }
    return mat;
}
}

#endif

