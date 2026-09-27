#include "BuiltinPanels.h"

#include "ExtensionInterface.h"
#include "LogPanel.h"
#include "PanelRegistry.h"

#ifdef ZYCLEAR_HAS_AI
#include "Ai/AiPanel.h"
#endif

#include <QWidget>

namespace {

#ifdef ZYCLEAR_HAS_AI

// 适配器：AiPanel 只管聊天界面，不背面板注册与停靠位置的职责
class AiPanelExtension : public IPanel {
public:
    explicit AiPanelExtension(const QList<IToolProvider*>& providers)
        : m_providers(providers)
    {
    }

    QString PanelId() const override
    {
        return QStringLiteral("extension.panel.ai");
    }

    QString PanelTitle() const override
    {
        return QStringLiteral("AI 助手");
    }

    QWidget* CreateWidget(QWidget* parent) override
    {
        auto* panel = new AiPanel(parent);
        panel->SetToolProviders(m_providers);
        return panel;
    }

    Qt::DockWidgetArea DefaultArea() const override
    {
        return Qt::RightDockWidgetArea;
    }

private:
    QList<IToolProvider*> m_providers;
};

#endif // ZYCLEAR_HAS_AI

class LogPanelExtension : public IPanel {
public:
    QString PanelId() const override
    {
        return QStringLiteral("extension.panel.log");
    }

    QString PanelTitle() const override
    {
        return QStringLiteral("日志");
    }

    QWidget* CreateWidget(QWidget* parent) override
    {
        return new LogPanel(parent);
    }

    Qt::DockWidgetArea DefaultArea() const override
    {
        return Qt::BottomDockWidgetArea;
    }

    // 底部日志默认收起，图像区占大头
    bool VisibleByDefault() const override
    {
        return false;
    }
};

} // namespace

void RegisterBuiltinPanels(PanelRegistry* registry, const QList<IToolProvider*>& providers)
{
    if (registry == nullptr) {
        return;
    }

#ifdef ZYCLEAR_HAS_AI
    registry->Register(QStringLiteral("extension.panel.ai"),
        [providers]() -> IPanel* { return new AiPanelExtension(providers); });
#endif

    registry->Register(QStringLiteral("extension.panel.log"),
        []() -> IPanel* { return new LogPanelExtension(); });
}
