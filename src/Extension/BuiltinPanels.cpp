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
        // 每次挂载都新造一个 AiPanel；providers 这里只是浅拷一串裸指针，
        // provider 对象本身仍由调用方养着，面板不负责它的生死。
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

// 这些适配器只在本文件的注册点用一次，所以不放进头文件：
// 外部没有理由认识它们，面板对外的身份就是注册表里的那个 id。
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
    // 没链上 agent4cpp 时整个 AI 面板不登记，程序退化成纯相机客户端，
    // 其余面板照常。闭包按值捕获 providers，因此不依赖调用方的栈。
    registry->Register(QStringLiteral("extension.panel.ai"),
        [providers]() -> IPanel* { return new AiPanelExtension(providers); });
#endif

    // 内置面板统一用 extension.panel. 前缀，与外部插件的 id 处在同一命名空间，
    // 靠「先登记」占位，保证 external DLL 顶不掉它们。
    registry->Register(QStringLiteral("extension.panel.log"),
        []() -> IPanel* { return new LogPanelExtension(); });
}
