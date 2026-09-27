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
    // 范围与步长完全来自相机上报（IntParam 的 min/max/increment），界面不做任何写死，
    // 这一点与 Double 相反——后者的参数结构里根本没有增量字段
    QSpinBox* m_SpinBox;
};

#endif

