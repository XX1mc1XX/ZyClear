#include "CameraParamDelegate.h"
#include "CameraParamModel.h"
#include "CustomWidget/BoolCustomWidget.h"
#include "CustomWidget/CmdCustomWidget.h"
#include "CustomWidget/DoubleCustomWidget.h"
#include "CustomWidget/EnumCustomWidget.h"
#include "CustomWidget/IntCustomWidget.h"
#include "CustomWidget/StringCustomWidget.h"
#include "OneCustomWidget.h"

CameraParamDelegate::CameraParamDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

CameraParamDelegate::~CameraParamDelegate()
{
}

QWidget* CameraParamDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)

    // 类型到控件的一对一映射：新增一种参数类型只需在这里加一支，
    // 模型、视图、上层都不必改动，这正是"新增型号不编译"落地的位置
    auto createWidget = [](CameraParam& cameraParam, const QModelIndex& index, QWidget* parent) -> OneCustomWidget* {
        switch (cameraParam.type()) {
        case STRING:
            return new StringCustomWidget(cameraParam, index, parent);
        case CMD:
            return new CmdCustomWidget(cameraParam, index, parent);
        case INT:
            return new IntCustomWidget(cameraParam, index, parent);
        case DOUBLE:
            return new DoubleCustomWidget(cameraParam, index, parent);
        case BOOL:
            return new BoolCustomWidget(cameraParam, index, parent);
        case ENUM:
            return new EnumCustomWidget(cameraParam, index, parent);
        default:
            return nullptr;
        }
    };

    if (index.column() == CameraParamModel::ColType::VALUE) {

        const QVariant varParam = index.data(CameraParamModel::ParamRole);
        CameraParam cameraParam = varParam.value<CameraParam>();

        auto* oneCustomWidget = createWidget(cameraParam, index, parent);
        if (oneCustomWidget) {
            // InitWidget 要在接管信号之前调：控件得先把自己内部的输入元素建好，
            // 否则随后 setEditorData 推值时会写进一个还没成形的控件
            oneCustomWidget->InitWidget();
            // UniqueConnection 用来容忍同一控件被重复接管，避免一次改动触发多路回写设备
            connect(oneCustomWidget, &OneCustomWidget::sigValueChanged, this,
                &CameraParamDelegate::onValueChanged, Qt::UniqueConnection);
        }
        return oneCustomWidget;
    }
    // 非值列返回空指针，视图会退回内置的默认编辑行为；名字列本就不可编辑，走不到这
    return nullptr;
}

void CameraParamDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{

    // 每次进入编辑态都重新推一次值，保证控件看到的是模型此刻的快照，
    // 而不是造控件时那一份可能已经过期的拷贝
    if (index.column() == CameraParamModel::ColType::VALUE) {
        OneCustomWidget* pCustomEdit = qobject_cast<OneCustomWidget*>(editor);

        const QVariant varParam = index.data(CameraParamModel::ParamRole);
        CameraParam cameraParam = varParam.value<CameraParam>();

        pCustomEdit->setParam(cameraParam);
    }
}

void CameraParamDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{

    if (index.column() == CameraParamModel::ColType::VALUE) {
        OneCustomWidget* pCustomEdit = qobject_cast<OneCustomWidget*>(editor);
        CameraParam cameraParam = pCustomEdit->getParam();
        // 整包写回 ParamRole，而不是按列写某个标量；控件改的只是包内 value 字段，
        // 权限位、提示语等元信息随包一起回到模型
        model->setData(index, QVariant::fromValue(cameraParam), CameraParamModel::ParamRole);
    }
}

void CameraParamDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index)
    // 铺满整格且不留边距，让控件底边与下面委托画的那条网格线严丝合缝
    editor->setGeometry(option.rect);
}

void CameraParamDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    // 不调基类 paint，改为自己画右/下两条浅灰线凑出树形网格，也不要图标与焦点框。
    // 代价是长文本不做省略截断、选中行也没把前景色切成高亮色
    painter->save();

    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    }

    QString text = index.data().toString();
    painter->drawText(option.rect, Qt::AlignLeft | Qt::AlignVCenter, text);

    QPen pen(QColor(220, 220, 220), 1, Qt::SolidLine);
    painter->setPen(pen);

    painter->drawLine(option.rect.topRight(), option.rect.bottomRight());

    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

    painter->restore();
}

QSize CameraParamDelegate::sizeHint(const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);

    // 行高写死：参数行要求密集且等高，随内容浮动会让整表在滚动时上下抖
    size.setHeight(20);
    return size;
}

void CameraParamDelegate::onValueChanged(const CameraParam& param, const QModelIndex& index)
{
    // index.model() 只给 const 指针，而写入必然要改模型，故 const_cast。
    // 控件会一直攥着这个 QModelIndex 直到编辑结束，期间若模型 clear()/reset() 过，
    // 它指向的节点已被删，所以先判 isValid 再写
    OneCustomWidget* pCustomEdit = qobject_cast<OneCustomWidget*>(sender());
    if (pCustomEdit) {
        if (index.isValid()) {

            QAbstractItemModel* model = const_cast<QAbstractItemModel*>(index.model());
            if (model) {
                model->setData(index, QVariant::fromValue(param), CameraParamModel::ParamRole);
            }
        }
    }
}

