#ifndef AISETTINGSDIALOG_H
#define AISETTINGSDIALOG_H

#ifdef ZYCLEAR_HAS_AI

// 没编入 agent4cpp 时整个对话框从编译中消失，缺这套依赖的构建照样能过
#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

// Key 存本机 QSettings，不写进源码或仓库
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
