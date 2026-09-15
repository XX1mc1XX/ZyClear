#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QApplication>

class AppStyle {
public:
    AppStyle() { };

    static void Polish();

private:

    static void setPalette();

    static void setQss();
};

#endif

