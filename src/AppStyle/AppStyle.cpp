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
    // 两句的顺序不能反，setStyle 也不能省：第二句要的就是 Fusion 的标准盘。
    // 之所以必须锁死 Fusion 而不是让平台风格留任 —— 下面这个自定义盘只设了它关心的
    // 那些角色，其余角色最终会回落到「当前风格的标准盘」，而 Windows 原生风格的盘
    // 是浅色的，跟这套深色配色拼在一起就是花的。
    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setPalette(QApplication::style()->standardPalette());

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 30));
    palette.setColor(QPalette::WindowText, QColor(204, 204, 204));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
        QColor(133, 133, 133));
    palette.setColor(QPalette::Base, QColor(37, 37, 38));
    palette.setColor(QPalette::AlternateBase, QColor(42, 45, 46));
    palette.setColor(QPalette::ToolTipBase, QColor(37, 37, 38));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, QColor(204, 204, 204));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(133, 133, 133));
    palette.setColor(QPalette::Dark, QColor(24, 24, 24));
    palette.setColor(QPalette::Shadow, Qt::black);
    palette.setColor(QPalette::Mid, QColor(60, 60, 60));
    palette.setColor(QPalette::Button, QColor(51, 51, 51));
    palette.setColor(QPalette::Light, QColor(60, 60, 60));
    palette.setColor(QPalette::ButtonText, QColor(204, 204, 204));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
        QColor(133, 133, 133));
    palette.setColor(QPalette::BrightText, QColor(0, 127, 212));
    palette.setColor(QPalette::Link, QColor(0, 127, 212));
    palette.setColor(QPalette::Highlight, QColor(4, 57, 94));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
        QColor(60, 60, 60));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
        QColor(133, 133, 133));

    // Disabled 组单独指定：深色底上禁用态若沿用自动派生的颜色几乎读不出来，
    // 这里统一压到 #858585 这一档，保证「看得出是灰的、但仍能读」。
    qApp->setPalette(palette);
}

void AppStyle::setQss()
{

    // 样式表编在二进制里（:/ 前缀是 Qt 资源），所以部署时没有外置皮肤文件依赖。
    // 反过来说，qrc 里漏掉这个文件时这里只会静默失败 —— 界面退化成「只有调色板、
    // 没有圆角与贴图」的样子，不报任何错。排查外观问题时先确认资源有没有编进去。
    QFile qssfile(":/zyClear.qss");
    if (qssfile.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(qssfile.readAll());
        qssfile.close();
    }
}

