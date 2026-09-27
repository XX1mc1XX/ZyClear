#ifndef ONECUSTOMWIDGET_H
#define ONECUSTOMWIDGET_H

#include "CameraInterface/ZCCameraParam.h"
#include <QLayout>
#include <QModelIndex>
#include <QWidget>

class OneCustomWidget : public QWidget {
    Q_OBJECT
public:
    // 按类型运行时挂载：CameraParamDelegate 只在单元格被打开时，用参数自带的 type()
    // 现场构造对应子类，视图与模型始终只认基类指针，多一种参数类型不必改界面层。
    // 构造完还得由外部再调一次 InitWidget()：它要派发虚函数 addEditLayout()，而构造期
    // 调用只会落到基类的空实现，子类那个编辑控件就挂不进布局。
    explicit OneCustomWidget(CameraParam param, const QModelIndex& index, QWidget* parent = nullptr);
    void InitWidget();

    // 约定子类先调基类版本再同步自身控件：基类整体覆盖 m_param（含 meta 与读写权限位），
    // 子类只把值灌进控件。灌值前必须断开控件的值变更信号，因为 setText/setValue 这类编程式
    // 设值同样会发信号，不断开就会经 onValueChanged 回程再走一次 setParam，读写互为因果成死循环。
    virtual void setParam(CameraParam& param);
    // 值语义快照，可能已被 onValueChanged 就地更新；delegate 在 setModelData 里拿它写回模型。
    virtual CameraParam getParam();

signals:
    // 编辑即时上报，带自身 m_index 供 delegate 定位单元格。CMD 型参数没有值，
    // 完全靠“发出这个信号”本身表示执行一次命令。
    void sigValueChanged(const CameraParam& param, const QModelIndex& index);

protected:
    // 只由 InitWidget() 调用，子类在此把唯一那个编辑控件加进水平布局。
    virtual void addEditLayout(QHBoxLayout* layout);

protected:
    CameraParam m_param;
    // 只在该模型未重置期间有效，模型 clear()/reset 后即失效，不可跨刷新长期持有。
    QModelIndex m_index;
};

#endif

