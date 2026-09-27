#include "LoadingDialog.h"
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

// 文件作用域的单例句柄，「最多一个加载框」这条不变式全靠它维持。
// 有意不加 static：没打算给它内部链接。它非线程安全，只允许在 GUI 线程上碰。
LoadingDialog* s_LoadingWidget = Q_NULLPTR;

LoadingDialog::LoadingDialog(QWidget* parent)
    : QDialog(parent)
    , m_pCloseButton(nullptr)
    , m_pGifLabel(nullptr)
    , m_pLoadingMovie(nullptr)
{
    setupUi();

    // 去边框 + 透明底：可见外观全部由 loading.gif 自己提供，
    // 于是下面的 setFixedSize 必须与 gif 的缩放尺寸一致，
    // 两边对不上就会把动图裁掉一块或留出透明边。
    setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    this->setFixedSize(250, 180);

    // QMovie 没有父对象，QLabel::setMovie 也不接管它的所有权，
    // 因此 HideLoading 删除对话框时这里不会被释放 —— 每开关一次泄漏一个。
    m_pLoadingMovie = new QMovie(":/loading.gif");
    m_pLoadingMovie->setScaledSize(QSize(250, 180));
    m_pGifLabel->setMovie(m_pLoadingMovie);
    m_pLoadingMovie->start();
}

void LoadingDialog::setupUi()
{
    // .ui 导出时留下的尺寸，已被构造函数里的 setFixedSize 覆盖，不生效。
    resize(451, 397);
    setWindowTitle(tr("Dialog"));

    QVBoxLayout* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName("verticalLayout");

    QWidget* topWidget = new QWidget(this);
    topWidget->setObjectName("widget");
    QHBoxLayout* horizontalLayout = new QHBoxLayout(topWidget);
    horizontalLayout->setObjectName("horizontalLayout");
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    horizontalLayout->addItem(new QSpacerItem(388, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_pCloseButton = new QPushButton(topWidget);
    m_pCloseButton->setObjectName("Close_Button");
    m_pCloseButton->setStyleSheet(QStringLiteral("#Close_Button{\n"
                                                 "  background-color:none;\n"
                                                 "  border:none;\n"
                                                 "  width:28px;\n"
                                                 "  height:28px;\n"
                                                 "  padding:4px;\n"
                                                 "  image:url(:/Close.png)\n"
                                                 "}\n"
                                                 "#Close_Button:hover{\n"
                                                 "  background-color:rgb(247,181,84);\n"
                                                 "}\n"
                                                 "#Close_Button:pressed{\n"
                                                 "  background-color:none;\n"
                                                 "}"));
    horizontalLayout->addWidget(m_pCloseButton);
    verticalLayout->addWidget(topWidget);

    m_pGifLabel = new QLabel(this);
    m_pGifLabel->setObjectName("Gif_label");
    verticalLayout->addWidget(m_pGifLabel);

    connect(m_pCloseButton, &QPushButton::clicked, this, &LoadingDialog::on_Close_Button_clicked);
}

LoadingDialog::~LoadingDialog()
{
}

void LoadingDialog::Loading(QWidget* parent)
{
    // 复用已有实例：parent 只在第一次构造时生效，后续调用方传进来的会被忽略。
    // 这是单例换来的代价，也意味着加载框可能落在别的窗口上方。
    if (Q_NULLPTR == s_LoadingWidget) {
        s_LoadingWidget = new LoadingDialog(parent);
    }

    s_LoadingWidget->setModal(true);
    s_LoadingWidget->show();
    // 调用方紧接着执行的是同步阻塞的 SDK 调用，不在这里主动跑一次事件循环，
    // 上面 show() 排队的重绘就永远轮不到执行，框根本画不出来。
    QApplication::processEvents();
}

void LoadingDialog::HideLoading()
{
    // 无条件删除而不是 hide()：加载框不承载任何状态，留着只会让下次复用变复杂。
    // 用户若已手动关过，这里删除同样安全 —— close() 只是隐藏，对象还活着。
    if (s_LoadingWidget) {
        s_LoadingWidget->close();
        delete s_LoadingWidget;
        s_LoadingWidget = Q_NULLPTR;
    }
}

void LoadingDialog::on_Close_Button_clicked()
{
    // 只关不删：单例指针仍指着这个已隐藏的实例，下次 Loading() 会把它
    // show() 回来、动图从原进度接着播；真正的销毁只在 HideLoading() 里发生。
    this->close();
}

