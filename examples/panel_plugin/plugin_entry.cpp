// =============================================================================
// plugin_entry.cpp —— 插件的导出入口
//
// 宿主不认识插件的类，只认这两个 C 函数：
//   ZyClearExtensionCount()  这个 dll 里装了几个面板
//   ZyClearExtensionAt(i)    第 i 个面板是什么（每次都要返回新实例）
//
// 【为什么用 C 函数而不是直接导出类】
//   C++ 的类没有稳定的二进制布局：换编译器、换运行库、换 STL 版本都可能对不上。
//   裸的 C 签名在所有编译器下都一致，是插件接口的通行做法。
//
// 【硬性要求】插件与宿主必须用同一套编译器和运行库（本项目两边都是 MSVC +
//   同一份 Qt），否则即使能加载，传递 QWidget* 之类的对象也会出问题。
// =============================================================================

#include "DemoPanel.h"

extern "C" ZYCLEAR_EXTENSION_EXPORT int ZyClearExtensionCount()
{
    return 1;
}

extern "C" ZYCLEAR_EXTENSION_EXPORT IPanel* ZyClearExtensionAt(int index)
{
    // 每次调用都造一个新的：宿主可能反复开关这个面板
    if (index == 0) {
        return new DemoPanel();
    }
    return nullptr;
}
