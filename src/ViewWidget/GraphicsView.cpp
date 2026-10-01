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

    // 滚动条全关、场景矩形放大到 int 量级：平移交给 ScrollHandDrag，图元拖到
    // 任何位置都触不到场景边界（触到就会被 sceneRect 钳住），归位靠 OnCenter，
    // 反复缩放时也就不会闪出滚动条。
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->setRenderHint(QPainter::Antialiasing);
    // 缩放锚定视口中心，配合 OnCenter 复位，避免光标位置牵动画面漂移
    this->setTransformationAnchor(QGraphicsView::AnchorViewCenter);

    // paintEvent 自铺棋盘底纹，局部更新会在底纹上留下前后帧拼接的残块
    this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::ScrollHandDrag);
    this->setSceneRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
    PrepareBackgroundBoard();
    centerOn(0, 0);

    // InitWidget 目前恒返回 true，这个 bad_alloc 只是兜底，不会真的触发
    if (false == InitWidget()) {
        throw std::bad_alloc();
    }
}

GraphicsView::~GraphicsView()
{
    // 回收全部交给 Qt 的父子关系：scene 以 this 为 parent（见 InitWidget），
    // 图元在 addItem 时已移交给 scene，本对象析构时这条链会自动走完。
    // 这里刻意不做任何显式 delete —— 原先的 scene->deleteLater() 把删除推迟到
    // 事件循环（那时本对象可能已销毁），delete m_pImageItem 又与 scene 的所有权
    // 重复，两行合起来构成一次二次删除。
}

bool GraphicsView::InitWidget()
{

    // m_pImageItem 以 this 为 parent 传入，但那个参数在 ImageItem 里并未使用，
    // 图元实际由 scene 的 addItem 接管所有权（析构顺序见 ~GraphicsView）。
    m_pScene = new QGraphicsScene(this);
    m_pImageItem = new ImageItem(this);
    //    m_pImageItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    this->setScene(m_pScene);
    m_pScene->addItem(m_pImageItem);
    // 坐标信息做成视图的子控件而非 scene 里的图元：它要固定在视口底部，
    // 不能跟着缩放和平移跑，所以几何位置得由 resizeEvent 手工重算。
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

    // 信号里带的是拼好的整串文本，视图只负责显示：字段顺序与单位的口径
    // 定在图元那一侧，避免以后想换显示格式要同时改两个文件。
    connect(m_pImageItem, &ImageItem::RGBValue, this, [&](QString InfoVal) {
        m_pPosInfoLabel->setText(InfoVal);
    });

    return true;
}

void GraphicsView::SetImage(const QImage& image)
{
    // 只留尺寸供 fitFrame 换算，图像本体交给 QPixmap
    m_qImageSize = image.size();

    // 跨线程投递来的只能是 QImage（QPixmap 依赖 GUI 线程的绘图后端，别处
    // 构造会直接崩），所以转换必须落在本槽内；w/h 顺手存货给图元，供悬停
    // 时的坐标与尺寸显示用。
    auto qPixmap = QPixmap::fromImage(image);
    m_pImageItem->w = qPixmap.width();
    m_pImageItem->h = qPixmap.height();
    m_pImageItem->setPixmap(qPixmap);

    // 每帧都回一次适配视图：好处是换分辨率、换相机后必然看得见全景；
    // 代价是持续拉流期间用户自己做的缩放与平移会被下一帧冲掉。
    fitFrame();
    OnCenter();
    show();
}

void GraphicsView::Clear()
{
    // 清图像同时要把位姿归零：空图下 fitFrame 会因宽高为 0 提前返回，
    // 但 OnCenter 仍会走一遍，把上一台相机留下的平移量抹掉。
    m_pPosInfoLabel->setText(" W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0");
    SetImage(QImage());
}

void GraphicsView::wheelEvent(QWheelEvent* event)
{

    // 上下限拿自己累计的 m_dZoomValue 判，不回读 transform()：QGraphicsView
    // 的缩放是乘性的，反复缩放后矩阵里的值会漂，拿它跟阈值比容易提前锁死。
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
    // 双击即"看清全图"：把缩放与平移都拨回默认；基类调用留最后，保持
    // Qt 自身处理链完整。
    if (event->button() == Qt::LeftButton) {

        fitFrame();

        OnCenter();
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsView::paintEvent(QPaintEvent* event)
{
    // 底纹是"没有图像的地方"的视觉基准，必须赶在基类绘制之前铺满视口
    QPainter paint(this->viewport());

    paint.drawTiledPixmap(QRect(QPoint(0, 0), QPoint(this->width(), this->height())), m_qTilePixmap);
    QGraphicsView::paintEvent(event);
}

void GraphicsView::resizeEvent(QResizeEvent* event)
{
    // 窗口尺寸变了就重新适配，否则图像会缩在旧尺寸对应的角落里；
    // 坐标信息条是贴底的手工定位控件，这里必须跟着一起挪。
    fitFrame();
    OnCenter();
    m_pPosInfoWidget->setGeometry(0, this->height() - 25, this->width(), 25);
    QGraphicsView::resizeEvent(event);
}

void GraphicsView::OnCenter()
{

    // 先让视口对准图元中心，再把图元挪回场景原点：两步顺序不能反，
    // 否则 setPos 之后中心点变了，画面会整体偏掉。这个组合同时把用户
    // 拖拽累积的平移量归零。
    this->centerOn(m_pImageItem->pixmap().width() / 2, m_pImageItem->pixmap().height() / 2);
    m_pImageItem->setPos(0, 0);
}

void GraphicsView::OnZoom(double scaleFactor)
{

    // 传入相对倍率：记账与变换必须同步，否则 fitFrame 的补偿会算偏
    m_dZoomValue *= scaleFactor;

    this->scale(scaleFactor, scaleFactor);
}

void GraphicsView::fitFrame()
{
    if (this->width() < 1 || m_qImageSize.width() < 1)
        return;

    // 目标是"绝对适配倍率"，但 QGraphicsView::scale 只能做相对乘法，所以先算
    // 目标与当前 m_dZoomValue 的比值再乘上去。加 1 是防边界：图像宽高恰好
    // 等于窗口时除法会放大出微小溢出，留下一条缝。
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

    // 一整块 36×36 里画两个 18×18 方格，平铺即成棋盘。不用 setBackgroundBrush
    // 是因为它按场景坐标平铺，拖动时底纹会跟着滑走；这里要的是相对屏幕固定
    // 的底纹，所以留给 paintEvent 逐帧平铺。invertColor 为浅色主题预留。
    m_qTilePixmap.fill(invertColor ? QColor(220, 220, 220) : QColor(35, 35, 35));
    QPainter tilePainter(&m_qTilePixmap);
    constexpr QColor color(50, 50, 50, 255);
    constexpr QColor invertedColor(210, 210, 210, 255);
    tilePainter.fillRect(0, 0, 18, 18, invertColor ? invertedColor : color);
    tilePainter.fillRect(18, 18, 18, 18, invertColor ? invertedColor : color);
    tilePainter.end();

    // setBackgroundBrush(m_tilePixmap);
}

