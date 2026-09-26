#ifndef CAMERAPARAM_H
#define CAMERAPARAM_H

#include <QString>
#include <QVariant>
#include <QVector>

enum ZCParamType {
    UNKNOWN = 0,
    INT,
    DOUBLE,
    ENUM,
    BOOL,
    CMD,
    STRING
};

struct CameraParamMetaInfo {
    QString group;
    QString name;
    ZCParamType type { UNKNOWN };
    QString relative_list;
    QString tips;
};

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

    void SetValue(const QVariant& value)
    {
        _value = value;
    }

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
    struct
    {
        bool valid : 1;
        bool readable : 1;
        bool writeable : 1;
    } _accessMode {};
};

Q_DECLARE_METATYPE(IntParam);
Q_DECLARE_METATYPE(DoubleParam);
Q_DECLARE_METATYPE(BoolParam);
Q_DECLARE_METATYPE(StringParam);
Q_DECLARE_METATYPE(EnumParam);
Q_DECLARE_METATYPE(CmdParam);
Q_DECLARE_METATYPE(CameraParam);

#endif

