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

#include <memory>

namespace {

// 现画一个「侧边栏」图标：一个矩形，里面一条竖线表示面板停在哪一侧。
// 画出来而不是塞一张 png，是为了不往资源里加二进制文件 ——
// 这点小图形不值得多一个资源条目，也省得为不同分辨率准备多套图。
QIcon MakePanelIcon(Qt::DockWidgetArea area)
{
    const int size = 20;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(210, 210, 210), 1.4));

    // 外框
    painter.drawRoundedRect(QRectF(2.5, 4.5, 15, 11), 2, 2);

    // 分隔线：按照面板实际停靠的位置画
    if (area == Qt::LeftDockWidgetArea) {
        painter.drawLine(QPointF(7, 4.5), QPointF(7, 15.5));
    } else if (area == Qt::BottomDockWidgetArea) {
        painter.drawLine(QPointF(2.5, 12), QPointF(17.5, 12));
    } else {
        painter.drawLine(QPointF(13, 4.5), QPointF(13, 15.5));
    }

    painter.end();
    return QIcon(pixmap);
}

// 找主窗口里名为「视图」的菜单，没有就新建一个。
// 面板被关掉之后要能从这里重新打开，否则用户会以为它丢了。
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

int ExtensionHost::Attach(QMainWindow* window, PanelRegistry* registry)
{
    if (window == nullptr || registry == nullptr) {
        return 0;
    }

    QMenu* viewMenu = EnsureViewMenu(window);

    // 顶部工具栏：每个面板一个切换按钮，对应 VS Code 右上角那排布局图标。
    // 面板关掉之后从这里一键打开，不必翻菜单。
    QToolBar* panelBar = window->findChild<QToolBar*>(QStringLiteral("panelToolBar"));
    if (panelBar == nullptr) {
        panelBar = new QToolBar(QStringLiteral("面板"), window);
        panelBar->setObjectName(QStringLiteral("panelToolBar"));
        panelBar->setMovable(false);
        panelBar->setIconSize(QSize(18, 18));
        window->addToolBar(Qt::TopToolBarArea, panelBar);

        // 一个会自动撑开的空控件，把后面的按钮挤到最右边
        QWidget* spacer = new QWidget(panelBar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        panelBar->addWidget(spacer);
    }

    int attached = 0;
    const QStringList ids = registry->Ids();

    QList<QDockWidget*> sideDocks; // 停靠在左右两侧的
    QList<QDockWidget*> bottomDocks; // 停靠在底部的

    for (const QString& id : ids) {
        // 面板对象只用来提供元信息和造出界面，造完就可以释放
        std::unique_ptr<IPanel> panel(registry->Create(id));
        if (!panel) {
            continue;
        }

        QDockWidget* dock = new QDockWidget(panel->PanelTitle(), window);
        dock->setObjectName(QStringLiteral("dock_") + id);

        // 内容的所有权交给 dock，Qt 的父子关系负责析构
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
        // 勾上/取消就等价于显示/隐藏，勾选状态还会自动跟着面板状态走
        QAction* toggle = dock->toggleViewAction();
        toggle->setText(panel->PanelTitle());
        toggle->setIcon(MakePanelIcon(area));
        toggle->setToolTip(QStringLiteral("显示/隐藏%1").arg(panel->PanelTitle()));
        viewMenu->addAction(toggle);
        panelBar->addAction(toggle);

        dock->setVisible(panel->VisibleByDefault());

        ++attached;
    }

    // 给停靠面板一个像样的初始尺寸。
    // QDockWidget 默认按内容的最小尺寸摆放，容易窄到看不出那是个面板；
    // 这里给个初始值，用户之后可以随意拖动或关闭。
    // 宽度刻意取小：这机器是高 DPI 小逻辑屏，侧边栏一宽就会把中央区域挤变形。
    if (!sideDocks.isEmpty()) {
        window->resizeDocks(sideDocks, QList<int>(sideDocks.size(), 220), Qt::Horizontal);
    }
    if (!bottomDocks.isEmpty()) {
        window->resizeDocks(bottomDocks, QList<int>(bottomDocks.size(), 200), Qt::Vertical);
    }

    return attached;
}
