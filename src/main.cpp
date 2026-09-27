#include "mainwindow.h"

#include <QApplication>
#include <QString>

#ifdef ZYCLEAR_HAS_AI
// 控制台入口整块藏在条件编译里：不开 AI 扩展的构建连 --cli 都不认识，
// 直接落到下面的图形分支，不会出现「参数被吃掉但什么都不发生」。
#include "Extension/Ai/ConsoleRunner.h"
#include "Integration/CameraToolProvider.h"

#include <QCoreApplication>
#include <QList>
#endif

namespace {

#ifdef ZYCLEAR_HAS_AI
// 模式判定必须发生在任何应用对象构造之前：图形模式要 QApplication，
// 控制台模式要 QCoreApplication，而一个进程只允许有一个、且没法中途重建，
// 选错了没有回头的机会。也正因为这个顺序，不能用 QCommandLineParser
// ——它要求先有 QCoreApplication 才肯解析，于是这里手扫 argv。
//
// 比对用 fromLocal8Bit：Windows 上 argv 是本地代码页字节，
// 非 ASCII 启动参数直接当 UTF-8 串比会得到空结果。
bool WantsConsoleMode(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--cli")) {
            return true;
        }
    }
    return false;
}
#endif

} // namespace

int main(int argc, char* argv[])
{
#ifdef ZYCLEAR_HAS_AI
    // 控制台模式：不建主窗口，直接在命令行里对话。
    // 与图形界面共用同一批工具和同一份配置，行为一致。
    if (WantsConsoleMode(argc, argv)) {
        // 刻意用 QCoreApplication：不触碰窗口系统，无显示设备的环境
        // （Linux 无 X/Wayland 的服务器）也能跑起来。
        QCoreApplication app(argc, argv);

        static CameraToolProvider cameraTools;
        const QList<IToolProvider*> providers { &cameraTools };

        ConsoleRunner runner(providers);
        // 到这里已与图形模式彻底分叉，不能再往下走：下面那句 QApplication
        // 一旦执行就是第二个应用对象，直接崩。
        return runner.Run(&app);
    }
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
