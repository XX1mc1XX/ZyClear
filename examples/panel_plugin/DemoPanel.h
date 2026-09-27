// 示例外部面板插件：只依赖 ExtensionInterface.h，编成 DLL 放进 extensions/ 即被宿主加载

#ifndef DEMOPANEL_H
#define DEMOPANEL_H

#include "ExtensionInterface.h"

class DemoPanel : public IPanel {
public:
    QString PanelId() const override;

    QString PanelTitle() const override;

    QWidget* CreateWidget(QWidget* parent) override;

    Qt::DockWidgetArea DefaultArea() const override;

    bool VisibleByDefault() const override;
};

#endif
