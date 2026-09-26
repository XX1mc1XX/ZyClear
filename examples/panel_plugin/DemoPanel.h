// =============================================================================
// DemoPanel.h —— 外部面板插件的示例
//
// 这个文件演示「一切皆可导入」到底怎么写：
//   继承宿主的 IPanel 接口，实现四个方法，然后在入口文件里导出两个 C 函数。
//   编成 DLL 丢进宿主程序目录下的 extensions/ 就能被自动加载。
//
// 【它只依赖一个头文件】ExtensionInterface.h。
//   不需要宿主的任何其他代码，也不需要链接宿主的库 ——
//   这正是插件与宿主隔离的意义：换一个宿主，插件照样能编。
// =============================================================================

#ifndef DEMOPANEL_H
#define DEMOPANEL_H

#include "ExtensionInterface.h"

class DemoPanel : public IPanel {
public:
    QString PanelId() const override;

    QString PanelTitle() const override;

    QWidget* CreateWidget(QWidget* parent) override;

    Qt::DockWidgetArea DefaultArea() const override;

    bool VisibleByDefault() const override;
};

#endif
