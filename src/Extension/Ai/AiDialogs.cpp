#include "AiDialogs.h"

#include "AiAgentService.h"
#include "HistoryStore.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// 会话跟随的配色，和主面板保持一致
const char* kColorUser = "#1a73e8";
const char* kColorAssistant = "#188038";
const char* kColorError = "#d93025";
const char* kColorMuted = "#7a7a7a";

// 会话正文来自模型输出和用户输入，可能含尖括号，逐条转义后再拼 HTML
QString ToHtml(const QString& text)
{
    QString escaped = text.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return escaped;
}

} // namespace

SessionDialog::SessionDialog(AiAgentService* service, QWidget* parent)
    : QDialog(parent)
    , m_pService(service)
    , m_pList(new QListWidget(this))
    , m_pDetail(new QTextBrowser(this))
    , m_pDeleteButton(new QPushButton(QStringLiteral("删除这次会话"), this))
    , m_pSummaryLabel(new QLabel(this))
{
    setupUi();
    reload();

    connect(m_pList, &QListWidget::currentRowChanged, this, &SessionDialog::onCurrentRowChanged);
    connect(m_pDeleteButton, &QPushButton::clicked, this, &SessionDialog::onDeleteClicked);
}

void SessionDialog::setupUi()
{
    setWindowTitle(QStringLiteral("对话历史"));
    resize(760, 480);

    m_pDetail->setOpenExternalLinks(false);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_pList);
    splitter->addWidget(m_pDetail);
    splitter->setSizes({ 260, 500 });

    QPushButton* closeButton = new QPushButton(QStringLiteral("关闭"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* footer = new QHBoxLayout();
    footer->addWidget(m_pSummaryLabel);
    footer->addStretch();
    footer->addWidget(m_pDeleteButton);
    footer->addWidget(closeButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(splitter);
    layout->addLayout(footer);
}

void SessionDialog::reload()
{
    m_pList->clear();

    const QList<AiSession> sessions = m_pService->Sessions();
    for (const AiSession& session : sessions) {
        const QString title = session.title.trimmed().isEmpty()
            ? QStringLiteral("(还没有问答)")
            : session.title;

        auto* item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(title, session.subtitle()));
        item->setData(Qt::UserRole, session.id);
        m_pList->addItem(item);
    }

    m_pSummaryLabel->setText(QStringLiteral("共 %1 次会话").arg(sessions.size()));

    if (m_pList->count() > 0) {
        m_pList->setCurrentRow(0); // 借 currentRowChanged 顺带把首条的详情加载出来
    } else {
        m_pDetail->setHtml(QStringLiteral(
            "<i style='color:%1'>还没有历史会话。问一句试试。</i>")
                               .arg(QString::fromLatin1(kColorMuted)));
    }
}

void SessionDialog::onCurrentRowChanged(int row)
{
    if (row < 0 || row >= m_pList->count()) {
        m_pDetail->clear();
        return;
    }

    const QString id = m_pList->item(row)->data(Qt::UserRole).toString();
    // 正文现从磁盘取：列表项里只留了一个 id，没必要把所有会话都驻留在内存
    const AiSession session = HistoryStore::Load(id);

    if (session.turns.isEmpty()) {
        m_pDetail->setHtml(QStringLiteral("<i style='color:%1'>这次会话还没有问答。</i>")
                               .arg(QString::fromLatin1(kColorMuted)));
        return;
    }

    QString html;
    for (const AiTurn& turn : session.turns) {
        html += QStringLiteral("<div style='margin:8px 0'><b style='color:%1'>你</b><br/>%2</div>")
                    .arg(QString::fromLatin1(kColorUser), ToHtml(turn.question));

        if (!turn.trace.isEmpty()) {
            html += QStringLiteral("<div style='color:%1;font-size:11px'>调用了 %2 次工具：%3</div>")
                        .arg(QString::fromLatin1(kColorMuted))
                        .arg(turn.trace.size())
                        .arg(turn.trace.join(QStringLiteral(" · ")).toHtmlEscaped());
        }

        if (!turn.answer.isEmpty()) {
            html += QStringLiteral("<div style='margin:8px 0'><b style='color:%1'>AI</b><br/>%2</div>")
                        .arg(QString::fromLatin1(kColorAssistant), ToHtml(turn.answer));
        }

        if (!turn.error.isEmpty()) {
            html += QStringLiteral("<div style='margin:8px 0'><b style='color:%1'>出错</b><br/>%2</div>")
                        .arg(QString::fromLatin1(kColorError), ToHtml(turn.error));
        }
    }

    m_pDetail->setHtml(html);
}

void SessionDialog::onDeleteClicked()
{
    QListWidgetItem* item = m_pList->currentItem();
    if (item == nullptr) {
        return;
    }

    const QString id = item->data(Qt::UserRole).toString();
    const QString title = item->text().split(QLatin1Char('\n')).first();

    const QMessageBox::StandardButton answer = QMessageBox::question(this,
        QStringLiteral("删除会话"),
        QStringLiteral("确定删除「%1」？这条记录会从磁盘上移除，无法恢复。").arg(title),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    // 删的若是当前会话，service 会顺手开一段新会话并重置模型侧上下文
    m_pService->DeleteSession(id);
    reload();
}

KnowledgeDialog::KnowledgeDialog(AiAgentService* service, QWidget* parent)
    : QDialog(parent)
    , m_pService(service)
    , m_pFileList(new QListWidget(this))
    , m_pSummaryLabel(new QLabel(this))
{
    setupUi();
    refresh();

    // 导入和清空都经过 service，挂这一个信号能同时覆盖两条路径，也和主面板的显示保持一致
    connect(m_pService, &AiAgentService::SigKnowledgeChanged, this, &KnowledgeDialog::refresh);
}

void KnowledgeDialog::setupUi()
{
    setWindowTitle(QStringLiteral("知识库"));
    resize(520, 420);

    QLabel* hint = new QLabel(
        QStringLiteral(
            "把设备手册、操作规程、故障码表之类的文档导进来。\n"
            "提问时会先按关键词检索，把最相关的片段一起发给模型 —— 它就不必靠猜。"),
        this);
    hint->setWordWrap(true);

    QPushButton* importButton = new QPushButton(QStringLiteral("导入文档…"), this);
    QPushButton* clearButton = new QPushButton(QStringLiteral("清空知识库"), this);
    QPushButton* closeButton = new QPushButton(QStringLiteral("关闭"), this);

    connect(importButton, &QPushButton::clicked, this, &KnowledgeDialog::onImportClicked);
    connect(clearButton, &QPushButton::clicked, this, &KnowledgeDialog::onClearClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* footer = new QHBoxLayout();
    footer->addWidget(m_pSummaryLabel);
    footer->addStretch();
    footer->addWidget(importButton);
    footer->addWidget(clearButton);
    footer->addWidget(closeButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_pFileList);
    layout->addLayout(footer);
}

void KnowledgeDialog::refresh()
{
    m_pFileList->clear();

    const QStringList files = m_pService->KnowledgeFiles();
    for (const QString& name : files) {
        m_pFileList->addItem(name);
    }

    if (files.isEmpty()) {
        m_pSummaryLabel->setText(QStringLiteral("还没有导入文档"));
    } else {
        m_pSummaryLabel->setText(QStringLiteral("%1 个文档 / %2 个片段")
                                     .arg(files.size())
                                     .arg(m_pService->KnowledgeChunkCount()));
    }
}

void KnowledgeDialog::onImportClicked()
{
    if (m_pService->IsBusy()) {
        QMessageBox::information(this, QStringLiteral("稍等"),
            QStringLiteral("AI 正在处理上一句，等它答完再导入。"));
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(this,
        QStringLiteral("选择要导入的文档"), QString(),
        QStringLiteral("文档 (*.txt *.md *.log *.csv *.json);;所有文件 (*.*)"));

    if (files.isEmpty()) {
        return;
    }

    const int imported = m_pService->ImportKnowledge(files);

    if (imported == 0) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("这些文件都没能读入（可能是空文件或没有读取权限）。"));
    } else {
        QMessageBox::information(this, QStringLiteral("导入完成"),
            QStringLiteral("成功导入 %1 个文档。\n\n"
                           "注意：为了让新资料立即生效，会话已经重新开始，"
                           "之前那轮对话可以在「对话历史」里回看。")
                .arg(imported));
    }

    refresh();
}

void KnowledgeDialog::onClearClicked()
{
    if (m_pService->KnowledgeFileCount() == 0) {
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(this,
        QStringLiteral("清空知识库"),
        QStringLiteral("只影响程序内存里的检索库，磁盘上的原文档不会被删。\n确定清空？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    m_pService->ClearKnowledge();
    refresh();
}
