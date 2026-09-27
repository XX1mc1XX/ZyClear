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
    // DoubleParam 只有 value/min/max，没有增量字段，步长只能在界面侧定；3 位小数覆盖
    // 常见的曝光/增益精度。必须先设范围再设值，否则超界的值会被旧区间夹一次
    m_SpinBox->setMinimum(varParam.min);
    m_SpinBox->setMaximum(varParam.max);
    m_SpinBox->setSingleStep(0.1);
    m_SpinBox->setDecimals(3);
    m_SpinBox->setValue(varParam.value);

    // valueChanged 有 int 与 double 两个重载，Qt6 下必须用 QOverload 显式取址，
    // 否则 connect 推导不出目标函数
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

