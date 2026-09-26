#include "CameraToolProvider.h"

#ifdef ZYCLEAR_HAS_AI

#include "CameraInterface/CameraContext.h"
#include "CameraInterface/CameraError.h"
#include "CameraInterface/ZCCameraMetaInfo.h"
#include "CameraInterface/ZCCameraParam.h"

#include "agent4cpp/status.h"
#include "agent4cpp/tool.h"
#include "agent4cpp/tool_registry.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QString>
#include <QThread>
#include <QVector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <functional>
#include <string>

namespace {

// =============================================================================
// 一、跨界工具：agent4cpp 的 ToolResult 构造
// =============================================================================

std::string ToJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}

// 成功的返回：一句人话 + 结构化数据
agent4cpp::ToolResult MakeOk(const QString& content, const QJsonObject& payload)
{
    agent4cpp::ToolResult result;
    result.status = agent4cpp::Status::Ok();
    result.content = content.toStdString();
    result.payload_json = ToJson(payload);
    return result;
}

// 失败的返回。
//
// ★ 失败也照样回灌给模型（不是中断循环）——模型看到原因才能自我修正。
//   所以这里的文案是写给模型看的：说清「哪里错、错在哪、可接受范围是什么」，
//   而不是只丢一句 "failed"。
agent4cpp::ToolResult MakeError(const QString& content, const QJsonObject& payload = {})
{
    agent4cpp::ToolResult result;
    result.status = agent4cpp::Status::FailedPrecondition(content.toStdString());
    result.content = content.toStdString();
    result.payload_json = ToJson(payload);
    return result;
}

// 【为什么需要它】AI 循环跑在后台线程，而 CameraContext 背后的
// CameraInterface 不是线程安全的，用户也可能同时在点界面。
// 所以所有相机操作统一切回主线程执行，天然和界面操作串行，
// 不需要给相机的每个方法加锁。
//
// 代价：调用期间主线程的事件队列被这个任务占用。但单次工具调用是毫秒级
// （虚拟相机更快），真正耗时的「等模型回复」在后台线程，所以界面不会卡。
agent4cpp::ToolResult RunOnUiThread(const std::function<agent4cpp::ToolResult()>& body)
{
    QCoreApplication* app = QCoreApplication::instance();
    if (app == nullptr || QThread::currentThread() == app->thread()) {
        return body();
    }

    agent4cpp::ToolResult result;
    QMetaObject::invokeMethod(app, [&body, &result]() { result = body(); },
        Qt::BlockingQueuedConnection);
    return result;
}

// 把「收 JSON、在主线程里干活」的函数包装成 agent4cpp 要的 ToolInvoker。
// 参数解析留在后台线程（不需要碰相机），只有 body 切回主线程。
agent4cpp::ToolInvoker UiTool(std::function<agent4cpp::ToolResult(const QJsonObject&)> body)
{
    return [body](const std::string& arguments_json) -> agent4cpp::ToolResult {
        const QByteArray raw(arguments_json.data(), static_cast<int>(arguments_json.size()));
        QJsonParseError parseError {};
        const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            return MakeError(QStringLiteral("参数不是合法 JSON：%1").arg(parseError.errorString()));
        }

        const QJsonObject arguments = document.isObject() ? document.object() : QJsonObject();
        return RunOnUiThread([&body, arguments]() { return body(arguments); });
    };
}

// =============================================================================
// 二、参数解析的小工具
// =============================================================================

// 工具大多作用于「某台相机」。没显式给序列号就用界面上当前选中的那台，
// 这样用户在界面上选好相机后可以直接说「曝光调亮一点」。
QString ResolveSerial(const QJsonObject& arguments, QString* error)
{
    QString serial = arguments.value(QStringLiteral("serial")).toString().trimmed();
    if (serial.isEmpty()) {
        serial = CameraContext::Instance()->currentSerial();
    }
    if (serial.isEmpty()) {
        *error = QStringLiteral("没有指定相机，界面上也没有选中任何相机。请先在左侧列表里选中并连接一台相机。");
        return QString();
    }
    return serial;
}

