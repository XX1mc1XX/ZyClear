#ifndef TOOLPROVIDERINTERFACE_H
#define TOOLPROVIDERINTERFACE_H

#include <QString>
#include <QStringList>

namespace agent4cpp {
class ToolRegistry;
}

// =============================================================================
// 宿主能力提供者（插件与宿主之间的唯一接口）
//
// 【为什么需要它】
//   这一整套 AI 助手要能当「寄生插件」用：搬到另一个客户端（PLC 客户端、
//   运动控制卡客户端、测试台上位机）时，理想情况是整包拷过去就能跑。
//
//   要做到这一点，通用层就不能认识「相机」二字。两边靠这个接口对接：
//     宿主实现它，说明「我这个客户端能做什么」；
//     AI 层只认这个接口，把宿主能力和它自带的工具一起注册给模型。
//
// 【依赖方向】宿主 → 接口 ← AI 层
//   宿主当然要依赖通用层（是宿主负责装配），但通用层绝不反向依赖宿主。
//   所以 src/Extension/ 整个目录可以直接复制到别的项目，只需要新写一个
//   实现类（几十行：把该客户端已有的函数包成工具）。
//
// 【一个客户端可以挂多个提供者】
//   例如相机客户端可以同时提供「相机控制」和「配方管理」两组能力。
// =============================================================================

class IToolProvider {
public:
    virtual ~IToolProvider() { }

    // 能力组的名字，用于日志与界面提示，例如「工业相机」
    virtual QString ProviderName() const = 0;

    // 一句话说明能做什么。这段文字会拼进系统提示词 ——
    // 写得越具体，模型越清楚哪类问题该走工具、哪类问题可以直接回答。
    virtual QString ProviderDescription() const = 0;

    // 把宿主能力注册成 agent4cpp 的工具。
    // 实现在宿主侧，所以暴露哪些能力、粒度多细，完全由宿主决定。
    virtual void RegisterTools(agent4cpp::ToolRegistry& registry) = 0;

    // 界面上给用户的示例问法，显示在 AI 面板的欢迎语里。
    // 让用户一眼知道「这个助手能替我干什么」。
    virtual QStringList ExamplePrompts() const = 0;
};

#endif
