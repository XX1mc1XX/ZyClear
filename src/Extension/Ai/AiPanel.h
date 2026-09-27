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

class AiPanel : public QWidget {
    Q_OBJECT

public:
    explicit AiPanel(QWidget* parent = nullptr);
    ~AiPanel();

    void SetToolProviders(const QList<IToolProvider*>& providers);

protected:
    // 回车发送，Shift+回车换行
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
