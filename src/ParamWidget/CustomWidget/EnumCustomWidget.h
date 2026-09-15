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
    QComboBox* m_pCombox;
};

#endif

