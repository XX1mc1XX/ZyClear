#include "DemoPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

QString DemoPanel::PanelId() const
{
    // 带命名空间前缀，避免和别的插件的 id 撞车（宿主拒绝重复 id）
    return QStringLiteral("demo.panel.hello");
}

QString DemoPanel::PanelTitle() const
{
    return QStringLiteral("示例插件");
}

Qt::DockWidgetArea DemoPanel::DefaultArea() const
{
    return Qt::LeftDockWidgetArea;
}

bool DemoPanel::VisibleByDefault() const
{
    return true;
}

QWidget* DemoPanel::CreateWidget(QWidget* parent)
{
    QWidget* panel = new QWidget(parent);

    QLabel* label = new QLabel(
        "这个面板来自一个外部 DLL，不是编进主程序里的。\n\n"
        "· 放法：把插件 dll 丢进程序目录下的 extensions/，重启即出现\n"
        "· 删掉那个 dll，这个面板就消失\n"
        "· 主程序没有为它改过一行代码\n\n"
        "这就是「一切皆可导入」：宿主只认 IPanel 接口，"
        "不认识任何一个具体面板。",
        panel);
    label->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->addWidget(label);
    layout->addStretch();

    return panel;
}
