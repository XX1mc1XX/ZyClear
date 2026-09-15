#include "StringCustomWidget.h"

StringCustomWidget::StringCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pLineEdit(new QLineEdit(this))
{
}

void StringCustomWidget::setParam(CameraParam& param)
{
    disconnect(m_pLineEdit, &QLineEdit::editingFinished,
        this, &StringCustomWidget::onValueChanged);

    OneCustomWidget::setParam(param);
    StringParam varParam = m_param.GetValue().value<StringParam>();
    m_pLineEdit->setText(varParam.value);

    connect(m_pLineEdit, &QLineEdit::editingFinished,
        this, &StringCustomWidget::onValueChanged);
}

CameraParam StringCustomWidget::getParam()
{
    return OneCustomWidget::getParam();
}

void StringCustomWidget::addEditLayout(QHBoxLayout* layout)
{
    layout->addWidget(m_pLineEdit);
}

void StringCustomWidget::onValueChanged()
{
    StringParam varParam = getParam().GetValue().value<StringParam>();
    varParam.value = m_pLineEdit->text();
    m_param.SetValue(QVariant::fromValue(varParam));
}

