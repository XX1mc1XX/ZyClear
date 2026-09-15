#include "CmdCustomWidget.h"

CmdCustomWidget::CmdCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pCmdButton(new QPushButton(this))
{
}

void CmdCustomWidget::setParam(CameraParam& param)
{
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
    emit sigValueChanged(m_param, m_index);
}

