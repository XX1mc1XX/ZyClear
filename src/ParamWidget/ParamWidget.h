#ifndef PARAMWIDGET_H
#define PARAMWIDGET_H

#include "Listener.h"
#include <QTextBrowser>
#include <QTreeView>
#include <QVector>
#include <QWidget>

class QPushButton;
class QSplitter;

class CameraParamDelegate;
class CameraParamModel;
class ControlWidget;
class CameraParam;
class ParamWidget : public QWidget, Listener {
    Q_OBJECT

public:
    // ParamWidget(QWidget *parent = nullptr);
    explicit ParamWidget(ControlWidget* controlWidget, QWidget* parent = nullptr);
    ~ParamWidget();

    void initParamWidget(QVector<CameraParam> paramList);
    void clearParamWidget();
    void writeCameraParam(const QModelIndex& index);
    void RespondMessage(int message) override;

signals:
    void SigUpdateErrorInfo(QString info);

public slots:
    void OnUpdataSelection(const QItemSelection& selected, const QItemSelection& deselected);

private slots:
    void on_Refresh_Button_clicked();

private:

    void setupUi();

    QPushButton* m_pRefreshButton;
    QSplitter* m_pSplitter;
    QTreeView* m_pParamTreeView;
    QTextBrowser* m_pParamDescript;
    CameraParamModel* m_pModel;
    CameraParamDelegate* m_pCameraParamDelegate;
    QItemSelectionModel* m_pSelectionModel;

    ControlWidget* m_pControlWidget;
};

#endif

