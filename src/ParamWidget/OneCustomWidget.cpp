#include "OneCustomWidget.h"

OneCustomWidget::OneCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : QWidget { parent }
    , m_param(param)
    , m_index(index)
{
}

void OneCustomWidget::InitWidget()
{
    // 布局本身不留白，编辑器几何由 delegate 的 updateEditorGeometry 按单元格 rect 设定
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
    // 有意留空而非纯虚：漏重写的子类会静默得到空单元格，而不是编译期报错
}

