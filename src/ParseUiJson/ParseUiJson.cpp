#include "ParseUiJson.h"
#include <QFile>
#include <QJsonParseError>

const QString kGroup = "group";
const QString kParams = "params";
const QString kName = "name";
const QString kType = "type";
const QString kRelativeList = "relative_list";
const QString kTips = "tips";

const QString kInt = "INT";
const QString kDouble = "DOUBLE";
const QString kEnum = "ENUM";
const QString kBool = "BOOL";
const QString kCmd = "CMD";
const QString kString = "STRING";

ParseUiJson* ParseUiJson::m_instance = nullptr;
QMutex ParseUiJson::m_mutex;

ParseUiJson::ParseUiJson(QObject* parent)
    : QObject(parent)
    , m_isValid(false)
{
}

ParseUiJson::~ParseUiJson()
{
}

ParseUiJson* ParseUiJson::instance()
{
    if (!m_instance) {
        QMutexLocker locker(&m_mutex);
        if (!m_instance) {
            m_instance = new ParseUiJson();
        }
    }
    return m_instance;
}

bool ParseUiJson::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        m_lastError = QString("文件不存在: %1").arg(filePath);
        m_isValid = false;
        emit parseFinished(false, m_lastError);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("无法打开文件: %1").arg(file.errorString());
        m_isValid = false;
        emit parseFinished(false, m_lastError);
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    return loadFromByteArray(jsonData);
}

bool ParseUiJson::loadFromString(const QString& jsonString)
{
    return loadFromByteArray(jsonString.toUtf8());
}

bool ParseUiJson::loadFromByteArray(const QByteArray& jsonData)
{

    clear();

    bool success = parseJson(jsonData);
    emit parseFinished(success, m_lastError);

    return success;
}

bool ParseUiJson::parseJson(const QByteArray& jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = QString("JSON解析错误: %1 (位置: %2)")
                          .arg(parseError.errorString())
                          .arg(parseError.offset);
        return false;
    }

    if (!doc.isArray()) {
        m_lastError = "JSON根节点必须是数组";
        return false;
    }

    QJsonArray rootArray = doc.array();
    int totalGroups = rootArray.size();

    for (int i = 0; i < rootArray.size(); ++i) {
        emit parseProgress(i, totalGroups);

        QJsonValue groupValue = rootArray.at(i);
        if (!groupValue.isObject()) {
            m_lastError = QString("第%1个元素不是对象格式").arg(i + 1);
            return false;
        }

        QJsonObject groupObj = groupValue.toObject();

        if (!groupObj.contains(kGroup) || !groupObj[kGroup].isString()) {
            m_lastError = QString("第%1个分组缺少group字段或group不是字符串").arg(i + 1);
            return false;
        }

        QString groupName = groupObj[kGroup].toString();

        if (!groupObj.contains(kParams) || !groupObj[kParams].isArray()) {
            m_lastError = QString("分组'%1'缺少params字段或params不是数组").arg(groupName);
            return false;
        }

        QJsonArray paramsArray = groupObj[kParams].toArray();

        for (int j = 0; j < paramsArray.size(); ++j) {
            QJsonValue paramValue = paramsArray.at(j);
            if (!paramValue.isObject()) {
                m_lastError = QString("分组'%1'的第%2个参数不是对象格式")
                                  .arg(groupName)
                                  .arg(j + 1);
                return false;
            }

            QJsonObject paramObj = paramValue.toObject();

            if (!validateParamObject(paramObj, groupName, j)) {
                return false;
            }

            CameraParamMetaInfo paramInfo;
            paramInfo.group = groupName;
            paramInfo.name = paramObj[kName].toString();
            paramInfo.type = stringToParamType(paramObj[kType].toString());
            paramInfo.relative_list = paramObj[kRelativeList].toString();
            paramInfo.tips = paramObj[kTips].toString();

            m_paramList.append(paramInfo);
        }
    }

    m_isValid = true;
    return true;
}

bool ParseUiJson::validateParamObject(const QJsonObject& paramObj, const QString& groupName, int index)
{

    QStringList requiredFields = { kName, kType, kTips };
    for (const QString& field : requiredFields) {
        if (!paramObj.contains(field)) {
            m_lastError = QString("分组'%1'的第%2个参数缺少'%3'字段")
                              .arg(groupName)
                              .arg(index + 1)
                              .arg(field);
            return false;
        }
    }

    if (!paramObj[kName].isString()) {
        m_lastError = QString("分组'%1'的第%2个参数的'name'字段必须是字符串")
                          .arg(groupName)
                          .arg(index + 1);
        return false;
    }

    if (!paramObj[kType].isString()) {
        m_lastError = QString("分组'%1'的第%2个参数的'type'字段必须是字符串")
                          .arg(groupName)
                          .arg(index + 1);
        return false;
    }

    if (paramObj.contains(kRelativeList) && !paramObj[kRelativeList].isString()) {
        m_lastError = QString("分组'%1'的第%2个参数的'relative_list'字段必须是字符串")
                          .arg(groupName)
                          .arg(index + 1);
        return false;
    }

    if (!paramObj[kTips].isString()) {
        m_lastError = QString("分组'%1'的第%2个参数的'tips'字段必须是字符串")
                          .arg(groupName)
                          .arg(index + 1);
        return false;
    }

    return true;
}

ZCParamType ParseUiJson::stringToParamType(const QString& typeStr) const
{
    static QMap<QString, ZCParamType> typeMap = {
        { kInt, INT },
        { kDouble, DOUBLE },
        { kEnum, ENUM },
        { kBool, BOOL },
        { kCmd, CMD },
        { kString, STRING }
    };

    return typeMap.value(typeStr, UNKNOWN);
}

QList<CameraParamMetaInfo> ParseUiJson::getParamList() const
{
    return m_paramList;
}

QList<CameraParamMetaInfo> ParseUiJson::getParamListByGroup(const QString& group) const
{
    QList<CameraParamMetaInfo> result;
    for (const CameraParamMetaInfo& param : m_paramList) {
        if (param.group == group) {
            result.append(param);
        }
    }
    return result;
}

QStringList ParseUiJson::getAllGroups() const
{
    QStringList groups;
    for (const CameraParamMetaInfo& param : m_paramList) {
        if (!groups.contains(param.group)) {
            groups.append(param.group);
        }
    }
    return groups;
}

void ParseUiJson::clear()
{
    m_paramList.clear();
    m_lastError.clear();
    m_isValid = false;
}

