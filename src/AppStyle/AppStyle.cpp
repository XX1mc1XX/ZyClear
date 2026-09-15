#include "AppStyle/AppStyle.h"

#include <QFile>
#include <QStyle>
#include <QStyleFactory>

void AppStyle::Polish()
{
    setPalette();
    setQss();
}

void AppStyle::setPalette()
{
    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setPalette(QApplication::style()->standardPalette());

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(200, 200, 200));
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
        QColor(127, 127, 127));
    palette.setColor(QPalette::Base, QColor(246, 246, 246));
    palette.setColor(QPalette::AlternateBase, QColor(64, 157, 224));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::black);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Dark, QColor(72, 72, 72));
    palette.setColor(QPalette::Shadow, Qt::black);
    palette.setColor(QPalette::Mid, QColor(77, 77, 77));
    palette.setColor(QPalette::Button, QColor(88, 88, 88));
    palette.setColor(QPalette::Light, QColor(98, 98, 98));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
        QColor(127, 127, 127));
    palette.setColor(QPalette::BrightText, QColor(247, 181, 84));
    palette.setColor(QPalette::Link, QColor(222, 137, 10));
    palette.setColor(QPalette::Highlight, QColor(246, 134, 86));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
        QColor(127, 127, 127));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
        QColor(127, 127, 127));

    qApp->setPalette(palette);
}

void AppStyle::setQss()
{

    QFile qssfile(":/zyClear.qss");
    if (qssfile.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(qssfile.readAll());
        qssfile.close();
    }
}

