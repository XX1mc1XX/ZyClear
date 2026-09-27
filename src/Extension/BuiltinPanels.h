#ifndef BUILTINPANELS_H
#define BUILTINPANELS_H

#include "Ai/ToolProviderInterface.h"

#include <QList>

class PanelRegistry;

// 内置面板的装配点：把随主程序一起编译进来的面板登记进注册表，
// 与 extensions/ 目录下外部 DLL 走的是同一条注册通道。
//
// providers 会被存进 AI 面板并持有到程序结束，只存裸指针、不做深拷贝，
// 传进来的对象必须比面板活得久（mainwindow 里用 static 局部对象正是为此）。
// 内置面板先于外部插件登记，同 id 的外部 DLL 会被 Register 拒掉。
void RegisterBuiltinPanels(PanelRegistry* registry, const QList<IToolProvider*>& providers);

#endif
