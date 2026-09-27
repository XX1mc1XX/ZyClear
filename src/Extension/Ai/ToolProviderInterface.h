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

    virtual QString ProviderName() const = 0;

    // 会拼进系统提示词，写具体些，模型才知道哪类问题该走工具
    virtual QString ProviderDescription() const = 0;

    virtual void RegisterTools(agent4cpp::ToolRegistry& registry) = 0;

    virtual QStringList ExamplePrompts() const = 0;
};

#endif
