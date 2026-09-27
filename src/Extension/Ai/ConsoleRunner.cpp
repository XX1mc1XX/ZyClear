#include "ConsoleRunner.h"

#ifdef ZYCLEAR_HAS_AI

#include "AiConfig.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTextStream>

#if defined(_WIN32)
#include <windows.h>
#include <cstdio>
#endif

ConsoleRunner::ConsoleRunner(const QList<IToolProvider*>& providers)
    : m_providers(providers)
{
}

void ConsoleRunner::AttachOrAllocateConsole()
{
#if defined(_WIN32)
    // 输出已经被重定向（管道、文件）时标准句柄是有效的，不要去动它 ——
    // 否则会把重定向覆盖成控制台，调用方再也拿不到输出。
    // 只有句柄无效时才说明这是个没有控制台的 GUI 进程，需要自己接一个。
    if (GetStdHandle(STD_OUTPUT_HANDLE) == nullptr
        || GetStdHandle(STD_OUTPUT_HANDLE) == INVALID_HANDLE_VALUE) {
        // 优先复用父进程的控制台（从 cmd/PowerShell 启动），没有就自己开一个窗口（双击 exe）
        if (AttachConsole(ATTACH_PARENT_PROCESS) == FALSE) {
            AllocConsole();
        }

        FILE* dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        freopen_s(&dummy, "CONIN$", "r", stdin);
    }

    // 中文输出要按 UTF-8 解释，否则控制台里是乱码
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void ConsoleRunner::PrintBanner(QTextStream& out) const
{
    const AiConfig config = AiConfig::Load();

    out << "\nzyClear 控制台助手\n";
    out << "  输入自然语言即可，例如「画面有点暗」\n";
    out << "  /help 看命令，/exit 退出\n";

    if (config.IsValid()) {
        out << "  当前模型：" << config.model << "  @ " << config.baseUrl << "\n";
    } else {
        out << "  尚未配置模型，请先在图形界面的 AI 面板里设置。\n";
    }
    out << "\n";
}

void ConsoleRunner::PrintHelp(QTextStream& out) const
{
    out << "  /help  显示这条帮助\n";
    out << "  /exit  退出（q、quit 也可以）\n";
    out << "  其余输入会直接交给助手处理\n\n";
}

void ConsoleRunner::PrintResult(QTextStream& out, const AiResult& result) const
{
    for (const QString& item : result.trace) {
        out << "  · " << item << "\n";
    }

    if (result.ok()) {
        out << result.answer << "\n\n";
    } else {
        out << "出错：" << result.error << "\n\n";
    }
}

int ConsoleRunner::Run(QCoreApplication* app)
{
    Q_UNUSED(app)

    AttachOrAllocateConsole();

    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Utf8);
    QTextStream in(stdin);

    AiAgentService service; // 栈上活到循环结束；Ask 是异步的，局部事件循环期间它必须还活着
    service.SetToolProviders(m_providers);

    PrintBanner(out);
    out.flush();

    while (true) {
        out << "> ";
        out.flush();

        const QString line = in.readLine();
        if (line.isNull()) {
            break; // 输入结束（Ctrl+Z、或管道读完）
        }

        const QString input = line.trimmed();
        if (input.isEmpty()) {
            continue;
        }
        if (input == QStringLiteral("/exit") || input == QStringLiteral("exit")
            || input == QStringLiteral("quit") || input == QStringLiteral("q")) {
            break;
        }
        if (input == QStringLiteral("/help") || input == QStringLiteral("help")) {
            PrintHelp(out);
            out.flush();
            continue;
        }

        // Ask 是异步的：开一个局部事件循环等它答完，再回到读下一行
        QEventLoop loop;
        QObject::connect(&service, &AiAgentService::SigFinished, &loop,
            [this, &out, &loop](const AiResult& result) {
                PrintResult(out, result);
                loop.quit();
            });

        service.Ask(input);
        loop.exec();

        out.flush();
    }

    out << "退出。\n";
    out.flush();
    return 0;
}

#endif // ZYCLEAR_HAS_AI
