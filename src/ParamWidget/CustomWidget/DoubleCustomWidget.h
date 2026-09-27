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
    // 用 QDoubleSpinBox 而非 QLineEdit + 校验器：相机的 DoubleParam 自带 min/max，
    // 交给控件做硬约束比事后拦非法输入省事
    QDoubleSpinBox* m_SpinBox;
};

#endif