// 错误码 → 给模型看的中文说明。
// 直接丢代码（0x0005）模型看不懂，也就没法决定下一步怎么办。
QString DescribeError(uint32_t code)
{
    return QStringLiteral("%1（错误码 %2）")
        .arg(getErrorInfoEn(code))
        .arg(code, 0, 16);
}

// 把 JSON 值写成一句人能读的话，用于错误提示里回显模型给的那个值
QString DescribeJsonValue(const QJsonValue& value)
{
    if (value.isString()) {
        return QStringLiteral("\"%1\"").arg(value.toString());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isNull() || value.isUndefined()) {
        return QStringLiteral("空值");
    }
    return QStringLiteral("（复合值）");
}

// 把 CameraParam 转成 JSON。只读项默认不返回，避免 37 个参数的清单把上下文塞满。
QJsonObject ParamToJson(const CameraParam& param, bool includeValue)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), param.name());
    object.insert(QStringLiteral("group"), param.group());
    object.insert(QStringLiteral("readable"), param.isReadable());
    object.insert(QStringLiteral("writeable"), param.isWriteable());

    if (!param.tips().isEmpty()) {
        object.insert(QStringLiteral("tips"), param.tips());
    }

    switch (param.type()) {
    case INT: {
        const IntParam value = param.GetValue().value<IntParam>();
        object.insert(QStringLiteral("type"), QStringLiteral("int"));
        object.insert(QStringLiteral("min"), static_cast<double>(value.min));
        object.insert(QStringLiteral("max"), static_cast<double>(value.max));
        if (includeValue) {
            object.insert(QStringLiteral("value"), static_cast<double>(value.value));
        }
        break;
    }
    case DOUBLE: {
        const DoubleParam value = param.GetValue().value<DoubleParam>();
        object.insert(QStringLiteral("type"), QStringLiteral("double"));
        object.insert(QStringLiteral("min"), value.min);
        object.insert(QStringLiteral("max"), value.max);
        if (includeValue) {
            object.insert(QStringLiteral("value"), value.value);
        }
        break;
    }
    case BOOL: {
        const BoolParam value = param.GetValue().value<BoolParam>();
        object.insert(QStringLiteral("type"), QStringLiteral("bool"));
        if (includeValue) {
            object.insert(QStringLiteral("value"), value.value);
        }
        break;
    }
    case STRING: {
        const StringParam value = param.GetValue().value<StringParam>();
        object.insert(QStringLiteral("type"), QStringLiteral("string"));
        if (includeValue) {
            object.insert(QStringLiteral("value"), value.value);
        }
        break;
    }
    case ENUM: {
        const EnumParam value = param.GetValue().value<EnumParam>();
        object.insert(QStringLiteral("type"), QStringLiteral("enum"));
        QJsonArray options;
        for (const QString& item : value.availableValue) {
            options.append(item);
        }
        object.insert(QStringLiteral("options"), options);
        if (includeValue) {
            object.insert(QStringLiteral("value"), value.value);
        }
        break;
    }
    case CMD: {
        object.insert(QStringLiteral("type"), QStringLiteral("command"));
        object.insert(QStringLiteral("note"), QStringLiteral("这是一个命令参数，通常不支持直接写值"));
        break;
    }
    default: {
        object.insert(QStringLiteral("type"), QStringLiteral("unknown"));
        break;
    }
    }

    return object;
}

// 从设备上把某台相机的参数全部读出来。找不到相机或读失败时返回错误说明。
QString FetchParams(const QString& serial, QVector<CameraParam>* params)
{
    const uint32_t code = CameraContext::Instance()->getParamList(serial, *params);
    if (code != ZYCLEAR_OK) {
        return DescribeError(code);
    }
    return QString();
}

// 在参数表里按名字找一个（设备上报的名字区分大小写，这里做宽松匹配更好用）
bool FindParam(const QVector<CameraParam>& params, const QString& name, CameraParam* out)
{
    for (const CameraParam& param : params) {
        if (param.name() == name) {
            *out = param;
            return true;
        }
    }
    // 退一步做不区分大小写的匹配
    for (const CameraParam& param : params) {
        if (param.name().compare(name, Qt::CaseInsensitive) == 0) {
            *out = param;
            return true;
        }
    }
    return false;
}

