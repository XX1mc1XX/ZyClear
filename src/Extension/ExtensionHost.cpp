#include "ExtensionHost.h"

#include "ExtensionInterface.h"
#include "PanelRegistry.h"

#include <QAction>
#include <QDockWidget>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QToolBar>

#include <algorithm>
#include <memory>

namespace {

// 现画的侧边栏图标：一个矩形，里面一条线表示面板贴在哪一侧。
// 画出来而不往资源里塞 png，省得为各种分辨率各准备一套图。
QIcon MakePanelIcon(Qt::DockWidgetArea area)
{
    const int size = 20;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 浮动窗口不贴任何一边，用虚线区分
    const bool floating = (area == Qt::NoDockWidgetArea);
    painter.setPen(QPen(QColor(210, 210, 210), 1.4,
        floating ? Qt::DashLine : Qt::SolidLine));
    painter.drawRoundedRect(QRectF(2.5, 4.5, 15, 11), 2, 2);

    if (!floating) {
        if (area == Qt::LeftDockWidgetArea) {
            painter.drawLine(QPointF(7, 4.5), QPointF(7, 15.5));
        } else if (area == Qt::BottomDockWidgetArea) {
            painter.drawLine(QPointF(2.5, 12), QPointF(17.5, 12));
        } else {
            painter.drawLine(QPointF(13, 4.5), QPointF(13, 15.5));
        }
    }

    painter.end();
    return QIcon(pixmap);
}

// 工具栏按钮的先后 = 面板此刻停靠的位置，从左扫到右。
// 不能直接拿 Qt::DockWidgetArea 的枚举值排序：那是 1/2/4/8 的位掩码，
// 排出来是 左、右、上、下，和屏幕上的空间顺序对不上。
int CurrentAreaOrder(QMainWindow* window, QDockWidget* dock)
{
    switch (window->dockWidgetArea(dock)) {
    case Qt::LeftDockWidgetArea:
        return 0;
    case Qt::BottomDockWidgetArea:
        return 1;
    case Qt::RightDockWidgetArea:
        return 2;
    case Qt::TopDockWidgetArea:
        return 3;
    default:
        return 4; // 浮动窗口，排最后
    }
}

// 找主窗口里名为「视图」的菜单，没有就新建。面板关掉后要能从这里重新打开
QMenu* EnsureViewMenu(QMainWindow* window)
{
    QMenuBar* menuBar = window->menuBar();
    if (menuBar == nullptr) {
        menuBar = new QMenuBar(window);
        window->setMenuBar(menuBar);
    }

    const QString title = QStringLiteral("视图");
    const QList<QAction*> actions = menuBar->actions();
    for (QAction* action : actions) {
        if (action->text() == title && action->menu() != nullptr) {
            return action->menu();
        }
    }

    return menuBar->addMenu(title);
}

} // namespace

// 工具栏按钮要跟着面板的实际归属走，所以要记住「哪个面板对应哪个按钮」，
// 并在面板被拖到别处后重排。这份对应关系得有地方存，用一个小对象挂在主窗口下。
class PanelBarController : public QObject {
    Q_OBJECT

public:
    PanelBarController(QMainWindow* window, QToolBar* bar, QObject* parent)
        : QObject(parent)
        , m_window(window)
        , m_bar(bar)
    {
    }

    void Add(QDockWidget* dock, QAction* toggle)
    {
        m_entries.append(PanelEntry { dock, toggle });

        // 面板被拖到别的边（或拖出去浮动）后重新归组。
        // 不接这个信号，按钮会一直停在初始那一组，和真实归属对不上。
        connect(dock, &QDockWidget::dockLocationChanged, this,
            [this](Qt::DockWidgetArea) { Rebuild(); });
    }

    void Rebuild()
    {
        // 先全部摘下来再按新顺序放回。
        // 用 removeAction 而不是 bar->clear()：clear() 会连那个把按钮顶到右边的
        // spacer 一起清掉；而且 toggle 的所有权在 dock 手上，摘下来不会删它。
        for (const PanelEntry& entry : m_entries) {
            m_bar->removeAction(entry.toggle);
        }

        QList<PanelEntry> ordered = m_entries;
        std::stable_sort(ordered.begin(), ordered.end(),
            [this](const PanelEntry& lhs, const PanelEntry& rhs) {
                return CurrentAreaOrder(m_window, lhs.dock)
                    < CurrentAreaOrder(m_window, rhs.dock);
            });

        for (const PanelEntry& entry : ordered) {
            entry.toggle->setIcon(MakePanelIcon(m_window->dockWidgetArea(entry.dock)));
            m_bar->addAction(entry.toggle);
        }
    }

private:
    struct PanelEntry {
        QDockWidget* dock;
        QAction* toggle;
    };

