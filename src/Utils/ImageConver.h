#ifndef IMAGECONVER_H
#define IMAGECONVER_H

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <QtCore/QDebug>
#include <QtGui/QImage>

namespace ImageConver {

static QImage cvMat2QImage(const cv::Mat& mat, bool clone = true, bool rb_swap = true)
{
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
            if (rb_swap) {
                cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            }
            return image;
        }

    } else if (mat.type() == CV_8UC4) {
        qDebug() << "CV_8UC4";
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        if (clone)
            return image.copy();
        return image;
    } else {
        qDebug() << "ERROR: Mat could not be converted to QImage.";
        return QImage();
    }
}

static cv::Mat QImage2cvMat(QImage& image, bool clone = true, bool rb_swap = true)
{
    cv::Mat mat;
    qDebug() << image.format();
    switch (image.format()) {
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

