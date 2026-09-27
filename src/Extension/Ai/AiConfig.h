#ifndef AICONFIG_H
#define AICONFIG_H

#include <QString>

// 配置存本机 QSettings，不入源码库
struct AiConfig {
    QString baseUrl;

    QString model;

    // 明文落在用户目录的 QSettings 里，不走系统密钥链：后者要额外依赖和平台分支，
    // 这里换来的折中是文件只在用户 profile 下，且绝不随源码或仓库分发
    QString apiKey;

    // 偏低是故意的：要的是稳定选对工具，温度调高会让模型乱调工具
    double temperature { 0.2 };

    int maxTokens { 1024 };

    // 单次 HTTP 请求的超时，不是整轮问答的上限：模型分多步调工具时每步各自计时
    int timeoutMs { 60000 };

    static AiConfig Default();

    // 每次重建 Agent 都会重新读盘，所以改完设置只要触发一次重建就生效，不用重启进程
    static AiConfig Load();

    void Save() const;

    // 只校验能不能建起客户端的三项；温度、长度、超时都有默认值，缺了也照样可用
    bool IsValid() const;

    // agent4cpp 只接受环境变量名而非 Key 本身，这里把 Key 注入进程内环境变量再返回变量名
    QString ApplyApiKeyToEnv() const;

    static QString ApiKeyEnvName();
};

#endif
