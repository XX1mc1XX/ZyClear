#include "IntCustomWidget.h"

IntCustomWidget::IntCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_SpinBox(new QSpinBox(this))
{
    // 数值框撑满整格；其它几类控件没设，属于各自的差异，不是统一约定
    m_SpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void IntCustomWidget::setParam(CameraParam& param)
{
    disconnect(m_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &IntCustomWidget::onValueChanged);

    OneCustomWidget::setParam(param);
    IntParam varParam = m_param.GetValue().value<IntParam>();
    // 先设范围再设值，否则超界的值会被旧区间夹一次；increment 为 0 时 QSpinBox 会
    // 回退到默认步长
    m_SpinBox->setMinimum(varParam.min);
    m_SpinBox->setMaximum(varParam.max);
    m_SpinBox->setSingleStep(varParam.increment);
    m_SpinBox->setValue(varParam.value);

    connect(m_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &IntCustomWidget::onValueChanged);
}

CameraParam IntCustomWidget::getParam()
{
    return OneCustomWidget::getParam();
}

void IntCustomWidget::addEditLayout(QHBoxLayout* layout)
{
    layout->addWidget(m_SpinBox);
}

void IntCustomWidget::onValueChanged(int value)
{
    IntParam varParam = getParam().GetValue().value<IntParam>();
    varParam.value = value;
    m_param.SetValue(QVariant::fromValue(varParam));
}

