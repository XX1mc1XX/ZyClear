#ifndef IMAGEITEM_H
#define IMAGEITEM_H

#include <QGraphicsPixmapItem>

// 图元自身不被缩放或旋转（缩放全部发生在视图变换上），于是它的事件坐标
// 就是图像像素坐标，悬停取色可以直接当下标用，不必做坐标反变换。
class ImageItem : public QObject, public QGraphicsPixmapItem {
    Q_OBJECT
public:
    explicit ImageItem(QWidget* parent = nullptr);
signals:
    // 发出的是拼好的整串文本，格式口径由本类独占，视图侧只负责显示
    void RGBValue(QString InfoVal);

protected:
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event);

public:
    // 由 GraphicsView::SetImage 逐帧写入，仅用于坐标条显示；像素本体留在
    // pixmap 里，视图侧不再存第二份 QImage。
    int w;
    int h;
};

#endif

