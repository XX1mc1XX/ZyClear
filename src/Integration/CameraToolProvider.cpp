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

// 整条 AI 链路里只有这一层认识「相机」：它把 CameraContext 门面的能力翻译成模型
// 看得懂的工具，门面背后是海康 SDK 还是虚拟相机，这里不关心也不该关心 —— 正因为
// 如此，Extension/ 那套通用 AI 层可以整体搬到别的客户端，只换掉这个文件即可。
//
// 要交给模型的「说明书」（工具名、description、参数 Schema）全部由本文件提供，
// 模型只能靠这些文字决定该不该调、传什么值，所以这些文案本身就是程序逻辑的一部分，
// 改措辞和改代码一样要防回归。
namespace {

std::string ToJson(const QJsonObject& object)
{
    // 用 Compact 而不是 Indented：payload 每轮都要塞进模型上下文，缩进空白纯属浪费 token。
    return QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}

agent4cpp::ToolResult MakeOk(const QString& content, const QJsonObject& payload)
{
    // content 是给人看的一句话；payload 是给模型继续推理的结构化数据，
    // 两条通道别互相替代（工具体系对这两者的约定见 agent4cpp/tool.h）。
    agent4cpp::ToolResult result;
    result.status = agent4cpp::Status::Ok();
    result.content = content.toStdString();
    result.payload_json = ToJson(payload);
    return result;
}

// 失败也回灌给模型（不中断循环），文案要写清错误原因
// 用 FailedPrecondition 而不是 Internal：语义是「参数/条件不满足，改一下可重试」，
// 模型据此会自己纠错，而不是把它当成设备坏了。
agent4cpp::ToolResult MakeError(const QString& content, const QJsonObject& payload = {})
{
    agent4cpp::ToolResult result;
    result.status = agent4cpp::Status::FailedPrecondition(content.toStdString());
    result.content = content.toStdString();
    result.payload_json = ToJson(payload);
    return result;
}

// CameraInterface 非线程安全，相机操作统一切回主线程串行执行，免加锁
//
// 工具回调跑在 agent 的后台线程（IToolProvider::RegisterTools 的约定），
// 所以必须切回主线程；用 BlockingQueuedConnection 同步等结果，是因为调用方
// 要马上拿 ToolResult 返回，异步回调没法往回送。
// 已经在主线程、或根本没有 QCoreApplication 时直接执行 —— 从主线程投递给自己
// 的阻塞队列调用会立刻死锁。
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

// 把「收 QJsonObject、返 ToolResult」的本地函数适配成 agent4cpp 的 ToolInvoker。
// 模型给的是一串 JSON 文本，统一在这里解析：残缺 JSON 不抛异常，而是当成一次
// 可解释的失败回给模型让它重发。解析出的对象按值捕获进 lambda —— 真正执行发生在
// 切回主线程之后，那时原始 QByteArray 早就析构了，靠引用会悬空。
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

// serial 是可选项：省略就落到界面上当前选中的相机。两者都空时给的是面向用户
// 的引导语 —— 这句文案会被模型转述给用户，所以写成人话而非错误码。
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

// 描述里带上十六进制错误码：日志和界面上出现的就是这个码，方便对着 SDK 手册查。
QString DescribeError(uint32_t code)
{
    return QStringLiteral("%1（错误码 %2）")
        .arg(getErrorInfoEn(code))
        .arg(code, 0, 16);
}

// 把模型给的值原样回显进错误文案：它看到自己传了什么，才知道该怎么改。
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

// 把一个相机参数翻译成给模型看的 Schema：readable/writeable 告诉它这项能不能写，
// min/max 与 options 给出全部合法取值，模型据此自己纠错，不必靠反复试错。
// tips 有内容才带上，空字段不占 token。
// 只读项默认不返回，避免参数清单把上下文撑满
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

// 包一层 getParamList：把门面的 uint32 错误码统一翻成人话，调用点只需看一个字符串。
QString FetchParams(const QString& serial, QVector<CameraParam>* params)
{
    const uint32_t code = CameraContext::Instance()->getParamList(serial, *params);
    if (code != ZYCLEAR_OK) {
        return DescribeError(code);
    }
    return QString();
}

// 先精确匹配、再退到大小写不敏感：模型偶尔把 ExposureTime 写成 exposuretime。
// 分两轮是为了在「仅大小写不同」的参数同时存在时，精确那一轮先命中，不被误配。
bool FindParam(const QVector<CameraParam>& params, const QString& name, CameraParam* out)
{
    for (const CameraParam& param : params) {
        if (param.name() == name) {
            *out = param;
            return true;
        }
    }
    for (const CameraParam& param : params) {
        if (param.name().compare(name, Qt::CaseInsensitive) == 0) {
            *out = param;
            return true;
        }
    }
    return false;
}

// 值来自模型推理，可能超范围或类型不对，在这里拦住并说明原因
// 类型收得宽（数字、数字字符串都收）：模型受 JSON 训练影响常把 8000 写成 "8000"，
// 严格拒绝只是平白多耗一轮。
// 但超范围直接拒绝、绝不静默钳位：min/max 是设备的硬约束，钳位会让模型以为
// 写进去的就是它要的值，后续推理全建立在错前提上。
QString ApplyValue(CameraParam& param, const QJsonValue& value)
{
    switch (param.type()) {
    case INT: {
        IntParam target = param.GetValue().value<IntParam>();

        // 模型常把数值写成字符串，数字和字符串两种形式都收
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
            const int number = static_cast<int>(value.toDouble());
            if (number >= 0 && number < target.availableValue.size()) {
                index = number;
            }
        }

        if (index < 0) {
            QStringList options;
            for (const QString& item : target.availableValue) {
                options << item;
            }
            return QStringLiteral("参数 %1 的可选值有：[%2]，收到的 %3 不在其中")
                .arg(param.name(), options.join(QStringLiteral(", ")), DescribeJsonValue(value));
        }

        target.value = target.availableValue.at(index);
        // 枚举同时存字符串名与对应整数值：下发给 SDK 要用整数，回显给模型要用字符串，
        // 两个都得跟上。
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

// payload 里的 hint / note 是把「下一步能做什么、这次动作有什么副作用」直接告诉模型，
// 工具之间只能靠这类文案互相指路。
agent4cpp::ToolResult ToolListCameras(const QJsonObject& arguments)
{
    const bool refresh = arguments.value(QStringLiteral("refresh")).toBool(false);

    if (!refresh) {
        // 扫描会断开所有已连接相机，不能因模型随口一问就执行
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

// 无参数工具：模型不确定「该操作哪台相机」时的入口，答案永远取自界面当前选中项。
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

// 先查已连接状态再连：对已连接的相机再连会报错，而模型很容易重复调同一个工具，
// 做成幂等后重复调用不算失败。
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

// connect 的收尾动作；serial 同样可省，落回当前选中的相机。
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

// 默认只列可写参数：只读项数量多、模型也改不了，全列出来只会挤占上下文。
// total / writeable / returned 三个计数让模型自己判断结果有没有被过滤或截断。
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
            continue;
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

// 找不到参数时把模型引向 list_params，而不是只回一句失败 —— 这样模型才有自己纠正的余地。
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

// 写值前重新 getParamList 拿完整 CameraParam：writeParam 要的是带类型与范围的整个
// 参数对象，不是孤零零一个值。先在本地用 ApplyValue 校验再下发，能挡掉大部分坏输入。
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

    const QString applyError = ApplyValue(target, arguments.value(QStringLiteral("value")));
    if (!applyError.isEmpty()) {
        return MakeError(QStringLiteral("写入被拒绝：%1").arg(applyError));
    }

    const uint32_t code = context->writeParam(serial, target);
    if (code != ZYCLEAR_OK) {
        return MakeError(QStringLiteral("写入参数 %1 失败：%2").arg(name, DescribeError(code)));
    }

    // 设备可能内部取整或钳位，回读才能拿到实际生效值
    CameraParam readback = target;
    const uint32_t readCode = context->readParam(serial, readback);

    QJsonObject payload;
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("name"), name);

    if (readCode == ZYCLEAR_OK) {
        // value 是设备实际生效值，requested 是模型原本请求的值；两个并列给出来，
        // 模型才看得出设备是怎么取整或截断的。
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

// 拉流是取帧与画面分析的前提，单独暴露成一个工具，模型才能显式表达
// 「先开流、再看画面」这个顺序。
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

// 停止拉流单独成工具：收工得由模型显式表达，不能随着一轮对话结束就隐式把流停了。
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

// 不回图像本身、只回量化指标：文本数字省 token 且结果确定，模型据此判断画面质量，
// 再决定去改哪个参数。
// payload 里的 _scale 字段顺便把「数值多大对应什么观感」讲给模型，省得它自己猜。
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

    // 亮度/对比度/清晰度都定义在灰度上，先转灰度再算，省掉逐通道处理。
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

    // QImage 每行有对齐填充，步长必须用 bytesPerLine，不能按 width 算
    const cv::Mat grayMat(gray.height(), gray.width(), CV_8UC1,
        const_cast<uchar*>(gray.constBits()), static_cast<size_t>(gray.bytesPerLine()));

    cv::Scalar meanScalar;
    cv::Scalar stddevScalar;
    cv::meanStdDev(grayMat, meanScalar, stddevScalar);

    const double brightness = meanScalar[0] / 255.0;
    const double contrast = stddevScalar[0];

    cv::Mat overMask;
    cv::Mat underMask;
    cv::compare(grayMat, cv::Scalar(250), overMask, cv::CMP_GT);
    cv::compare(grayMat, cv::Scalar(20), underMask, cv::CMP_LT);

    const double total = static_cast<double>(grayMat.total());
    const double overRatio = total > 0.0 ? cv::countNonZero(overMask) / total : 0.0;
    const double underRatio = total > 0.0 ? cv::countNonZero(underMask) / total : 0.0;

    cv::Mat laplacian;
    cv::Laplacian(grayMat, laplacian, CV_64F);
    cv::Scalar lapMean;
    cv::Scalar lapStddev;
    cv::meanStdDev(laplacian, lapMean, lapStddev);
    // 拉普拉斯响应的方差是经典的对焦评价函数，平方即方差；画面糊时高频细节变少，
    // 整体响应随之变小。
    const double sharpness = lapStddev[0] * lapStddev[0];

    // 判定阈值（亮度 0.25 / 0.75、对比度 12、清晰度 30 等）是现场经验值，统一写死在这里；
    // 只把结论标签交给模型，不让它自己套阈值，避免同一条链路各说各话。
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

// 这段直接拼进系统提示词，本身就是模型的路由规则：讲清「用户这么说 → 该调这里的工具」。
// 少了这类触发词，模型容易拿常识硬答，而不会去调工具。
QString CameraToolProvider::ProviderDescription() const
{
    return QStringLiteral(
        "可以枚举与连接相机、读写相机参数、控制实时拉流，"
        "以及分析当前画面的亮度/过曝/对比度/清晰度。"
        "用户说画面偏暗、过曝、发灰、糊了、看不清，"
        "或者问某个参数现在是多少、能设成多少，都该用这里的工具。");
}

// 这些句子显示在 AI 面板的欢迎语里，点一下就当作提问原样发出去，
// 所以要用用户的口吻写，而不是写成给模型的指令。
QStringList CameraToolProvider::ExamplePrompts() const
{
    return {
        QStringLiteral("画面有点暗，帮我调亮一点"),
        QStringLiteral("当前相机是什么状态？"),
        QStringLiteral("曝光时间现在是多少？"),
        QStringLiteral("画面是不是糊了？"),
    };
}

// 十个工具的说明书：description 决定模型在什么场景选它，parameters 是随请求一起发出去的
// JSON Schema，UiTool(...) 则是真正落到门面上的本地实现。
// 工具名统一 snake_case：模型对这套命名最熟。注册顺序只影响这份源码的可读性 ——
// registry 内部是有序 map，跟注册先后无关。
// 参数 required 默认是 true，凡 serial、refresh、include_readonly 这类可省略的项都显式
// 传 false，模型才不会因为漏填而被参数校验挡下。
void CameraToolProvider::RegisterTools(agent4cpp::ToolRegistry& registry)
{
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
        "用户说画面暗、画面糊、看不清之类的主观描述时，先调它拿到客观数据，"
        "再据此决定改哪个参数。相机需要处于拉流状态。",
        { agent4cpp::StringParam("serial", "相机序列号，省略则用当前选中的相机。", false) },
        UiTool(ToolFrameStats) });
}

#endif // ZYCLEAR_HAS_AI
