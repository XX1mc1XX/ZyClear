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
class CameraParam;
// 参数区的容器：这里只搭空壳（两列表头 + 说明框 + 刷新按钮），真正的参数在收到
// 相机连接消息后由模型与委托运行时装配，所以本类里不会出现任何相机型号的参数名。
class ParamWidget : public QWidget, Listener {
    Q_OBJECT

public:
    explicit ParamWidget(QWidget* parent = nullptr);
    ~ParamWidget();

    // 入参是当前型号 Schema 列出的参数模板，值尚未填充，需逐个向设备读回后才有效
    void initParamWidget(QVector<CameraParam> paramList);
    void clearParamWidget();
    // 把模型里该行的参数下发到设备；由模型的 SigValueChanged 驱动，界面不直接调
    void writeCameraParam(const QModelIndex& index);
    // 相机生命周期消息的出口，把连接/断开/采集中断翻译成参数区的启停与清空
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
    // 委托父对象为空（不随本类析构），所以这个指针不能当"已托管"的凭据用；
    // 视图只借它渲染，不负责它的释放。
    CameraParamDelegate* m_pCameraParamDelegate;
    // 从视图取出，所有权仍归视图，本类不能 delete；赋值必须排在 setModel 之后，
    // 否则拿到的是 setModel 时被换掉的那个旧 selectionModel
    QItemSelectionModel* m_pSelectionModel;
};

#endif