// ★ 把模型给的值装进 CameraParam，并按类型和范围校验。
//
//   这是整层最容易出错的地方：模型给的值来自一段自然语言推理，
//   可能超范围、可能类型不对、可能给个设备根本不认的枚举项。
//   在这里拦住并说清原因，模型下一轮就会改对；
//   放过去则要么写失败、要么写进去一个非法值。
//
//   返回空字符串表示成功，否则是给模型看的错误说明。
QString ApplyValue(CameraParam& param, const QJsonValue& value)
{
    switch (param.type()) {
    case INT: {
        IntParam target = param.GetValue().value<IntParam>();

        // 宽容解析：模型可能给数字 8000，也可能给字符串 "8000"。
        // 工具签名把这些值声明成字符串参数，所以字符串形式反而更常见，
        // 只认数字会把一大批本来正确的调用判死。
        bool parsed = false;
        int64_t number = 0;
        if (value.isDouble()) {
            number = static_cast<int64_t>(value.toDouble());
            parsed = true;
        } else if (value.isString()) {
            number = value.toString().trimmed().toLongLong(&parsed);
        }
        if (!parsed) {
            return QStringLiteral("参数 %1 需要整数，收到的是 %2")
                .arg(param.name(), DescribeJsonValue(value));
        }
        if (number < target.min || number > target.max) {
            return QStringLiteral("参数 %1 超出范围：%2 不在 [%3, %4] 内")
                .arg(param.name())
                .arg(number)
                .arg(target.min)
                .arg(target.max);
        }
        target.value = number;
        param.SetValue(QVariant::fromValue(target));
        return QString();
    }
    case DOUBLE: {
        DoubleParam target = param.GetValue().value<DoubleParam>();

        bool parsed = false;
        double number = 0.0;
        if (value.isDouble()) {
            number = value.toDouble();
            parsed = true;
        } else if (value.isString()) {
            number = value.toString().trimmed().toDouble(&parsed);
        }
        if (!parsed) {
            return QStringLiteral("参数 %1 需要数字，收到的是 %2")
                .arg(param.name(), DescribeJsonValue(value));
        }
        if (number < target.min || number > target.max) {
            return QStringLiteral("参数 %1 超出范围：%2 不在 [%3, %4] 内")
                .arg(param.name())
                .arg(number)
                .arg(target.min)
                .arg(target.max);
        }
        target.value = number;
        param.SetValue(QVariant::fromValue(target));
        return QString();
    }
    case BOOL: {
        BoolParam target = param.GetValue().value<BoolParam>();
        if (value.isBool()) {
            target.value = value.toBool();
        } else if (value.isDouble()) {
            target.value = value.toDouble() != 0.0;
        } else if (value.isString()) {
            const QString text = value.toString().trimmed().toLower();
            if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
                target.value = true;
            } else if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
                target.value = false;
            } else {
                return QStringLiteral("参数 %1 是布尔类型，无法把 \"%2\" 当成真假值").arg(param.name(), text);
            }
        } else {
            return QStringLiteral("参数 %1 是布尔类型，需要 true 或 false").arg(param.name());
        }
        param.SetValue(QVariant::fromValue(target));
        return QString();
    }
    case STRING: {
        StringParam target = param.GetValue().value<StringParam>();
        if (!value.isString()) {
            return QStringLiteral("参数 %1 是字符串类型，需要传字符串").arg(param.name());
        }
        const QString text = value.toString();
        if (target.nMaxLength > 0 && static_cast<unsigned int>(text.size()) > target.nMaxLength) {
            return QStringLiteral("参数 %1 最长 %2 个字符，收到 %3 个")
                .arg(param.name())
                .arg(target.nMaxLength)
                .arg(text.size());
        }
        target.value = text;
        param.SetValue(QVariant::fromValue(target));
        return QString();
    }
    case ENUM: {
        EnumParam target = param.GetValue().value<EnumParam>();

        int index = -1;
        if (value.isString()) {
            index = target.availableValue.indexOf(value.toString());
            if (index < 0) {
                index = target.availableValue.indexOf(value.toString(), 0);
            }
        } else if (value.isDouble()) {
            // 也允许用枚举序号来指定
            const int number = static_cast<int>(value.toDouble());
            if (number >= 0 && number < target.availableValue.size()) {
                index = number;
            }
        }

        if (index < 0) {
            // ★ 把可选项全列出来。模型看到清单下一轮就能改对，
            //   这比只回一句「枚举值无效」有用得多。
            QStringList options;
            for (const QString& item : target.availableValue) {
                options << item;
            }
            return QStringLiteral("参数 %1 的可选值有：[%2]，收到的 %3 不在其中")
                .arg(param.name(), options.join(QStringLiteral(", ")), DescribeJsonValue(value));
        }

        target.value = target.availableValue.at(index);
        if (index < target.availableInt.size()) {
            target.valueInt = target.availableInt.at(index);
        }
        param.SetValue(QVariant::fromValue(target));
        return QString();
    }
    case CMD: {
        return QStringLiteral("参数 %1 是命令型参数，不能用 set_param 写值").arg(param.name());
    }
    default:
        break;
    }

    return QStringLiteral("参数 %1 的类型未知，无法写入").arg(param.name());
}

