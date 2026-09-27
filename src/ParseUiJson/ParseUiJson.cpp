#include "ParseUiJson.h"
#include <QFile>
#include <QJsonParseError>

// 字段名集中定义。它们是这份 JSON 的对外契约键，也是唯一允许改动 schema 的地方 ——
// 散成字面量后改一处漏一处，解析会静默丢字段而不是报错。
//
// 注意这些是非 static 的全局常量（外部链接）。当前只有一个目标文件包含本实现，
// 没问题；若将来把这份 .cpp 编进多个静态库，会出现重名符号的链接错误。
const QString kGroup = "group";
const QString kParams = "params";
const QString kName = "name";
const QString kType = "type";
const QString kRelativeList = "relative_list";
const QString kTips = "tips";

// 类型字符串同样是契约的一部分，与 CameraInterface/ZCCameraParam.h 里的
// ZCParamType 枚举一一对应。加了新枚举值，这里也要同步加映射，
// 否则新类型在解析时只会落成 UNKNOWN。
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
    // 双检锁：快路径不加锁，与头文件里声明的「只在启动阶段单线程调用」前提配套。
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
    // 三条失败路径（不存在、打不开、解析失败）都只是「设错误 + 置无效 + 发信号 +
    // 返回」，都不碰 m_paramList。前两条尤其要注意：此时参数表还留着上一轮的内容，
    // 于是 isValid()==false 与「表里有一批参数」会同时成立。
    // 调用方必须以此处的返回值/ isValid() 为准，别直接信任 getParamList()。
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

    // 只有这一个入口负责清理，保证「一次完整解析 = 一份干净的参数表」。
    // 清在这里而不是清在 parseJson 里，是为了让上面两条读文件失败的早退路径
    // 保持「表不动」的语义（要么拿到新表，要么连旧表一起明确作废）。
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

    // 顶层固定是「分组数组」：单个分组对象、单个参数对象都不接受。
    // 这样 JSON 里的分组顺序就直接等于界面上的分组顺序，不需要额外的排序字段。
    if (!doc.isArray()) {
        m_lastError = "JSON根节点必须是数组";
        return false;
    }

    QJsonArray rootArray = doc.array();
    int totalGroups = rootArray.size();

    // 校验是 fail-fast：遇到第一个不合规的分组或参数就整体放弃，不做「跳过坏项继续」。
    // 理由是参数表缺项会让界面显示的控件与相机实际能力对不上 ——
    // 这种不同步比「整表不加载」更难排查，宁可明确失败。
    for (int i = 0; i < rootArray.size(); ++i) {
        // current 用的是 0 基下标（下面报错文案里的 i+1 才是 1 基），
        // 语义是「正在处理第几组」，因此它永远不会到达 total。
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
            // 类型不在白名单里不报错，静默落成 UNKNOWN —— 刻意的容错：
            // schema 允许先于实现出现新类型，界面遇到 UNKNOWN 自行跳过渲染即可，
            // 不该因为它整表加载失败。
            paramInfo.type = stringToParamType(paramObj[kType].toString());
            // relative_list 是可选项，缺省与「显式空串」等价，都表示无关联项；
            // validateParamObject 只挡「写了但不是字符串」这一种错。
            paramInfo.relative_list = paramObj[kRelativeList].toString();
            paramInfo.tips = paramObj[kTips].toString();

            m_paramList.append(paramInfo);
        }
    }

    // 只有走完全部校验才会到这里；中途任何一处 return false 都会让它保持
    // loadFromByteArray 里 clear() 后的无效状态。
    m_isValid = true;
    return true;
}

bool ParseUiJson::validateParamObject(const QJsonObject& paramObj, const QString& groupName, int index)
{

    // 必填三项：name 是读写参数时的寻址键（必须与相机节点名逐字符一致），
    // type 决定界面渲染成哪种控件，tips 是界面上的说明文本来源。
    // relative_list 不在必填之列 —— 见 parseJson 里的说明。
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
    // 函数局部 static：C++11 起首次初始化是线程安全的，整张表只建一次。
    // 用 value() 取而不带默认值的 operator[]，未知类型因此返回 UNKNOWN
    // 而非插入一个默认项 —— 这个函数是 const 的，本来也不该改动任何状态。
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
    // 按值返回，但底层有隐式共享垫着，拷贝本身是 O(1)、
    // 只有调用方真的改写时才复制一份 —— 所以这里不必返回 const 引用。
    return m_paramList;
}

QList<CameraParamMetaInfo> ParseUiJson::getParamListByGroup(const QString& group) const
{
    // 线性扫描。参数表规模在千条以内，为一次筛选建索引不划算，
    // 分组名匹配也因此是精确全等：JSON 里写错大小写就是查不到。
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
    // 按首次出现顺序去重，所以输出的分组顺序就是 JSON 里的书写顺序，
    // 界面直接照着排即可，不需要再做一次排序。
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
    // 三个状态一起复位，避免出现「表已经空了、isValid 却还是 true」这种
    // 自相矛盾的中间态被外部读到。
    m_paramList.clear();
    m_lastError.clear();
    m_isValid = false;
}

