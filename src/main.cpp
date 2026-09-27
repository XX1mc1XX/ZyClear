#include "mainwindow.h"

#include <QApplication>
#include <QString>

#ifdef ZYCLEAR_HAS_AI
#include "Extension/Ai/ConsoleRunner.h"
#include "Integration/CameraToolProvider.h"

#include <QCoreApplication>
#include <QList>
#endif

namespace {

#ifdef ZYCLEAR_HAS_AI
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
        QCoreApplication app(argc, argv);

        static CameraToolProvider cameraTools;
        const QList<IToolProvider*> providers { &cameraTools };

        ConsoleRunner runner(providers);
        return runner.Run(&app);
    }
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
