#ifndef EXTENSIONHOST_H
#define EXTENSIONHOST_H

class QMainWindow;
class PanelRegistry;

// 扩展宿主：把登记在册的面板挂到主窗口上。
// 主窗口只调一次 Attach，不关心有哪些面板。
//
// 用 QDockWidget 而不是 splitter：splitter 的每一栏都得一直占着宽度，
// 而 AI 助手和日志属于用时才打开的东西。QDockWidget 支持停靠到任意边、
// 浮动成独立窗口、右上角关闭、从菜单重新打开。
class ExtensionHost {
public:
    // 同一窗口重复调用是安全的：工具栏、视图菜单、重排控制器都按 objectName 复用，
    // 不会挂出第二套。造不出界面的面板被跳过，返回值里已经扣掉。
    // 会顺带建一个「视图」菜单，用于重新打开被关掉的面板。返回挂上去的面板数
    static int Attach(QMainWindow* window, PanelRegistry* registry);
};

#endif
