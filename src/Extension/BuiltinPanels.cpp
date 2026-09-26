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

// 把 AI 面板适配成「一个可停靠面板」。
//
// 【为什么用适配器而不是让 AiPanel 直接继承 IPanel】
//   AiPanel 的职责是「聊天界面」，不该同时背上面板注册、停靠位置这些事。
//   加一层适配器之后，AiPanel 保持干净，将来换掉面板框架也不用动它 ——
//   和相机适配器是同一个用法。
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
        // 宿主能力在这里注入进去 —— 面板本身不知道这是相机还是别的什么
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

// 日志面板：三份日志并列成标签页，默认停靠在底部
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

    // 底部面板默认收起：图像区本来就该占大头，需要看日志时再点开
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