// =============================================================================
// 三、各个工具
// =============================================================================

agent4cpp::ToolResult ToolListCameras(const QJsonObject& arguments)
{
    const bool refresh = arguments.value(QStringLiteral("refresh")).toBool(false);

    if (!refresh) {
        // ★ 不做扫描。扫描（EnumerationCamera）会把已连接的相机全部断开再重建注册表，
        //   这是个有副作用的操作，不能因为模型随口一问就执行。
        const QString current = CameraContext::Instance()->currentSerial();
        QJsonObject payload;
        payload.insert(QStringLiteral("current_serial"), current);
        payload.insert(QStringLiteral("refreshed"), false);
        payload.insert(QStringLiteral("hint"),
            QStringLiteral("如需重新扫描设备，用 refresh=true 调用；"
                           "注意扫描会断开所有已连接的相机。"));

        QString content = current.isEmpty()
            ? QStringLiteral("当前没有选中相机。")
            : QStringLiteral("当前选中的相机：%1").arg(current);
        return MakeOk(content, payload);
    }

    QVector<CameraMetaInfo> infos;
    const uint32_t code = CameraContext::Instance()->EnumerationCamera(infos);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("扫描设备失败：%1").arg(DescribeError(code)));
    }

    QJsonArray cameras;
    for (const CameraMetaInfo& info : infos) {
        QJsonObject item;
        item.insert(QStringLiteral("serial"), info.Serial);
        item.insert(QStringLiteral("vendor"), info.VenderName);
        if (!info.UserDefineID.isEmpty()) {
            item.insert(QStringLiteral("user_name"), info.UserDefineID);
        }
        cameras.append(item);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("cameras"), cameras);
    payload.insert(QStringLiteral("count"), infos.size());
    payload.insert(QStringLiteral("current_serial"), CameraContext::Instance()->currentSerial());
    payload.insert(QStringLiteral("note"),
        QStringLiteral("扫描已断开所有相机的原有连接，需要在界面上重新连接。"));

    return MakeOk(QStringLiteral("扫描到 %1 台设备。注意：扫描断开了原来的连接。").arg(infos.size()), payload);
}

agent4cpp::ToolResult ToolCurrentCamera(const QJsonObject& arguments)
{
    Q_UNUSED(arguments)

    CameraContext* context = CameraContext::Instance();
    const QString serial = context->currentSerial();

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);

    if (serial.isEmpty()) {
        payload.insert(QStringLiteral("connected"), false);
        payload.insert(QStringLiteral("grabbing"), false);
        return MakeOk(QStringLiteral("界面上当前没有选中任何相机。"), payload);
    }

    bool connected = false;
    bool grabbing = false;
    context->isConnect(serial, connected);
    context->isGrabbing(serial, grabbing);

    payload.insert(QStringLiteral("connected"), connected);
    payload.insert(QStringLiteral("grabbing"), grabbing);

    return MakeOk(QStringLiteral("当前相机 %1：%2，%3。")
                      .arg(serial,
                          connected ? QStringLiteral("已连接") : QStringLiteral("未连接"),
                          grabbing ? QStringLiteral("正在拉流") : QStringLiteral("未拉流")),
        payload);
}

