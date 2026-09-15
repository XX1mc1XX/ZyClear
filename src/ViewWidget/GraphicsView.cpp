#include "GraphicsView.h"

#define ZOOMMAX 50
#define ZOOMMIN 0.1

GraphicsView::GraphicsView(QWidget* parent)
    : QGraphicsView(parent)
    , m_pScene(Q_NULLPTR)
    , m_pImageItem(Q_NULLPTR)
    , m_pPosInfoWidget(Q_NULLPTR)
    , m_pPosInfoLabel(Q_NULLPTR)
{

    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->setRenderHint(QPainter::Antialiasing);
    this->setTransformationAnchor(QGraphicsView::AnchorViewCenter);

    this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::ScrollHandDrag);
    this->setSceneRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
    PrepareBackgroundBoard();
    centerOn(0, 0);

    if (false == InitWidget()) {
        throw std::bad_alloc();
    }
}

GraphicsView::~GraphicsView()
{
    m_pScene->deleteLater();
    delete m_pImageItem;
}

bool GraphicsView::InitWidget()
{

    m_pScene = new QGraphicsScene(this);
    m_pImageItem = new ImageItem(this);
    //    m_pImageItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    this->setScene(m_pScene);
    m_pScene->addItem(m_pImageItem);
    m_pPosInfoLabel = new QLabel(this);
    m_pPosInfoWidget = new QWidget(this);

    m_pPosInfoLabel->setStyleSheet("color:rgb(200,255,200); "
                                   "background-color:rgba(50,50,50,160); "
                                   "font: Microsoft YaHei;"
                                   "font-size: 15px;");
    m_pPosInfoLabel->setText(" W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0");

    m_pPosInfoWidget->setFixedHeight(25);
    m_pPosInfoWidget->setGeometry(0, this->height() - 25, this->width(), 25);
    m_pPosInfoWidget->setStyleSheet("background-color:rgba(0,0,0,0);");
    QHBoxLayout* pInfoLayout = new QHBoxLayout();
    pInfoLayout->setSpacing(0);
    pInfoLayout->setContentsMargins(0, 0, 0, 0);
    pInfoLayout->addWidget(m_pPosInfoLabel);
    m_pPosInfoWidget->setLayout(pInfoLayout);

    connect(m_pImageItem, &ImageItem::RGBValue, this, [&](QString InfoVal) {
        m_pPosInfoLabel->setText(InfoVal);
    });

    return true;
}

void GraphicsView::SetImage(const QImage& image)
{
    // 只留尺寸供 fitFrame 换算，图像本体交给 QPixmap
    m_qImageSize = image.size();

    auto qPixmap = QPixmap::fromImage(image);
    m_pImageItem->w = qPixmap.width();
    m_pImageItem->h = qPixmap.height();
    m_pImageItem->setPixmap(qPixmap);

    fitFrame();
    OnCenter();
    show();
}

void GraphicsView::Clear()
{
    m_pPosInfoLabel->setText(" W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0");
    SetImage(QImage());
}

void GraphicsView::wheelEvent(QWheelEvent* event)
{

    QPoint scrollAmount = event->angleDelta();
    if ((scrollAmount.y() > 0) && (m_dZoomValue >= ZOOMMAX))
    {
        return;
    } else if ((scrollAmount.y() < 0) && (m_dZoomValue <= ZOOMMIN))
    {
        return;
    }

    scrollAmount.y() > 0 ? OnZoom(1.1) : OnZoom(0.9);
}

void GraphicsView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {

        fitFrame();

        OnCenter();
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsView::paintEvent(QPaintEvent* event)
{
    QPainter paint(this->viewport());

    paint.drawTiledPixmap(QRect(QPoint(0, 0), QPoint(this->width(), this->height())), m_qTilePixmap);
    QGraphicsView::paintEvent(event);
}

void GraphicsView::resizeEvent(QResizeEvent* event)
{
    fitFrame();
    OnCenter();
    m_pPosInfoWidget->setGeometry(0, this->height() - 25, this->width(), 25);
    QGraphicsView::resizeEvent(event);
}

void GraphicsView::OnCenter()
{

    this->centerOn(m_pImageItem->pixmap().width() / 2, m_pImageItem->pixmap().height() / 2);
    m_pImageItem->setPos(0, 0);
}

void GraphicsView::OnZoom(double scaleFactor)
{

    m_dZoomValue *= scaleFactor;

    this->scale(scaleFactor, scaleFactor);
}

void GraphicsView::fitFrame()
{
    if (this->width() < 1 || m_qImageSize.width() < 1)
        return;

    double winWidth = this->width();
    double winHeight = this->height();
    double ScaleWidth = (m_qImageSize.width() + 1) / winWidth;
    double ScaleHeight = (m_qImageSize.height() + 1) / winHeight;
    double s_temp = ScaleWidth >= ScaleHeight ? 1 / ScaleWidth : 1 / ScaleHeight;
    double scale = s_temp / m_dZoomValue;

    OnZoom(scale);
    m_dZoomValue = s_temp;
}

void GraphicsView::PrepareBackgroundBoard(bool invertColor)
{

    m_qTilePixmap.fill(invertColor ? QColor(220, 220, 220) : QColor(35, 35, 35));
    QPainter tilePainter(&m_qTilePixmap);
    constexpr QColor color(50, 50, 50, 255);
    constexpr QColor invertedColor(210, 210, 210, 255);
    tilePainter.fillRect(0, 0, 18, 18, invertColor ? invertedColor : color);
    tilePainter.fillRect(18, 18, 18, 18, invertColor ? invertedColor : color);
    tilePainter.end();

    // setBackgroundBrush(m_tilePixmap);
}

