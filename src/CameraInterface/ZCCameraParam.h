#ifndef CAMERAPARAM_H
#define CAMERAPARAM_H

#include <QString>
#include <QVariant>
#include <QVector>

// 相机参数的本体与元信息：所有厂商实现共用这套结构描述"一个可读写的参数"
// type 是 GetValue() 的解包开关，决定 QVariant 里按哪个结构体取值；
// 一旦 type 与实际存入的结构体不符，value<T>() 会静默返回默认构造值，
// 既不抛错也不告警 —— 厂商实现里 type 与 SetValue 的配对必须严格一致
enum ZCParamType {
    UNKNOWN = 0,
    INT,
    DOUBLE,
    ENUM,
    BOOL,
    CMD,
    STRING
};

// 参数分两层：meta 是静态描述（哪家的哪个参数），由厂商 getParamList 给出且固定；
// value 是运行期读回的真值，在 readParam 时回填
struct CameraParamMetaInfo {
    QString group;
    QString name;
    ZCParamType type { UNKNOWN };
    QString relative_list;
    QString tips;
};

// 多态基类：QVariant 存值本身已是深拷贝，clone 用于需要脱离源对象独立存活
// （按基类指针传递、晚于源失效）的场景，当前主要由测试使用
struct ZCParam {
    virtual ~ZCParam() = default;
    virtual ZCParam* clone() = 0;
};

struct IntParam : public ZCParam {
    int64_t value {};
    int64_t min {};
    int64_t max {};
    int64_t increment {};

    ZCParam* clone() override
    {
        return new IntParam(*this);
    }
};

struct DoubleParam : public ZCParam {
    double value {};
    double min {};
    double max {};

    ZCParam* clone() override
    {
        return new DoubleParam(*this);
    }
};

struct BoolParam : public ZCParam {
    bool value {};

    ZCParam* clone() override
    {
        return new BoolParam(*this);
    }
};

struct StringParam : public ZCParam {
    QString value {};
    unsigned int nMaxLength {};

    ZCParam* clone() override
    {
        return new StringParam(*this);
    }
};

// 枚举同时保留符号名与整数值两套表示：写卡用 valueInt（SDK 只认整数），
// UI 显示用 value；两者须由同一次 readParam 一起填出，不可只改其一
struct EnumParam : public ZCParam {
    QString value {};
    QVector<QString> availableValue;
    int valueInt {};
    QVector<int> availableInt;

    ZCParam* clone() override
    {
        return new EnumParam(*this);
    }
};

// 命令无值：它是一次性动作，写下去即触发，读回来不携带状态
struct CmdParam : public ZCParam {
    ZCParam* clone() override
    {
        return new CmdParam(*this);
    }
};

class CameraParam {
public:
    CameraParam()
    {
    }

    CameraParam(CameraParamMetaInfo meta)
        : _meta(meta)
    {
    }

    // 手写拷贝构造（与编译器默认生成等价）：留意 _accessMode 随对象整体复制，
    // 拷贝出的参数会带上源对象的读写权限标记
    CameraParam(const CameraParam& param)
    {
        _meta = param._meta;
        _value = param._value;
        _accessMode = param._accessMode;
    }

    QVariant GetValue() const
    {
        return _value;
    }

    // 值以 QVariant 承载，存进去的是结构体副本；取回后须按 type() 解包
    void SetValue(const QVariant& value)
    {
        _value = value;
    }

    // 只换元信息、不动已读回的值，用于同名参数换描述时复用对象
    void reset(CameraParamMetaInfo meta)
    {
        _meta = meta;
    }

    const QString& name() const
    {
        return _meta.name;
    }

    const QString& group() const
    {
        return _meta.group;
    }

    const QString& relativeList() const
    {
        return _meta.relative_list;
    }

    ZCParamType type() const
    {
        return _meta.type;
    }

    const QString& tips() const
    {
        return _meta.tips;
    }

    QString displayText() const
    {
        switch (type()) {
        case STRING: {
            StringParam varParam = GetValue().value<StringParam>();
            return varParam.value;
        }
        case CMD: {
            return "{Command}";
        }
        case INT: {
            IntParam varParam = GetValue().value<IntParam>();
            return QString::number(varParam.value);
        }
        case DOUBLE: {
            DoubleParam varParam = GetValue().value<DoubleParam>();
            return QString::number(varParam.value);
        }
        case BOOL: {
            BoolParam varParam = GetValue().value<BoolParam>();
            return varParam.value ? "True" : "False";
        }
        case ENUM: {
            EnumParam varParam = GetValue().value<EnumParam>();
            return varParam.value;
        }
        default:
            break;
        }
        // 拼写沿袭原文 unknow，别顺手改成 unknown
        return QString("unknow");
    }

    // 三个都是只读查询，标 const 才能在 const 上下文里用
    // （同文件的 displayText() 一直是 const，这三个是漏了）
    bool isValid() const
    {
        return _accessMode.valid;
    }
    bool isReadable() const
    {
        return _accessMode.readable;
    }
    bool isWriteable() const
    {
        return _accessMode.writeable;
    }

    void setValid(bool valid)
    {
        _accessMode.valid = valid;
    }
    void setReadable(bool readable)
    {
        _accessMode.readable = readable;
    }
    void setWriteable(bool writeable)
    {
        _accessMode.writeable = writeable;
    }

private:
    CameraParamMetaInfo _meta;
    QVariant _value;
    // 位域零初始化：默认 valid/readable/writeable 全为 false，
    // 未经厂商 getFeatureAccessMode 填过的参数会被 read/write 当作不可访问而静默跳过
    // —— 这正是各实现读值前先查 access mode 的原因
    struct
    {
        bool valid : 1;
        bool readable : 1;
        bool writeable : 1;
    } _accessMode {};
};

// 注册进 Qt 元类型系统，QVariant::fromValue/value<T> 才能编译通过；
// 少任一注册，对应参数就无法经 QVariant 在整条参数链路上传递
Q_DECLARE_METATYPE(IntParam);
Q_DECLARE_METATYPE(DoubleParam);
Q_DECLARE_METATYPE(BoolParam);
Q_DECLARE_METATYPE(StringParam);
Q_DECLARE_METATYPE(EnumParam);
Q_DECLARE_METATYPE(CmdParam);
Q_DECLARE_METATYPE(CameraParam);

#endif