agent4cpp::ToolResult ToolConnectCamera(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    CameraContext* context = CameraContext::Instance();

    bool connected = false;
    if (context->isConnect(serial, connected) == ZYCLEAR_OK && connected) {
        QJsonObject payload;
        payload.insert(QStringLiteral("serial"), serial);
        payload.insert(QStringLiteral("connected"), true);
        return MakeOk(QStringLiteral("相机 %1 已经处于连接状态，无需重复连接。").arg(serial), payload);
    }

    const uint32_t code = context->connect(serial);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("连接相机 %1 失败：%2").arg(serial, DescribeError(code)));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("connected"), true);
    return MakeOk(QStringLiteral("相机 %1 已连接。").arg(serial), payload);
}

agent4cpp::ToolResult ToolDisconnectCamera(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    CameraContext* context = CameraContext::Instance();
    const uint32_t code = context->disconnect(serial);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("断开相机 %1 失败：%2").arg(serial, DescribeError(code)));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("connected"), false);
    return MakeOk(QStringLiteral("相机 %1 已断开。").arg(serial), payload);
}

agent4cpp::ToolResult ToolListParams(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    const bool includeReadonly = arguments.value(QStringLiteral("include_readonly")).toBool(false);
    const QString groupFilter = arguments.value(QStringLiteral("group")).toString().trimmed();

    QVector<CameraParam> params;
    error = FetchParams(serial, &params);
    if (!error.isEmpty()) {
        return MakeError(QStringLiteral("读取相机 %1 的参数失败：%2").arg(serial, error));
    }

    QJsonArray items;
    int total = 0;
    int writeable = 0;
    for (const CameraParam& param : params) {
        if (!param.isValid()) {
            continue; // 设备不支持这一项，直接跳过
        }
        ++total;
        if (param.isWriteable()) {
            ++writeable;
        }
        if (!includeReadonly && !param.isWriteable()) {
            continue;
        }
        if (!groupFilter.isEmpty() && param.group() != groupFilter) {
            continue;
        }
        items.append(ParamToJson(param, true));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("total"), total);
    payload.insert(QStringLiteral("writeable"), writeable);
    payload.insert(QStringLiteral("returned"), items.size());
    payload.insert(QStringLiteral("params"), items);
    if (!includeReadonly) {
        payload.insert(QStringLiteral("note"),
            QStringLiteral("默认只列出可写参数。需要看全部（含只读）时传 include_readonly=true。"));
    }

    return MakeOk(QStringLiteral("相机 %1 共 %2 项参数，其中 %3 项可写，本次返回 %4 项。")
                      .arg(serial)
                      .arg(total)
                      .arg(writeable)
                      .arg(items.size()),
        payload);
}

agent4cpp::ToolResult ToolGetParam(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    const QString name = arguments.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
        return MakeError(QStringLiteral("必须指定参数名 name。"));
    }

    QVector<CameraParam> params;
    error = FetchParams(serial, &params);
    if (!error.isEmpty()) {
        return MakeError(QStringLiteral("读取相机 %1 的参数失败：%2").arg(serial, error));
    }

    CameraParam target;
    if (!FindParam(params, name, &target)) {
        return MakeError(QStringLiteral("相机 %1 上没有名为 %2 的参数。"
                                        "先调 list_params 看看有哪些可选参数。")
                             .arg(serial, name));
    }

    const QJsonObject payload = ParamToJson(target, true);
    return MakeOk(QStringLiteral("参数 %1 当前值为 %2。").arg(name, target.displayText()), payload);
}

