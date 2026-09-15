#ifndef BOOLCUSTOMWIDGET_H
#define BOOLCUSTOMWIDGET_H

#include "../OneCustomWidget.h"
#include <QCheckBox>

class BoolCustomWidget : public OneCustomWidget
{
public:
    BoolCustomWidget(CameraParam param, const QModelIndex& index, QWidget *parent = nullptr);
    virtual void setParam(CameraParam& param) override;
    virtual CameraParam getParam() override;

protected:
    virtual void addEditLayout(QHBoxLayout* layout) override;

protected slots:
    void onValueChanged(bool checked);

private:
    QCheckBox* m_pCheckBox;
};

#endif

