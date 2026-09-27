#ifndef EXTENSIONINTERFACE_H
#define EXTENSIONINTERFACE_H

#include <QString>
#include <Qt>

class QWidget;

class IPanel {
public:
    virtual ~IPanel() { }

    // 唯一标识，同名注册会被拒绝
    virtual QString PanelId() const = 0;

    virtual QString PanelTitle() const = 0;

    // parent 由宿主传入，生命周期交给 Qt 的父子关系管理
    virtual QWidget* CreateWidget(QWidget* parent) = 0;

    virtual Qt::DockWidgetArea DefaultArea() const
    {
        return Qt::RightDockWidgetArea;
    }

    virtual bool VisibleByDefault() const
    {
        return true;
    }
};

// 插件接口只用裸 C 函数：C++ 类没有稳定的二进制布局，换编译器或 STL 版本就对不上。
// 代价是插件与主程序必须用同一套编译器与运行库。

#if defined(_WIN32)
#define ZYCLEAR_EXTENSION_EXPORT extern "C" __declspec(dllexport)
#else
#define ZYCLEAR_EXTENSION_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define ZYCLEAR_EXTENSION_COUNT_FN "ZyClearExtensionCount"
#define ZYCLEAR_EXTENSION_AT_FN "ZyClearExtensionAt"

#endif
