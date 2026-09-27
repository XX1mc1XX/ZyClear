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
    // 只用一个两态勾选框：BoolParam 只带一个 bool，相机侧也没有“不确定”这种取值，
    // 因此不启用 QCheckBox::setTristate
    QCheckBox* m_pCheckBox;
};

#endif