agent4cpp::ToolResult ToolSetParam(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    const QString name = arguments.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
        return MakeError(QStringLiteral("必须指定参数名 name。"));
    }
    if (!arguments.contains(QStringLiteral("value"))) {
        return MakeError(QStringLiteral("必须指定参数值 value。"));
    }

    CameraContext* context = CameraContext::Instance();

    QVector<CameraParam> params;
    error = FetchParams(serial, &params);
    if (!error.isEmpty()) {
        return MakeError(QStringLiteral("读取相机 %1 的参数失败：%2").arg(serial, error));
    }

    CameraParam target;
    if (!FindParam(params, name, &target)) {
        return MakeError(QStringLiteral("相机 %1 上没有名为 %2 的参数。").arg(serial, name));
    }

    if (!target.isWriteable()) {
        return MakeError(QStringLiteral("参数 %1 当前不可写（只读项或设备未开放）。").arg(name));
    }

    // ★ 类型与范围校验都在这里。不合格就带着原因返回，模型下一轮会改。
    const QString applyError = ApplyValue(target, arguments.value(QStringLiteral("value")));
    if (!applyError.isEmpty()) {
        return MakeError(QStringLiteral("写入被拒绝：%1").arg(applyError));
    }

    const uint32_t code = context->writeParam(serial, target);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("写入参数 %1 失败：%2").arg(name, DescribeError(code)));
    }

    // ★ 写后回读。设备可能内部做了取整或钳位，真实生效值以回读为准。
    //   只回报「已写入」而不回报实际值，模型就会以为自己改成功了。
    CameraParam readback = target;
    const uint32_t readCode = context->readParam(serial, readback);

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("name"), name);

    if (readCode == ZYCLEAR_OK) {
        payload.insert(QStringLiteral("value"), readback.displayText());
        payload.insert(QStringLiteral("requested"),
            QString::fromUtf8(QJsonDocument(QJsonArray { arguments.value(QStringLiteral("value")) })
                                  .toJson(QJsonDocument::Compact))
                .trimmed());
        payload.insert(QStringLiteral("verified"), true);
        return MakeOk(QStringLiteral("参数 %1 已写入，回读值为 %2。").arg(name, readback.displayText()),
            payload);
    }

    payload.insert(QStringLiteral("verified"), false);
    payload.insert(QStringLiteral("note"),
        QStringLiteral("写入成功但回读失败，无法确认设备实际生效值。"));
    return MakeOk(QStringLiteral("参数 %1 已写入，但回读失败，无法确认实际生效值。").arg(name),
        payload);
}

agent4cpp::ToolResult ToolStartGrab(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    const uint32_t code = CameraContext::Instance()->startGrabbing(serial);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("相机 %1 开始拉流失败：%2").arg(serial, DescribeError(code)));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("grabbing"), true);
    return MakeOk(QStringLiteral("相机 %1 已开始拉流。").arg(serial), payload);
}

agent4cpp::ToolResult ToolStopGrab(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    const uint32_t code = CameraContext::Instance()->stopGrabbing(serial);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("相机 %1 停止拉流失败：%2").arg(serial, DescribeError(code)));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("grabbing"), false);
    return MakeOk(QStringLiteral("相机 %1 已停止拉流。").arg(serial), payload);
}

