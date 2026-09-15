#ifndef VIEWWIDGET_H
#define VIEWWIDGET_H

#include "Listener.h"
#include <QWidget>

class QPushButton;

class GraphicsView;
class ControlWidget;
class AcquireImageProcess;
class ViewWidget : public QWidget, Listener {
    Q_OBJECT

public:
    explicit ViewWidget(ControlWidget* controlWidget, QWidget* parent = nullptr);
    ~ViewWidget();
    void RespondMessage(int message) override;

signals:
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

    ControlWidget* m_pControlWidget;

    AcquireImageProcess* m_pImageProcess;
};

#endif

