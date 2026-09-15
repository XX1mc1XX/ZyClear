#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include "Listener.h"
#include <QWidget>

class QFrame;
class QHBoxLayout;
class QListWidget;
class QPushButton;
class QVBoxLayout;

struct CameraMetaInfo;
class ControlWidget : public QWidget, Listener {
    Q_OBJECT

public:
    explicit ControlWidget(QWidget* parent = nullptr);
    ~ControlWidget();

    void RespondMessage(int message) override;

    CameraMetaInfo GetCurrentCameraInfo();
    CameraMetaInfo GetCameraInfo(int index);

signals:

    void SigUpdateErrorInfo(QString info);

private slots:

    void on_Enumeration_Button_clicked();

    void on_SaveConfig_Button_clicked();

    void on_LoadConfig_Button_clicked();

    void on_Camera_listWidget_currentRowChanged(int currentRow);

    void on_Connect_Button_toggled(bool checked);

private:

    void setupUi();

    QFrame* m_pControlFrame;
    QPushButton* m_pEnumerationButton;
    QPushButton* m_pConnectButton;
    QPushButton* m_pSaveConfigButton;
    QPushButton* m_pLoadConfigButton;
    QListWidget* m_pCameraListWidget;
    int m_lastCameraIndex;

    QVector<CameraMetaInfo> m_cameraMetaInfos;
};

#endif

