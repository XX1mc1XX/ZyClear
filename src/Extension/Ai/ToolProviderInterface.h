#ifndef TOOLPROVIDERINTERFACE_H
#define TOOLPROVIDERINTERFACE_H

#include <QString>
#include <QStringList>

namespace agent4cpp {
class ToolRegistry;
}

// 宿主实现本接口把自身能力注册成工具；通用层只依赖这个接口，不反向依赖宿主

class IToolProvider {
public:
    virtual ~IToolProvider() { }

    // 会作为系统提示词里「可操作对象」的分节标题，同一份提示词内不能重名
    virtual QString ProviderName() const = 0;

    // 会拼进系统提示词，写具体些，模型才知道哪类问题该走工具
    virtual QString ProviderDescription() const = 0;

    // 注册进去的回调会在后台线程执行（问答本身不占主线程），要碰界面或相机句柄的实现
    // 必须自己切回主线程；接口这层刻意不给线程亲和参数，否则通用层就得认识宿主的线程规矩
    virtual void RegisterTools(agent4cpp::ToolRegistry& registry) = 0;

    // 示例跟着实际注册的工具一起变，所以由提供者自己给，面板只负责转述
    virtual QStringList ExamplePrompts() const = 0;
};

#endif
