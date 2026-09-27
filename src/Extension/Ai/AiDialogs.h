#ifndef AIDIALOGS_H
#define AIDIALOGS_H

#include <QDialog>

class AiAgentService;

class QLabel;
class QListWidget;
class QPushButton;
class QTextBrowser;

// 历史浏览：列表项只带会话 id，正文按选中项从磁盘现取，避免一次把全部会话读进内存。
// 两个对话框都只借用 service，生命周期短于它，所以存裸指针
class SessionDialog : public QDialog {
    Q_OBJECT

public:
    explicit SessionDialog(AiAgentService* service, QWidget* parent = nullptr);

private slots:

    void onCurrentRowChanged(int row);

    void onDeleteClicked();

private:
    void setupUi();

    void reload();

    AiAgentService* m_pService;
    QListWidget* m_pList;
    QTextBrowser* m_pDetail;
    QPushButton* m_pDeleteButton;
    QLabel* m_pSummaryLabel;
};

// 导入本地文档，检索时把相关片段一起发给模型
class KnowledgeDialog : public QDialog {
    Q_OBJECT

public:
    explicit KnowledgeDialog(AiAgentService* service, QWidget* parent = nullptr);

private slots:

    void onImportClicked();

    void onClearClicked();

private:
    void setupUi();

    void refresh();

    AiAgentService* m_pService;
    QListWidget* m_pFileList;
    QLabel* m_pSummaryLabel;
};

#endif
