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
    ~LogPanel();

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

    static QString NewestFileIn(const QString& directory, const QString& suffix);

    QTabBar* m_pSourceTabs;
    QPlainTextEdit* m_pView;
    QCheckBox* m_pAutoScrollCheck;
    QTimer* m_pRefreshTimer;

    int m_currentSource { 0 };
};

#endif
