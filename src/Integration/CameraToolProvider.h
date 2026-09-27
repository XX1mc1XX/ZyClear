#ifndef CAMERATOOLPROVIDER_H
#define CAMERATOOLPROVIDER_H

// 宿主适配层：把 CameraContext 的相机能力包成 AI 工具；只依赖门面接口，
// 不出现厂商类型，Extension/ 通用层因此可整包搬到别的客户端。
#ifdef ZYCLEAR_HAS_AI

#include "Extension/Ai/ToolProviderInterface.h"

// 整个类随 ZYCLEAR_HAS_AI 一起开关。agent4cpp 是可选闭源依赖，没链上时
// 这个头根本不会被 include，编译产物只是少一块 AI 能力，其余照常。
class CameraToolProvider : public IToolProvider {
public:
    QString ProviderName() const override;

    QString ProviderDescription() const override;

    // 注册进去的是「描述 + JSON Schema + 本地回调」；registry 归调用方所有，
    // 这里只在注册期间用一下，不保存引用。
    void RegisterTools(agent4cpp::ToolRegistry& registry) override;

    QStringList ExamplePrompts() const override;
};

#endif // ZYCLEAR_HAS_AI

#endif
