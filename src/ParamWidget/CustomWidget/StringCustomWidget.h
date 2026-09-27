#ifndef STRINGCUSTOMWIDGET_H
#define STRINGCUSTOMWIDGET_H

#include "../OneCustomWidget.h"
#include <QLineEdit>

class StringCustomWidget : public OneCustomWidget
{
public:
    StringCustomWidget(CameraParam param, const QModelIndex& index, QWidget *parent = nullptr);
    virtual void setParam(CameraParam& param) override;
    virtual CameraParam getParam() override;

protected:
    virtual void addEditLayout(QHBoxLayout* layout) override;

protected slots:
    // 槽没有参数：editingFinished 不携带新文本，只能回控件取 text()
    void onValueChanged();

private:
    QLineEdit* m_pLineEdit;
};

#endif

