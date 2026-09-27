#ifndef AISETTINGSDIALOG_H
#define AISETTINGSDIALOG_H

#ifdef ZYCLEAR_HAS_AI

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
