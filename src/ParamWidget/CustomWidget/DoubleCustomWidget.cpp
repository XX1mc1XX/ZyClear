#include "DoubleCustomWidget.h"

DoubleCustomWidget::DoubleCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_SpinBox(new QDoubleSpinBox(this))
{
}

void DoubleCustomWidget::setParam(CameraParam& param)
{
    disconnect(m_SpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DoubleCustomWidget::onValueChanged);

    OneCustomWidget::setParam(param);
    DoubleParam varParam = m_param.GetValue().value<DoubleParam>();
    m_SpinBox->setMinimum(varParam.min);
    m_SpinBox->setMaximum(varParam.max);
    m_SpinBox->setSingleStep(0.1);
    m_SpinBox->setDecimals(3);
    m_SpinBox->setValue(varParam.value);

    connect(m_SpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DoubleCustomWidget::onValueChanged);
}

CameraParam DoubleCustomWidget::getParam()
{
    return OneCustomWidget::getParam();
}

void DoubleCustomWidget::addEditLayout(QHBoxLayout* layout)
{
    layout->addWidget(m_SpinBox);
}

void DoubleCustomWidget::onValueChanged(double value)
{
    DoubleParam varParam = getParam().GetValue().value<DoubleParam>();
    varParam.value = value;
    m_param.SetValue(QVariant::fromValue(varParam));
}

