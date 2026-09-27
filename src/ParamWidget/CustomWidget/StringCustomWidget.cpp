#include "StringCustomWidget.h"

StringCustomWidget::StringCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent)
    : OneCustomWidget(param, index, parent)
    , m_pLineEdit(new QLineEdit(this))
{
}

void StringCustomWidget::setParam(CameraParam& param)
{
    // 用 editingFinished 而非 textChanged：前者只在回车或失焦时发一次，不至于每敲一个
    // 字符就往相机写一次。它的坑是回车与失焦都会触发且不带文本，取值只能回到控件 text()
    disconnect(m_pLineEdit, &QLineEdit::editingFinished,
        this, &StringCustomWidget::onValueChanged);

    OneCustomWidget::setParam(param);
    StringParam varParam = m_param.GetValue().value<StringParam>();
    // StringParam 的 nMaxLength 没有下发到 setMaxLength()，控件层的输入长度不受相机约束
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

