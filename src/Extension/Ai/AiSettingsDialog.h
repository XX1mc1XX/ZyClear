#ifndef AISETTINGSDIALOG_H
#define AISETTINGSDIALOG_H

#ifdef ZYCLEAR_HAS_AI

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

// AI 服务配置对话框。
//
// 【为什么要有它】接口地址、模型名、API Key 都随服务商和账号而变，
// 写死在代码里等于每次换服务商都要重新编译发版。
//
// 【安全】Key 用密码框显示，存进本机 QSettings，不进源码也不进仓库。
class AiSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AiSettingsDialog(QWidget* parent = nullptr);
    ~AiSettingsDialog();

private slots:

    void onProviderChanged(int index);

    void onAccepted();

private:
    void setupUi();

    void LoadFromConfig();

    QComboBox* m_pProviderCombo;
    QLineEdit* m_pBaseUrlEdit;
    QLineEdit* m_pModelEdit;
    QLineEdit* m_pApiKeyEdit;
    QSpinBox* m_pMaxTokensSpin;
};

#endif // ZYCLEAR_HAS_AI

#endif
