#ifndef IMAGEITEM_H
#define IMAGEITEM_H

#include <QGraphicsPixmapItem>

class ImageItem : public QObject, public QGraphicsPixmapItem {
    Q_OBJECT
public:
    explicit ImageItem(QWidget* parent = nullptr);
signals:
    void RGBValue(QString InfoVal);

protected:
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event);

public:
    int w;
    int h;
};

#endif