// ★ 本层最关键的工具。大模型看不到图像，只能基于数字推理，
//   所以这里把一帧画面量化成有语义的指标，并给出判定结论。
agent4cpp::ToolResult ToolFrameStats(const QJsonObject& arguments)
{
    QString error;
    const QString serial = ResolveSerial(arguments, &error);
    if (serial.isEmpty()) {
        return MakeError(error);
    }

    QImage image;
    const uint32_t code = CameraContext::Instance()->getImageLast(serial, image);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("取帧失败：%1。相机可能没有在拉流，"
                                        "先确认已连接并调 start_grab。")
                             .arg(DescribeError(code)));
    }
    if (image.isNull()) {
        return MakeError(QStringLiteral("取到的图像为空。"));
    }

    // 转成 8 位灰度再算指标。彩色图直接算均值会把颜色差异混进来
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

    // QImage 的每一行可能有对齐填充，所以要把 bytesPerLine 传给 cv::Mat，
    // 不能想当然按 width 去算步长
    const cv::Mat grayMat(gray.height(), gray.width(), CV_8UC1,
        const_cast<uchar*>(gray.constBits()), static_cast<size_t>(gray.bytesPerLine()));

    cv::Scalar meanScalar;
    cv::Scalar stddevScalar;
    cv::meanStdDev(grayMat, meanScalar, stddevScalar);

    const double brightness = meanScalar[0] / 255.0; // 归一化到 0~1，模型更容易理解
    const double contrast = stddevScalar[0];

    // 过曝 / 欠曝像素占比
    cv::Mat overMask;
    cv::Mat underMask;
    cv::compare(grayMat, cv::Scalar(250), overMask, cv::CMP_GT);
    cv::compare(grayMat, cv::Scalar(20), underMask, cv::CMP_LT);

    const double total = static_cast<double>(grayMat.total());
    const double overRatio = total > 0.0 ? cv::countNonZero(overMask) / total : 0.0;
    const double underRatio = total > 0.0 ? cv::countNonZero(underMask) / total : 0.0;

    // 清晰度：拉普拉斯算子的方差。方差越小说明边缘越少，画面越糊
    cv::Mat laplacian;
    cv::Laplacian(grayMat, laplacian, CV_64F);
    cv::Scalar lapMean;
    cv::Scalar lapStddev;
    cv::meanStdDev(laplacian, lapMean, lapStddev);
    const double sharpness = lapStddev[0] * lapStddev[0];

    // ★ 判定结论（「语义化观测值」）。没有这一句，模型得自己猜
    //   brightness=0.31 算不算暗——而它对不同设备、不同工艺的阈值一无所知。
    QString assessment;
    if (brightness < 0.25 || underRatio > 0.35) {
        assessment = QStringLiteral("underexposed");
    } else if (brightness > 0.75 || overRatio > 0.20) {
        assessment = QStringLiteral("overexposed");
    } else if (contrast < 12.0) {
        assessment = QStringLiteral("low_contrast");
    } else if (sharpness < 30.0) {
        assessment = QStringLiteral("blurry");
    } else {
        assessment = QStringLiteral("normal");
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("width"), gray.width());
    payload.insert(QStringLiteral("height"), gray.height());
    payload.insert(QStringLiteral("brightness"), brightness);
    payload.insert(QStringLiteral("brightness_scale"),
        QStringLiteral("0.0=全黑, 1.0=全白, 0.35~0.65 通常算曝光正常"));
    payload.insert(QStringLiteral("overexposed_ratio"), overRatio);
    payload.insert(QStringLiteral("underexposed_ratio"), underRatio);
    payload.insert(QStringLiteral("contrast"), contrast);
    payload.insert(QStringLiteral("contrast_scale"),
        QStringLiteral("灰度标准差，越小画面越平（发灰），低于 12 通常偏低"));
    payload.insert(QStringLiteral("sharpness"), sharpness);
    payload.insert(QStringLiteral("sharpness_scale"),
        QStringLiteral("拉普拉斯方差，越小越糊；对焦不准或运动模糊时明显下降"));
    payload.insert(QStringLiteral("assessment"), assessment);

    return MakeOk(QStringLiteral("画面判定为 %1：亮度 %2，过曝占比 %3%，欠曝占比 %4%，"
                                 "对比度 %5，清晰度 %6。")
                      .arg(assessment)
                      .arg(brightness, 0, 'f', 3)
                      .arg(overRatio * 100.0, 0, 'f', 1)
                      .arg(underRatio * 100.0, 0, 'f', 1)
                      .arg(contrast, 0, 'f', 1)
                      .arg(sharpness, 0, 'f', 1),
        payload);
}

} // namespace

QString CameraToolProvider::ProviderName() const
{
    return QStringLiteral("工业相机");
}

QString CameraToolProvider::ProviderDescription() const
{
    // 这段话会拼进系统提示词。写得具体一点，模型才知道
    // 「画面暗」「糊了」这类日常说法该走工具，而不是凭常识编。
    return QStringLiteral(
        "可以枚举与连接相机、读写相机参数、控制实时拉流，"
        "以及分析当前画面的亮度/过曝/对比度/清晰度。"
        "用户说画面偏暗、过曝、发灰、糊了、看不清，"
        "或者问某个参数现在是多少、能设成多少，都该用这里的工具。");
}

