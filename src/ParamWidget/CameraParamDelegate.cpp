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
            oneCustomWidget->InitWidget();
            connect(oneCustomWidget, &OneCustomWidget::sigValueChanged, this,
                &CameraParamDelegate::onValueChanged, Qt::UniqueConnection);
        }
        return oneCustomWidget;
    }
    return nullptr;
}

void CameraParamDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{

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
        model->setData(index, QVariant::fromValue(cameraParam), CameraParamModel::ParamRole);
    }
}

void CameraParamDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index)
    editor->setGeometry(option.rect);
}

void CameraParamDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
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

    size.setHeight(20);
    return size;
}

void CameraParamDelegate::onValueChanged(const CameraParam& param, const QModelIndex& index)
{
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

