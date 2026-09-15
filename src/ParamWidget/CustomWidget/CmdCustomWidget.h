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
    QPushButton* m_pCmdButton;
};

#endif

