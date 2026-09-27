// 导出 C 函数而非类：C++ 类无稳定 ABI，换编译器或 STL 版本就对不上
// 插件与宿主必须同一套编译器和 Qt，否则传递 QWidget* 会出问题

#include "DemoPanel.h"

extern "C" ZYCLEAR_EXTENSION_EXPORT int ZyClearExtensionCount()
{
    return 1;
}

extern "C" ZYCLEAR_EXTENSION_EXPORT IPanel* ZyClearExtensionAt(int index)
{
    if (index == 0) {
        return new DemoPanel();
    }
    return nullptr;
}
