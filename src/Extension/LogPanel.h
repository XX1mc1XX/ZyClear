#ifndef LOGPANEL_H
#define LOGPANEL_H

#include <QString>
#include <QWidget>

class QCheckBox;
class QPlainTextEdit;
class QTabBar;
class QTimer;

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget* parent = nullptr);
    // 析构不回退全局消息钩子：钩子指向的是文件级静态函数，面板销毁后仍然有效，
    // 反复装卸反而会让 handler 链越套越长。
    ~LogPanel();

    // 静态入口，给拿不到面板对象、或日志早于面板构造的地方用；内部有锁，任意线程可调。
    static void AppendApplicationLog(const QString& line);

private slots:

    void OnSourceChanged(int index);

    void OnRefresh();

    void OnClearClicked();

private:
    void setupUi();

    void InstallMessageHandler();

    // 只读文件尾部，日志很大时不至于把整个文件读进内存
    QString ReadTail(const QString& filePath) const;

    // 日志文件按日期滚动，文件名事先不可知，只能按修改时间挑最新的那个；
    // 目录不存在或没有匹配文件时返回空串。
    static QString NewestFileIn(const QString& directory, const QString& suffix);

    QTabBar* m_pSourceTabs;
    QPlainTextEdit* m_pView;
    QCheckBox* m_pAutoScrollCheck;
    QTimer* m_pRefreshTimer;

    int m_currentSource { 0 };
};

#endif
