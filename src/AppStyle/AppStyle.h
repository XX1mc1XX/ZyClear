#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QApplication>

class AppStyle {
public:
    AppStyle() { };

    // 唯一的对外入口，整进程调用一次即可：调色板与样式表都设在 qApp 上，
    // 是进程级设置，之后新建的控件自动继承。当前由 MainWindow 构造函数
    // 在全部控件装配完之后调用。
    static void Polish();

private:

    static void setPalette();

    static void setQss();
};

#endif

