#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include "ImageItem.h"
#include <QBoxLayout>
#include <QGraphicsView>
#include <QLabel>
#include <qevent.h>

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
    double m_dZoomValue = 1;

    QGraphicsScene* m_pScene;
    ImageItem* m_pImageItem;
    QWidget* m_pPosInfoWidget;
    QLabel* m_pPosInfoLabel;
    QSize m_qImageSize;
    QPixmap m_qTilePixmap = QPixmap(36, 36);
};

#endif

