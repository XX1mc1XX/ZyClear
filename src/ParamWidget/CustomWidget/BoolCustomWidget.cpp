#include "BoolCustomWidget.h"

BoolCustomWidget::BoolCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pCheckBox(new QCheckBox(this))
{
    m_pCheckBox->setStyleSheet(QString("QCheckBox{background-color: white;}"));
}

void BoolCustomWidget::setParam(CameraParam& param)
{
    disconnect(m_pCheckBox, &QCheckBox::clicked,
        this, &BoolCustomWidget::onValueChanged);

    OneCustomWidget::setParam(param);
    BoolParam varParam = m_param.GetValue().value<BoolParam>();
    m_pCheckBox->setChecked(varParam.value);
    m_pCheckBox->setText(m_param.displayText());

    connect(m_pCheckBox, &QCheckBox::clicked,
        this, &BoolCustomWidget::onValueChanged);
}

CameraParam BoolCustomWidget::getParam()
{
    return OneCustomWidget::getParam();
}

void BoolCustomWidget::addEditLayout(QHBoxLayout* layout)
{
    layout->addWidget(m_pCheckBox);
}

void BoolCustomWidget::onValueChanged(bool checked)
{
    BoolParam varParam = getParam().GetValue().value<BoolParam>();
    varParam.value = checked;
    m_param.SetValue(QVariant::fromValue(varParam));
}

