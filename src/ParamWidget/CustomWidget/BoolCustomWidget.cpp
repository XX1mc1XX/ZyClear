#include "BoolCustomWidget.h"

BoolCustomWidget::BoolCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pCheckBox(new QCheckBox(this))
{
    // 勾选框自带底色会盖住表格的交替行色与选中态，刷白与同行的其它单元格保持一致
    m_pCheckBox->setStyleSheet(QString("QCheckBox{background-color: white;}"));
}

void BoolCustomWidget::setParam(CameraParam& param)
{
    // clicked 只在用户交互时发出，编程式 setChecked 不会回发信号，这里本可不断开；
    // 保持与其它控件同一对 disconnect/connect 包围，免得日后换信号或加控件时漏掉
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

