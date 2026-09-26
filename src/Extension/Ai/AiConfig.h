#ifndef AICONFIG_H
#define AICONFIG_H

#include <QString>

// AI 助手配置。
//
// 【存在哪】全部落在本机 QSettings（Windows 上是注册表 HKEY_CURRENT_USER），
//   不进源码、不进仓库。API Key 一旦提交到 Git 就等于公开，
//   别人可以拿你的账号花钱。
//
// 【为什么单独一个类】配置的读取/写入/校验各只有一个出口，
//   面板、服务、对话框都从这里取，避免三处各写一遍 QSettings 键名。
struct AiConfig {
    // 模型服务基地址。代码会在后面自动拼 "/chat/completions"
    QString baseUrl;

    // 模型名，例如 deepseek-chat / glm-4-plus
    QString model;

    // API Key。只存本机
    QString apiKey;

    // 随机性。0.2 偏低是故意的：这个场景要的是「稳定地选对工具」，
    // 不是「有创意」。温度调高会导致模型这次调 set_param、下次自作主张调别的
    double temperature { 0.2 };

    // 单次回复最长 token 数，同时控制费用
    int maxTokens { 1024 };

    // 网络超时（毫秒）
    int timeoutMs { 60000 };

    // 默认配置（首次使用时给的值）
    static AiConfig Default();

    // 从本机读配置；没有存过则返回 Default()
    static AiConfig Load();

    // 写回本机
    void Save() const;

    // 三项必填项齐了才算配置完整
    bool IsValid() const;

    // agent4cpp 只接受「环境变量名」，不接受 Key 本身（防止 Key 写进配置文件和代码）。
    // 所以这里把界面填的 Key 注入一个进程内环境变量，再把变量名交给 agent4cpp。
    // 返回注入的环境变量名。
    QString ApplyApiKeyToEnv() const;

    // 注入 API Key 时用的环境变量名，固定值
    static QString ApiKeyEnvName();
};

#endif