    QMainWindow* m_window;
    QToolBar* m_bar;
    QList<PanelEntry> m_entries;
};

int ExtensionHost::Attach(QMainWindow* window, PanelRegistry* registry)
{
    // 宿主按可选依赖对待：参数不全就当没有可挂的面板，返回 0。
    // 调用方（包括测试）因此不必自己先判空。
    if (window == nullptr || registry == nullptr) {
        return 0;
    }

    QMenu* viewMenu = EnsureViewMenu(window);

    // 顶部工具栏：每个面板一个切换按钮，对应 VS Code 右上角那排布局图标
    // 工具栏按 objectName 找回而不是记成员：Attach 才能不持状态，对任意窗口都可调
    QToolBar* panelBar = window->findChild<QToolBar*>(QStringLiteral("panelToolBar"));
    if (panelBar == nullptr) {
        panelBar = new QToolBar(QStringLiteral("面板"), window);
        panelBar->setObjectName(QStringLiteral("panelToolBar"));
        panelBar->setMovable(false);
        panelBar->setIconSize(QSize(18, 18));
        window->addToolBar(Qt::TopToolBarArea, panelBar);

        // 会自动撑开的空控件，把后面的按钮挤到最右边
        QWidget* spacer = new QWidget(panelBar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        panelBar->addWidget(spacer);
    }

    // 重复调用 Attach（例如运行中重新加载插件）时复用同一个，避免挂出多份重排逻辑
    auto* controller
        = window->findChild<PanelBarController*>(QStringLiteral("panelBarController"));
    if (controller == nullptr) {
        controller = new PanelBarController(window, panelBar, window);
        controller->setObjectName(QStringLiteral("panelBarController"));
    }

    int attached = 0;
    const QStringList ids = registry->Ids();

    QList<QDockWidget*> sideDocks;
    QList<QDockWidget*> bottomDocks;

    for (const QString& id : ids) {
        // 面板对象只用来提供元信息和造出界面，造完即可释放
        std::unique_ptr<IPanel> panel(registry->Create(id));
        if (!panel) {
            continue;
        }

        QDockWidget* dock = new QDockWidget(panel->PanelTitle(), window);
        dock->setObjectName(QStringLiteral("dock_") + id);

        // 一个面板造不出界面只跳过它一个；dock 这时还没有父窗口接管，
        // 必须在这里手动 delete，否则每次挂载都漏一份。
        QWidget* content = panel->CreateWidget(dock);
        if (content == nullptr) {
            delete dock;
            continue;
        }
        dock->setWidget(content);

        // 可关闭 + 可移动 + 可浮动：右上角能收起，也能拖到别的边
        dock->setFeatures(QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable
            | QDockWidget::DockWidgetFloatable);

        const Qt::DockWidgetArea area = panel->DefaultArea();
        window->addDockWidget(area, dock);
        if (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea) {
            sideDocks.append(dock);
        } else {
            bottomDocks.append(dock);
        }

        // toggleViewAction 是 QDockWidget 自带的动作，
        // 勾上/取消就等于显示/隐藏，勾选状态还会跟着面板状态自动变
        QAction* toggle = dock->toggleViewAction();
        toggle->setText(panel->PanelTitle());
        toggle->setIcon(MakePanelIcon(area));
        toggle->setToolTip(QStringLiteral("显示/隐藏%1").arg(panel->PanelTitle()));
        viewMenu->addAction(toggle);

        controller->Add(dock, toggle);

        dock->setVisible(panel->VisibleByDefault());

        ++attached;
    }

    // 顺序和图标都按面板此刻的实际位置算，统一在这里落位
    controller->Rebuild();

    // 给停靠面板一个像样的初始尺寸。QDockWidget 默认按内容最小尺寸摆放，
    // 容易窄到看不出那是个面板。宽度刻意取小：这机器是高 DPI 小逻辑屏，
    // 侧边栏一宽就会把中央区域挤变形。
    if (!sideDocks.isEmpty()) {
        window->resizeDocks(sideDocks, QList<int>(sideDocks.size(), 220), Qt::Horizontal);
    }
    if (!bottomDocks.isEmpty()) {
        window->resizeDocks(bottomDocks, QList<int>(bottomDocks.size(), 200), Qt::Vertical);
    }

    return attached;
}

#include "ExtensionHost.moc"
