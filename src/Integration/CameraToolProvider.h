#ifndef CAMERATOOLPROVIDER_H
#define CAMERATOOLPROVIDER_H

// 宿主适配层：把本客户端的相机能力暴露成 AI 可调用的工具。
//
// 【这一层为什么必须存在】Extension/ 那套通用框架（面板、会话、知识库、
//   多步循环）要能整包搬到别的客户端，就不能认识「相机」二字。
//   两边的接缝就是这里 —— 换一个客户端（PLC、运动控制卡、测试台上位机）时，
//   只要照这个文件新写一个实现（把该客户端已有的函数包成工具），
//   通用层一行都不用改。
//
// 【依赖方向】本文件依赖通用层接口，通用层不反向依赖它。
//   所以 src/Extension/ 整个目录是可以直接拷到别的项目里的。

#ifdef ZYCLEAR_HAS_AI

#include "Extension/Ai/ToolProviderInterface.h"

// 把相机能力包装成 AI 可以调用的工具。
//
// 【它演什么角色】翻译官。
//   门面 CameraContext 讲的是 Qt 语言（QString / QVariant / QVector），
//   大模型讲的是 JSON。这个类把两边的说法对上：
//     模型说 {"name":"ExposureTime","value":8000}
//       → 翻译成 CameraParam 能接受的对象
//       → 调门面写进设备
//       → 再把设备真值翻译回 JSON 交给模型
//
// 【依赖边界】只允许依赖门面 CameraContext，不许出现任何厂商类型，
//   也不许直接调 CameraInterface。这样 AI 层和界面层同级，
//   天然不感知品牌，也天然支持多相机（凭序列号寻址）。
class CameraToolProvider : public IToolProvider {
public:
    QString ProviderName() const override;

    QString ProviderDescription() const override;

    // 把所有相机能力注册进工具登记处
    void RegisterTools(agent4cpp::ToolRegistry& registry) override;

    QStringList ExamplePrompts() const override;
};

#endif // ZYCLEAR_HAS_AI

#endif
