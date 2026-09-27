#include "CmdCustomWidget.h"

CmdCustomWidget::CmdCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pCmdButton(new QPushButton(this))
{
}

void CmdCustomWidget::setParam(CameraParam& param)
{
    // clicked 只在用户交互时发出，编程式改文本不会回发信号，这里本可不断开；
    // 与其它控件保持同一对 disconnect/connect 包围，避免不一致的写法被照抄到有信号的控件上
    disconnect(m_pCmdButton, &QPushButton::clicked,
        this, &CmdCustomWidget::onCmdButtonClicked);

    OneCustomWidget::setParam(param);
    m_pCmdButton->setText(m_param.displayText());

    connect(m_pCmdButton, &QPushButton::clicked,
        this, &CmdCustomWidget::onCmdButtonClicked);
}

CameraParam CmdCustomWidget::getParam()
{
    return OneCustomWidget::getParam();
}

void CmdCustomWidget::addEditLayout(QHBoxLayout* layout)
{
    layout->addWidget(m_pCmdButton);
}

void CmdCustomWidget::onCmdButtonClicked()
{
    // 不先改 m_param：命令参数的值本来就是空的，相机端凭 name 执行，直接上报即可。
    // 也因此本控件没有“编辑期”，不会走 delegate 的 setModelData 回写路径
    emit sigValueChanged(m_param, m_index);
}