QStringList CameraToolProvider::ExamplePrompts() const
{
    return {
        QStringLiteral("画面有点暗，帮我调亮一点"),
        QStringLiteral("当前相机是什么状态？"),
        QStringLiteral("曝光时间现在是多少？"),
        QStringLiteral("画面是不是糊了？"),
    };
}

void CameraToolProvider::RegisterTools(agent4cpp::ToolRegistry& registry)
{
    // 说明文案是写给模型看的，直接决定它会不会用、什么时候用。
    // 所以每条都用「什么时候该调 + 有什么副作用」的写法。
    registry.Register(agent4cpp::ToolDefinition {
        "list_cameras",
        "列出当前相机。默认只回报界面上当前选中的相机，不做扫描。"
        "需要查找新接入的设备时传 refresh=true，注意扫描会断开所有已连接的相机。",
        { agent4cpp::BooleanParam("refresh", "true 表示重新扫描设备，会断开所有已连接相机。默认 false。", false) },
        UiTool(ToolListCameras) });

    registry.Register(agent4cpp::ToolDefinition {
        "get_current_camera",
        "查询界面上当前选中的相机，以及它是否已连接、是否正在拉流。"
        "不确定该操作哪台相机时先调它。",
        {},
        UiTool(ToolCurrentCamera) });

    registry.Register(agent4cpp::ToolDefinition {
        "connect_camera",
        "连接一台相机。不传 serial 时连接当前选中的相机。"
        "连接是修改参数和拉流的前提。",
        { agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolConnectCamera) });

    registry.Register(agent4cpp::ToolDefinition {
        "disconnect_camera",
        "断开一台相机。不传 serial 时断开当前选中的相机。",
        { agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolDisconnectCamera) });

    registry.Register(agent4cpp::ToolDefinition {
        "list_params",
        "列出相机参数。默认只返回可写参数（每项含类型、当前值、取值范围或可选值）。"
        "需要连只读项一起看时传 include_readonly=true。不确定参数名时先调它。",
        { agent4cpp::StringParam("group", "只列出某个分组，省略则列出全部分组。", false),
            agent4cpp::BooleanParam("include_readonly", "true 表示连只读参数一起返回，默认 false。", false),
            agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolListParams) });

    registry.Register(agent4cpp::ToolDefinition {
        "get_param",
        "读取单个参数的当前值、类型和取值范围。",
        { agent4cpp::StringParam("name", "参数名，例如 ExposureTime。"),
            agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。") },
        UiTool(ToolGetParam) });

    registry.Register(agent4cpp::ToolDefinition {
        "set_param",
        "修改一个相机参数并回读确认。值必须在该参数允许的范围内，"
        "枚举型参数必须从可选值里挑。写入失败会返回原因和合法范围。",
        { agent4cpp::StringParam("name", "参数名，例如 ExposureTime。"),
            agent4cpp::StringParam("value", "要写入的值。整数和浮点直接写数字（例如 8000），枚举写可选项字符串，布尔写 true/false。"),
            agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。") },
        UiTool(ToolSetParam) });

    registry.Register(agent4cpp::ToolDefinition {
        "start_grab",
        "开始拉流（实时采集）。取帧和画面分析前需要先拉流。",
        { agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolStartGrab) });

    registry.Register(agent4cpp::ToolDefinition {
        "stop_grab",
        "停止拉流。",
        { agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolStopGrab) });

    registry.Register(agent4cpp::ToolDefinition {
        "get_frame_stats",
        "分析当前画面并返回量化指标：亮度、过曝/欠曝像素占比、对比度、清晰度，"
        "以及综合判定结论 assessment（underexposed / overexposed / low_contrast / blurry / normal）。"
        "★ 用户说画面暗、画面糊、看不清之类的主观描述时，先调它拿到客观数据，"
        "再据此决定改哪个参数。相机需要处于拉流状态。",
        { agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolFrameStats) });
}

#endif // ZYCLEAR_HAS_AI
