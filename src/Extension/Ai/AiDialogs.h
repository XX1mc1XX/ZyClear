#ifndef AIDIALOGS_H
#define AIDIALOGS_H

#include <QDialog>

class AiAgentService;

class QLabel;
class QListWidget;
class QPushButton;
class QTextBrowser;

// =============================================================================
// AI 面板的两个附属窗口
//
// 【为什么做成弹窗而不是侧边栏里的一块】侧边栏天生窄，
//   把会话列表和知识库清单塞进去会把对话框本身挤没。
//   做成弹窗后，侧边栏可以缩到很窄也不影响这两件事的操作。
// =============================================================================

// 历史会话：左边列出过去的会话，右边看那次聊了什么
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

// 知识库：导入本地文档，检索时会把相关片段一起发给模型
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
