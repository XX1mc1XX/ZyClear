#include "ImageItem.h"
#include <QGraphicsSceneHoverEvent>

ImageItem::ImageItem(QWidget* parent)
    : QGraphicsPixmapItem(nullptr)
{
    // parent 参数实际未透传给基类，图元没有 QObject 父对象，所有权归
    // addItem 之后的 scene；不打开悬停事件的话 hoverMoveEvent 根本不会来。
    setAcceptHoverEvents(true);
}

void ImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF mousePosition = event->pos();
    int R, G, B;
    int x, y;
    x = mousePosition.x();
    y = mousePosition.y();
    // 图上没有像素的地方（棋盘底纹区域）会给出负坐标，夹到 0 只为让显示
    // 不至于出现负值；右下方向越界不夹，pixelColor 会回无效色显示成 0,0,0。
    if (mousePosition.x() < 0) {
        x = 0;
    }
    if (mousePosition.y() < 0) {
        y = 0;
    }
    // pixmap().toImage() 每次悬停都整幅转一遍，高分辨率下鼠标一动就是一次
    // 全图拷贝；这里为了不额外缓存一份 QImage 而接受这个开销。
    pixmap().toImage().pixelColor(x, y).getRgb(&R, &G, &B);
    QString InfoVal = QString(" W:%1,H:%2 | X:%3,Y:%4 | R:%5,G:%6,B:%7")
                          .arg(QString::number(w))
                          .arg(QString::number(h))
                          .arg(QString::number(x))
                          .arg(QString::number(y))
                          .arg(QString::number(R))
                          .arg(QString::number(G))
                          .arg(QString::number(B));
    emit RGBValue(InfoVal);
}

