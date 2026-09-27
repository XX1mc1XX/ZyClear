#ifndef CONSOLERUNNER_H
#define CONSOLERUNNER_H

#ifdef ZYCLEAR_HAS_AI

#include "AiAgentService.h"

#include <QList>
#include <QString>

class IToolProvider;
class QCoreApplication;
class QTextStream;

// 控制台模式：不进图形界面，直接在命令行里跟助手对话。
// 和图形面板共用同一个 AiAgentService 与同一批工具，只是换了输入输出的方式。
class ConsoleRunner {
public:
    explicit ConsoleRunner(const QList<IToolProvider*>& providers);

    // 跑主循环，返回进程退出码
    int Run(QCoreApplication* app);

private:
    // GUI 子系统程序默认没有控制台，这里给自己接一个
    static void AttachOrAllocateConsole();

    void PrintBanner(QTextStream& out) const;
    void PrintHelp(QTextStream& out) const;
    // 工具调用逐条列出、最后给结论：控制台里没有面板的排版，靠这个把「动手过程」交代清楚
    void PrintResult(QTextStream& out, const AiResult& result) const;

    QList<IToolProvider*> m_providers;
};

#endif // ZYCLEAR_HAS_AI

#endif
