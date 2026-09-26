#ifndef LOGPANEL_H
#define LOGPANEL_H

#include <QString>
#include <QWidget>

class QCheckBox;
class QPlainTextEdit;
class QTabBar;
class QTimer;

// =============================================================================
// 日志面板：把散在各处的日志收进一个可切换的标签栏
//
// 【为什么要它】这个项目现在有三份日志，各自落在不同的地方，
//   出了问题要先知道该去看哪一份、再去那个目录里翻最新文件：
//
//     AI 助手日志   bin/agent4cpp_log/*.txt   AI 每一步决定调什么工具
//     相机 SDK 日志 bin/MvSDKLog/             海康 SDK 自己落的运行日志
//     应用日志      只进调试器                程序自身的 qDebug / qWarning
//
//   这个面板把三者并列成标签页，点一下切换，不用再翻目录。
//
// 【设计上不碰任何现有代码】三个来源都是「只读地看已有的东西」：
//   两个是读文件尾部，应用日志靠 qInstallMessageHandler 挂一个钩子。
//   没有任何一个模块需要为它做改动。
// =============================================================================

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget* parent = nullptr);
    ~LogPanel();

    // 让外部（例如事件总线）也能往应用日志里追加一行
    static void AppendApplicationLog(const QString& line);

private slots:

    void OnSourceChanged(int index);

    void OnRefresh();

    void OnClearClicked();

private:
    void setupUi();

    void InstallMessageHandler();

    // 读文件尾部，避免日志很大时把整个文件读进内存
    QString ReadTail(const QString& filePath) const;

    // 取目录下最新的一个文件
    static QString NewestFileIn(const QString& directory, const QString& suffix);

    QTabBar* m_pSourceTabs;
    QPlainTextEdit* m_pView;
    QCheckBox* m_pAutoScrollCheck;
    QTimer* m_pRefreshTimer;

    int m_currentSource { 0 };
};

#endif
