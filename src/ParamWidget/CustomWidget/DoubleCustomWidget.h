#ifndef DOUBLECUSTOMWIDGET_H
#define DOUBLECUSTOMWIDGET_H

#include "../OneCustomWidget.h"
#include <QDoubleSpinBox>

class DoubleCustomWidget : public OneCustomWidget
{
public:
    DoubleCustomWidget(CameraParam param, const QModelIndex& index, QWidget *parent = nullptr);
    virtual void setParam(CameraParam& param) override;
    virtual CameraParam getParam() override;

protected:
    virtual void addEditLayout(QHBoxLayout* layout) override;

protected slots:
    void onValueChanged(double value);

private:
    QDoubleSpinBox* m_SpinBox;
};

#endif

