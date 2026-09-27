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
// 多继承：QWidget 给界面与信号槽，Listener 只是一层纯虚接口。
// Listener 不是 QObject，所以 RespondMessage 是普通虚调用、不走元对象系统，
// 也因此它只在创建本部件的线程（主线程）上被总线调到。
class ControlWidget : public QWidget, Listener {
    Q_OBJECT

public:
    explicit ControlWidget(QWidget* parent = nullptr);
    ~ControlWidget();

    void RespondMessage(int message) override;

    // 两个查询都带越界保护：索引非法时返回默认构造的元信息，
    // 其 Serial 是 null QString —— 调用方统一用 serial.isNull() 判断
    // 「当前没有可用相机」，而不是去比对列表行号。
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
    // 上一次选中的行号，用于切相机时把「上一台」断连。
    // 它在重新枚举时不会复位，而行号语义在枚举后已变（旧索引可能指向另一台
    // 甚至越界），所以切回首项的瞬间那一次断连对象可能是错的 ——
    // 好在断连本身是幂等的，错的那次只会打到一台本就未连接的相机上。
    int m_lastCameraIndex;

    QVector<CameraMetaInfo> m_cameraMetaInfos;
};

#endif

