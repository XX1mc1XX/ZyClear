#include "OneCustomWidget.h"

OneCustomWidget::OneCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : QWidget { parent }
    , m_param(param)
    , m_index(index)
{
}

void OneCustomWidget::InitWidget()
{
    QHBoxLayout* pLayout = new QHBoxLayout();
    pLayout->setContentsMargins(0, 0, 0, 0);
    addEditLayout(pLayout);
    this->setLayout(pLayout);
}

void OneCustomWidget::setParam(CameraParam& param)
{
    m_param = param;
}

CameraParam OneCustomWidget::getParam()
{
    return m_param;
}

void OneCustomWidget::addEditLayout(QHBoxLayout* layout)
{
}

