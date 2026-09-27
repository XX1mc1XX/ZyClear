#ifndef CMDCUSTOMWIDGET_H
#define CMDCUSTOMWIDGET_H

#include "../OneCustomWidget.h"
#include <QPushButton>

class CmdCustomWidget : public OneCustomWidget
{
public:
    CmdCustomWidget(CameraParam param, const QModelIndex& index, QWidget *parent = nullptr);
    virtual void setParam(CameraParam& param) override;
    virtual CameraParam getParam() override;

protected:
    virtual void addEditLayout(QHBoxLayout* layout) override;

protected slots:
    void onCmdButtonClicked();

private:
    // CmdParam 是空结构体，没有可读可写的值，本控件也就没有“设值”语义：
    // 按钮按下即等于执行一次该命令
    QPushButton* m_pCmdButton;
};

#endif

