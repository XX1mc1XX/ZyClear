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

    QSplitter* m_pSplitter;
    QWidget* m_pControlContainer;
    QWidget* m_pViewContainer;
    QWidget* m_pParamContainer;
    ControlWidget* m_pControlWidget;
    ParamWidget* m_pParamWidget;
    ViewWidget* m_pViewWidget;
    QLabel* m_pErrorInfoLabel;
};
#endif

