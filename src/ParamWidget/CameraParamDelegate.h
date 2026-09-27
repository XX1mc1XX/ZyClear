#ifndef CAMERAPARAMDELEGATE_H
#define CAMERAPARAMDELEGATE_H

#include "CameraInterface/ZCCameraParam.h"
#include <QModelIndex>
#include <QPainter>
#include <QStyledItemDelegate>

// 委托只做两件事：按参数类型造出对应的编辑控件、在控件与模型之间搬运 CameraParam。
// 它自己不存任何参数状态，真值永远只有一份，留在模型的 ParamRole 里。
class CameraParamDelegate : public QStyledItemDelegate {
public:
    CameraParamDelegate(QObject* parent = Q_NULLPTR);
    virtual ~CameraParamDelegate();

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    void setModelData(QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override;

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected slots:
    // 控件的值一变就走这里直写模型，不等编辑事务结束——本表没有"确认/取消"语义
    void onValueChanged(const CameraParam& param, const QModelIndex& index);
};

#endif

