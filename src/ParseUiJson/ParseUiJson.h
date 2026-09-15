#ifndef PARSEUIJSON_H
#define PARSEUIJSON_H

#include "../CameraInterface/ZCCameraParam.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMutex>
#include <QObject>

class ParseUiJson : public QObject {
    Q_OBJECT

public:

    static ParseUiJson* instance();

    bool loadFromFile(const QString& filePath);

    bool loadFromString(const QString& jsonString);

    bool loadFromByteArray(const QByteArray& jsonData);

    QList<CameraParamMetaInfo> getParamList() const;

    QList<CameraParamMetaInfo> getParamListByGroup(const QString& group) const;

    QStringList getAllGroups() const;

    void clear();

    QString getLastError() const { return m_lastError; }

    bool isValid() const { return m_isValid; }

signals:
    void parseFinished(bool success, const QString& error);
    void parseProgress(int current, int total);

private:

    explicit ParseUiJson(QObject* parent = nullptr);
    ~ParseUiJson();

    ParseUiJson(const ParseUiJson&) = delete;
    ParseUiJson& operator=(const ParseUiJson&) = delete;

    bool parseJson(const QByteArray& jsonData);

    ZCParamType stringToParamType(const QString& typeStr) const;

    bool validateParamObject(const QJsonObject& paramObj, const QString& groupName, int index);

private:
    static ParseUiJson* m_instance;
    static QMutex m_mutex;

    QList<CameraParamMetaInfo> m_paramList;
    QString m_lastError;
    bool m_isValid;
};

#endif

