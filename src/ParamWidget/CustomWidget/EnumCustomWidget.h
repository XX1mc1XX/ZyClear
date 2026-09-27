#ifndef ENUMCUSTOMWIDGET_H
#define ENUMCUSTOMWIDGET_H

#include "../OneCustomWidget.h"
#include <QComboBox>

class EnumCustomWidget : public OneCustomWidget
{
public:
    EnumCustomWidget(CameraParam param, const QModelIndex& index, QWidget *parent = nullptr);
    virtual void setParam(CameraParam& param) override;
    virtual CameraParam getParam() override;

protected:
    virtual void addEditLayout(QHBoxLayout* layout) override;

protected slots:
    void onValueChanged(int value);

private:
    // 下拉框只展示 EnumParam.availableValue 里的字符串；相机侧的整数编码
    // （valueInt/availableInt）对界面不可见，回写时才会用到
    QComboBox* m_pCombox;
};

#endif

