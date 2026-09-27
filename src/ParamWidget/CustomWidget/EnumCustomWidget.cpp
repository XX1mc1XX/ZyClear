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

    // 先清空再填：编辑器存活期间 setEditorData 可能被再次调用，
    // 不清就会把候选项叠加进去，下拉里出现重名条目
    QStringList valueList(varParam.availableValue.begin(), varParam.availableValue.end());
    m_pCombox->clear();
    m_pCombox->addItems(valueList);
    // 当前值按字符串匹配；若非可编辑下拉里找不到该文本，currentIndex 会停在 -1，
    // 界面显示空选中
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

    // valueInt 是相机侧的整型编码，不是下拉框下标 —— 两者只有在枚举编码
    // 恰好是 0..N-1 连续时才碰巧相等。相机集成层走的是 availableInt 那条路，
    // 界面这里必须与它同口径，否则在下拉里改一个枚举会往设备写进另一个值。
    // 超出编码表范围时保留原值，不把越界下标当成编码写出去
    if (value >= 0 && value < varParam.availableInt.size()) {
        varParam.valueInt = varParam.availableInt.at(value);
    }

    m_param.SetValue(QVariant::fromValue(varParam));
}

