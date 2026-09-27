#ifndef CAMERATOOLPROVIDER_H
#define CAMERATOOLPROVIDER_H

// 宿主适配层：把 CameraContext 的相机能力包成 AI 工具；只依赖门面接口，
// 不出现厂商类型，Extension/ 通用层因此可整包搬到别的客户端。
#ifdef ZYCLEAR_HAS_AI

#include "Extension/Ai/ToolProviderInterface.h"

class CameraToolProvider : public IToolProvider {
public:
    QString ProviderName() const override;

    QString ProviderDescription() const override;

    void RegisterTools(agent4cpp::ToolRegistry& registry) override;

    QStringList ExamplePrompts() const override;
};

#endif // ZYCLEAR_HAS_AI

#endif
