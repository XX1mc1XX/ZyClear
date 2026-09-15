#include "EnumCustomWidget.h"

EnumCustomWidget::EnumCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pCombox(new QComboBox(this))
{
}

void EnumCustomWidget::setParam(CameraParam& param)
{
    disconnect(m_pCombox, &QComboBox::currentIndexChanged,
        this, &EnumCustomWidget::onValueChanged);

    OneCustomWidget::setParam(param);
    EnumParam varParam = m_param.GetValue().value<EnumParam>();

    QStringList valueList(varParam.availableValue.begin(), varParam.availableValue.end());
    m_pCombox->addItems(valueList);
    m_pCombox->setCurrentText(varParam.value);

    connect(m_pCombox, &QComboBox::currentIndexChanged,
        this, &EnumCustomWidget::onValueChanged);
}

CameraParam EnumCustomWidget::getParam()
{
    return OneCustomWidget::getParam();
}

void EnumCustomWidget::addEditLayout(QHBoxLayout* layout)
{
    layout->addWidget(m_pCombox);
}

void EnumCustomWidget::onValueChanged(int value)
{
    EnumParam varParam = getParam().GetValue().value<EnumParam>();
    varParam.value = m_pCombox->currentText();
    varParam.valueInt = value;
    m_param.SetValue(QVariant::fromValue(varParam));
}

