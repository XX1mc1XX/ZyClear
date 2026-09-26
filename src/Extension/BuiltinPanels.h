#ifndef BUILTINPANELS_H
#define BUILTINPANELS_H

#include "Ai/ToolProviderInterface.h"

#include <QList>

class PanelRegistry;

// =============================================================================
// 内置面板的装配入口
//
// 【为什么单独一个文件】
//   PanelRegistry 只该管「登记与创建」这套机制，不该认识任何具体面板 ——
//   否则每加一个面板都要改注册处，机制和内容就搅在一起了。
//   这里集中放内置面板的登记，注册处保持干净。
//
// 【providers 从哪来】宿主注入。AI 面板本身不知道「相机」是什么，
//   它只认 IToolProvider；宿主把自己的能力包好交进来。
//   所以换客户端时，这个文件一行都不用改。
// =============================================================================

void RegisterBuiltinPanels(PanelRegistry* registry, const QList<IToolProvider*>& providers);

#endif
