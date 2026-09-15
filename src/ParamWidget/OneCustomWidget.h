#ifndef ONECUSTOMWIDGET_H
#define ONECUSTOMWIDGET_H

#include "CameraInterface/ZCCameraParam.h"
#include <QLayout>
#include <QModelIndex>
#include <QWidget>

class OneCustomWidget : public QWidget {
    Q_OBJECT
public:
    explicit OneCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent = nullptr);
    void InitWidget();

    virtual void setParam(CameraParam& param);
    virtual CameraParam getParam();

signals:
    void sigValueChanged(const CameraParam& param, const QModelIndex& index);

protected:
    virtual void addEditLayout(QHBoxLayout* layout);

protected:
    CameraParam m_param;
    QModelIndex m_index;
};

#endif

