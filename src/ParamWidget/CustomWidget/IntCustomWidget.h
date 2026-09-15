#ifndef INTCUSTOMWIDGET_H
#define INTCUSTOMWIDGET_H

#include "../OneCustomWidget.h"
#include <QSpinBox>

class IntCustomWidget : public OneCustomWidget
{
public:
    IntCustomWidget(CameraParam param, const QModelIndex& index, QWidget *parent = nullptr);
    virtual void setParam(CameraParam& param) override;
    virtual CameraParam getParam() override;

protected:
    virtual void addEditLayout(QHBoxLayout* layout) override;

protected slots:
    void onValueChanged(int value);

private:
    QSpinBox* m_SpinBox;
};

#endif

