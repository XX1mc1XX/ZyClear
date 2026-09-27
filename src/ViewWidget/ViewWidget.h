#ifndef VIEWWIDGET_H
#define VIEWWIDGET_H

#include "Listener.h"
#include <QWidget>

class QPushButton;

class GraphicsView;
class AcquireImageProcess;
// 预览面板只承担"看"：唯一的相机控制入口是拉流开关，其余状态变化一律
// 依赖 Listener 广播回来驱动（见 RespondMessage），刻意不在面板里缓存一份
// 会与 CameraContext 走偏的开流状态。
class ViewWidget : public QWidget, Listener {
    Q_OBJECT

public:
    explicit ViewWidget(QWidget* parent = nullptr);
    ~ViewWidget();
    void RespondMessage(int message) override;

signals:
    // 名字被 CameraContext 的 CHECK_RETURN 宏写死，且各面板共用同一个错误
    // 出口，改名会让宏在别处静默编译不过。
    void SigUpdateErrorInfo(QString info);

private slots:
    void on_Grabbing_Button_toggled(bool checked);

private:

    void setupUi();
    void setStarGrabbingState(bool state);

private:
    QPushButton* m_pGrabbingButton;
    QWidget* m_pViewBoxContainer;
    GraphicsView* m_pViewBox;

    // 取帧线程刻意不挂 parent：Qt 父子树回收会与"线程仍阻塞在取帧"撞车，
    // 生命周期改由拉流开关显式控制（start/stop），不走析构链。
    AcquireImageProcess* m_pImageProcess;
};

#endif

