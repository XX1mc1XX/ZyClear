#include "LoadingDialog.h"
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

LoadingDialog* s_LoadingWidget = Q_NULLPTR;

LoadingDialog::LoadingDialog(QWidget* parent)
    : QDialog(parent)
    , m_pCloseButton(nullptr)
    , m_pGifLabel(nullptr)
    , m_pLoadingMovie(nullptr)
{
    setupUi();

    setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    this->setFixedSize(250, 180);

    m_pLoadingMovie = new QMovie(":/loading.gif");
    m_pLoadingMovie->setScaledSize(QSize(250, 180));
    m_pGifLabel->setMovie(m_pLoadingMovie);
    m_pLoadingMovie->start();
}

void LoadingDialog::setupUi()
{
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
    if (Q_NULLPTR == s_LoadingWidget) {
        s_LoadingWidget = new LoadingDialog(parent);
    }

    s_LoadingWidget->setModal(true);
    s_LoadingWidget->show();
    QApplication::processEvents();
}

void LoadingDialog::HideLoading()
{
    if (s_LoadingWidget) {
        s_LoadingWidget->close();
        delete s_LoadingWidget;
        s_LoadingWidget = Q_NULLPTR;
    }
}

void LoadingDialog::on_Close_Button_clicked()
{
    this->close();
}

