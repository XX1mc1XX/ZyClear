#ifndef AIPANEL_H
#define AIPANEL_H

#include "AiAgentService.h"

#include <QList>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QToolButton;

// =============================================================================
// AI 助手面板（通用层）
//
// 【它演什么角色】对话框 + 控制台。
//   用户在这里说人话，AI 把它翻译成对设备的操作；
//   过程中调了哪些工具、用了哪些参考资料都显示出来 —— 避免 AI 变成黑盒。
//
// 【布局取舍】侧边栏天生窄，所以「历史会话」和「知识库」不占主区，
//   各做成一个弹窗；主区只留对话和输入。这样面板再窄也能正常用。
//
// 【依赖】只认识 AiAgentService 与 IToolProvider，不认识任何具体客户端。
// =============================================================================

class AiPanel : public QWidget {
    Q_OBJECT

public:
    explicit AiPanel(QWidget* parent = nullptr);
    ~AiPanel();

    // 注入宿主能力。由面板适配器在创建时调用
    void SetToolProviders(const QList<IToolProvider*>& providers);

protected:
    // 输入框里按 Enter 直接发送，Shift+Enter 才是换行
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:

    void onSendClicked();

    void onSettingsClicked();

    void onNewSessionClicked();

    void onHistoryClicked();

    void onKnowledgeClicked();

    void onFinished(AiResult result);

    void onBusyChanged(bool busy);

    void onSessionsChanged();

    void onKnowledgeChanged();

private:
    void setupUi();

    void appendBubble(const QString& title, const QString& body, const QString& color);

    void appendTrace(const QStringList& trace);

    void appendWelcome();

    void setStatus(const QString& text);

    void refreshHeader();

    AiAgentService* m_pService;

    QTextBrowser* m_pHistory;
    QPlainTextEdit* m_pInput;
    QPushButton* m_pSendButton;
    QToolButton* m_pSettingsButton;
    QToolButton* m_pNewSessionButton;
    QToolButton* m_pHistoryButton;
    QToolButton* m_pKnowledgeButton;
    QLabel* m_pStatusLabel;
    QLabel* m_pTitleLabel;
};

#endif
