#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ControlWidget/ControlWidget.h"
#include "ParamWidget/ParamWidget.h"
#include "ViewWidget/ViewWidget.h"
#include <QLabel>
#include <QMainWindow>

class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void OnUpdateErrorInfo(QString strErrorInfo);

private:

    void setupUi();

    // 三栏各自先包一层容器再挂进 splitter，多出来的这一层不是为了再套一个布局，
    // 而是为了让 objectName 落在容器上：皮肤表按 #ControlWidget / #ViewWidget /
    // #ParamWidget 这些名字选背景与边框，裸 widget 直接挂上去样式选不中。
    QSplitter* m_pSplitter;
    QWidget* m_pControlContainer;
    QWidget* m_pViewContainer;
    QWidget* m_pParamContainer;
    // 这三个在初始化列表里就 new 出来、且有意不给父对象，
    // 由构造函数体把它们塞进上面三个容器 —— 容器要等 setupUi() 建好才存在。
    ControlWidget* m_pControlWidget;
    ParamWidget* m_pParamWidget;
    ViewWidget* m_pViewWidget;
    // 状态栏里那行常驻错误文本。所有子部件通过 SigUpdateErrorInfo 汇到这里，
    // 空串表示清除。它与弹窗是两套出口，见 OnUpdateErrorInfo。
    QLabel* m_pErrorInfoLabel;
};
#endif

