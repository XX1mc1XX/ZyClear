#ifndef EXTENSIONINTERFACE_H
#define EXTENSIONINTERFACE_H

#include <QString>
#include <Qt>

class QWidget;

// =============================================================================
// 可扩展面板的契约层
//
// 【它解决什么问题】
//   主窗口不该认识任何一个具体面板。AI 助手、日志、以后可能有的标定面板，
//   对主窗口来说都只是「一个能收起的停靠面板」。新增面板 = 新增一个实现 +
//   注册一行，主窗口一行都不用改。
//
// 【和相机适配层同一个套路】
//   CameraInterface 定义硬件无关的能力边界，HikCamera / VirtualCamera 各自实现；
//   这里 IPanel 定义面板无关的能力边界，AI 面板 / 日志面板各自实现。
//   两处的依赖方向也一样：契约不认具体实现。
//
// 【怎么接入】两个办法，都不需要改主窗口：
//   1. 内置：实现 IPanel，在 PanelRegistry::RegisterBuiltin 里注册一行
//   2. 外部 DLL：实现 IPanel 并导出两个 C 函数（见文件末尾约定），
//      运行时用 PanelRegistry::LoadFromLibrary 加载
// =============================================================================

class IPanel {
public:
    virtual ~IPanel() { }

    // 唯一标识。同名注册会被拒绝，避免两个面板互相覆盖
    virtual QString PanelId() const = 0;

    // 停靠面板的标题，也是标签页上的文字
    virtual QString PanelTitle() const = 0;

    // 造出面板内容。parent 由宿主传入，生命周期交给 Qt 的父子关系管理
    virtual QWidget* CreateWidget(QWidget* parent) = 0;

    // 默认停靠到哪一侧。用户之后可以自己拖到别处
    virtual Qt::DockWidgetArea DefaultArea() const
    {
        return Qt::RightDockWidgetArea;
    }

    // 首次启动是否可见。关掉的面板可以从菜单里再打开
    virtual bool VisibleByDefault() const
    {
        return true;
    }
};

// =============================================================================
// 外部 DLL 面板的导出约定
//
// 插件 DLL 只要导出下面两个 C 函数，就能被 PanelRegistry::LoadFromLibrary 认出来。
//
// 【为什么用 C 函数而不是直接导出类】
//   C++ 的类没有稳定的二进制布局：换编译器、换运行库、换 STL 版本都可能对不上。
//   裸的 C 函数签名在所有编译器下都一致，是插件接口的通行做法。
//   代价是插件和主程序必须用同一套编译器与运行库（本项目的构建链已固定）。
//
// 插件写法：
//   extern "C" ZYCLEAR_EXTENSION_EXPORT int ZyClearExtensionCount() { return 1; }
//   extern "C" ZYCLEAR_EXTENSION_EXPORT IPanel* ZyClearExtensionAt(int index)
//   {
//       return index == 0 ? new MyPanel() : nullptr;
//   }
// =============================================================================

#if defined(_WIN32)
#define ZYCLEAR_EXTENSION_EXPORT extern "C" __declspec(dllexport)
#else
#define ZYCLEAR_EXTENSION_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// 插件必须导出的两个入口，名字固定
#define ZYCLEAR_EXTENSION_COUNT_FN "ZyClearExtensionCount"
#define ZYCLEAR_EXTENSION_AT_FN "ZyClearExtensionAt"

#endif
