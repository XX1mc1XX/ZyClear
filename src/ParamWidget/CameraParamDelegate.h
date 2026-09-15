#ifndef CAMERAPARAMDELEGATE_H
#define CAMERAPARAMDELEGATE_H

#include "CameraInterface/ZCCameraParam.h"
#include <QModelIndex>
#include <QPainter>
#include <QStyledItemDelegate>

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
    void onValueChanged(const CameraParam& param, const QModelIndex& index);
};

#endif

