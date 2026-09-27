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

// 虚拟相机参数表的解析器，数据源是资源里的 VirtualCameraParam.json。
// 做成单例是因为解析结果要跨相机实例复用：整张参数表只解析一次，
// 谁问给谁，避免每 new 一台虚拟相机都重读一遍资源。
class ParseUiJson : public QObject {
    Q_OBJECT

public:

    static ParseUiJson* instance();

    // 三个入口最终都汇聚到 loadFromByteArray，因此返回值口径一致：
    // false = 本次解析失败，详情见 getLastError()，同时 isValid() 被置 false。
    //
    // 关键坑：「返回 false」不等于「参数表是空的」。只有 loadFromByteArray
    // 会先 clear()，而 loadFromFile 在文件不存在/打不开时是提前返回的，
    // 上一轮成功解析出的 m_paramList 会原样留着。
    // 所以调用方要么看返回值、要么看 isValid()，只调 getParamList()
    // 会把上一次的旧数据当成这一轮的解析结果。
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

    // 构造/析构私有，只能经 instance() 取用；实例不挂父对象，与进程同寿命 ——
    // 也因此外面必须用指针而不是值语义来持有它。
    explicit ParseUiJson(QObject* parent = nullptr);
    ~ParseUiJson();

    // 禁拷贝：参数表是共享状态，复制一份出来各自改会破坏单例语义。
    ParseUiJson(const ParseUiJson&) = delete;
    ParseUiJson& operator=(const ParseUiJson&) = delete;

    bool parseJson(const QByteArray& jsonData);

    // 类型字符串→枚举的映射，未知类型不报错，落成 UNKNOWN 交给界面降级处理。
    ZCParamType stringToParamType(const QString& typeStr) const;

    // 单条参数的 schema 校验。index 是 0 基下标，仅用于拼错误文案（文案里会 +1）。
    bool validateParamObject(const QJsonObject& paramObj, const QString& groupName, int index);

private:
    static ParseUiJson* m_instance;
    static QMutex m_mutex;

    // m_mutex 只保护 instance() 的首次构造，不保护 m_paramList：
    // 解析完成后参数表被视为只读，并发读靠 Qt 容器的隐式共享与写时复制兜底。
    // m_instance 在快路径上是无锁读，严格说与初始化写之间存在数据竞争；
    // 这里接受它是因为调用点全在启动阶段的单线程路径上。
    QList<CameraParamMetaInfo> m_paramList;
    QString m_lastError;
    bool m_isValid;
};

#endif

