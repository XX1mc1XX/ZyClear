#ifndef EXTENSIONINTERFACE_H
#define EXTENSIONINTERFACE_H

#include <QString>
#include <Qt>

class QWidget;

class IPanel {
public:
    // 主程序只经 IPanel* 持有面板，销毁时走这个虚入口；非虚会只跑基类析构，
    // 插件在 CreateWidget 里自建、没交给 Qt 父子关系的资源会全漏。
    virtual ~IPanel() { }

    // 唯一标识，同名注册会被拒绝
    virtual QString PanelId() const = 0;

    // 同一个标题会被 dock 标题、视图菜单项、工具栏提示三处共用，写短一点，
    // 别在这里塞说明性文字。
    virtual QString PanelTitle() const = 0;

    // parent 由宿主传入，生命周期交给 Qt 的父子关系管理
    virtual QWidget* CreateWidget(QWidget* parent) = 0;

    // 决定面板挂上去时停靠在哪一侧，用户之后拖到别处不受影响。
    virtual Qt::DockWidgetArea DefaultArea() const
    {
        return Qt::RightDockWidgetArea;
    }

    // 挂载时的初始开合；用户关掉不写回配置，下次启动仍是这个初始值。
    virtual bool VisibleByDefault() const
    {
        return true;
    }
};

// 插件接口只用裸 C 函数：C++ 类没有稳定的二进制布局，换编译器或 STL 版本就对不上。
// 代价是插件与主程序必须用同一套编译器与运行库。

// extern "C" 还负责去掉 C++ 名字修饰：导出符号在 DLL 里就是 ZyClearExtensionAt 这个原名。
// 若按 C++ 链接，名字会被编码成带参数类型的长串，插件换个编译器版本宿主就 resolve 不到。

#if defined(_WIN32)
#define ZYCLEAR_EXTENSION_EXPORT extern "C" __declspec(dllexport)
#else
#define ZYCLEAR_EXTENSION_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define ZYCLEAR_EXTENSION_COUNT_FN "ZyClearExtensionCount"
#define ZYCLEAR_EXTENSION_AT_FN "ZyClearExtensionAt"

// 这两个导出名、以及 At 的「收 index 返回 IPanel*」签名，是宿主与插件之间唯一的硬契约。
// 改名或改签名不会在编译期报错，只会让老 DLL 在运行期静默错调，动之前先想兼容。

#endif
