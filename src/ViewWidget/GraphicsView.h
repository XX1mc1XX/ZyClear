#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include "ImageItem.h"
#include <QBoxLayout>
#include <QGraphicsView>
#include <QLabel>
#include <qevent.h>

// 缩放与平移一律改视图变换，不动图元本身：像素始终以原始分辨率躺在
// pixmap 里，悬停取色才能继续按像素下标直接寻址（见 ImageItem）。
class GraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    GraphicsView(QWidget* parent = 0);
    ~GraphicsView();

    bool InitWidget();

    void SetImage(const QImage& image);

    void Clear();

protected:
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

public slots:

    void OnCenter();

    void OnZoom(double scaleFactor);

private:

    void fitFrame();
    void PrepareBackgroundBoard(bool invertColor = false);

private:
    // QGraphicsView::scale 是相对当前变换做乘法，没法直接设定绝对值，于是
    // 自己记账累计缩放倍率：fitFrame 的除法与滚轮上下限判定都靠它。
    double m_dZoomValue = 1;

    QGraphicsScene* m_pScene;
    ImageItem* m_pImageItem;
    QWidget* m_pPosInfoWidget;
    QLabel* m_pPosInfoLabel;
    // 原图像素尺寸单独留一份：pixmap 是给渲染用的，空图或替换后拿不到
    // "上一帧多大"，而 fitFrame 必须按原始像素换算适配比例。
    QSize m_qImageSize;
    QPixmap m_qTilePixmap = QPixmap(36, 36);
};

#endif

