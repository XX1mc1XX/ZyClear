#ifndef AICONFIG_H
#define AICONFIG_H

#include <QString>

// 配置存本机 QSettings，不入源码库
struct AiConfig {
    QString baseUrl;

    QString model;

    QString apiKey;

    // 偏低是故意的：要的是稳定选对工具，温度调高会让模型乱调工具
    double temperature { 0.2 };

    int maxTokens { 1024 };

    int timeoutMs { 60000 };

    static AiConfig Default();

    static AiConfig Load();

    void Save() const;

    bool IsValid() const;

    // agent4cpp 只接受环境变量名而非 Key 本身，这里把 Key 注入进程内环境变量再返回变量名
    QString ApplyApiKeyToEnv() const;

    static QString ApiKeyEnvName();
};

#endif
