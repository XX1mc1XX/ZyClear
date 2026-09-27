#include <QtTest>

#include "CameraInterface/ZCCameraParam.h"
#include "ParamWidget/CameraParamModel.h"

// 参数模型：分组节点组织、两列布局、只读权限在模型层被掐断、整包写回。
//
// 模型只管界面语义，取值仍来自 CameraParam 里已装箱的 QVariant，
// 因此这里不碰任何相机，参数对象都是就地造出来的。

namespace {

CameraParamMetaInfo makeMeta(const QString& group, const QString& name,
                             ZCParamType type, const QString& tips = QString())
{
    CameraParamMetaInfo meta;
    meta.group = group;
    meta.name = name;
    meta.type = type;
    meta.tips = tips;
    return meta;
}

// 造一个带值的整型参数。writeable 默认为真，用于覆盖可编辑与只读两条分支
CameraParam makeIntParam(const QString& group, const QString& name,
                         int64_t value, bool writeable = true)
{
    CameraParam param(makeMeta(group, name, INT));

    IntParam data;
    data.value = value;
    param.SetValue(QVariant::fromValue(data));

    param.setValid(true);
    param.setReadable(true);
    param.setWriteable(writeable);
    return param;
}

CameraParam makeEnumParam(const QString& group, const QString& name, const QString& text)
{
    CameraParam param(makeMeta(group, name, ENUM));

    EnumParam data;
    data.value = text;
    data.valueInt = 3;
    data.availableValue = { text };
    data.availableInt = { 3 };
    param.SetValue(QVariant::fromValue(data));

    param.setWriteable(true);
    return param;
}

} // namespace

class TestCameraParamModel : public QObject
{
    Q_OBJECT

private:
    CameraParamModel* model = nullptr;

    // 取「第一个分组的第一个参数行」——模型是两层的，参数行必须经分组索引定位
    QModelIndex firstParamIndex(int column = CameraParamModel::VALUE) const
    {
        const QModelIndex group = model->index(0, 0);
        if (!group.isValid()) {
            return QModelIndex();
        }
        return model->index(0, column, group);
    }

private slots:

    void init()
    {
        model = new CameraParamModel({ QStringLiteral("名称"), QStringLiteral("值") }, this);
    }

    void cleanup()
    {
        delete model;
        model = nullptr;
    }

    void emptyModelReportsNoRows()
    {
        QCOMPARE(model->rowCount(), 0);
    }

    // 任何层级都报两列：Qt 对叶节点也会问列数，返回 0 会让值列整个消失
    void columnCountIsTwoAtEveryLevel()
    {
        QCOMPARE(model->columnCount(), 2);

        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QCOMPARE(model->columnCount(), 2);
        QCOMPARE(model->columnCount(model->index(0, 0)), 2);
    }

    void headerDataComesFromConstructorArgument()
    {
        QCOMPARE(model->headerData(0, Qt::Horizontal).toString(), QStringLiteral("名称"));
        QCOMPARE(model->headerData(1, Qt::Horizontal).toString(), QStringLiteral("值"));

        // 纵向表头没有数据
        QVERIFY(!model->headerData(0, Qt::Vertical).isValid());
    }

    // 每个分组成为顶层一行
    void eachGroupBecomesATopLevelRow()
    {
        CameraParam first = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        CameraParam second = makeIntParam(QStringLiteral("采集"), QStringLiteral("帧率"), 30);

        model->addCameraParam(first);
        model->addCameraParam(second);

        QCOMPARE(model->rowCount(), 2);
    }

    // 同分组的参数共用同一个分组节点，不会各起一行
    void paramsSharingAGroupShareOneNode()
    {
        CameraParam first = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        CameraParam second = makeIntParam(QStringLiteral("图像"), QStringLiteral("高度"), 1080);

        model->addCameraParam(first);
        model->addCameraParam(second);

        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->rowCount(model->index(0, 0)), 2);
    }

    void nameColumnShowsParameterName()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QCOMPARE(model->index(0, CameraParamModel::NAME).data().toString(),
                 QStringLiteral("图像")); // 分组行显示的是分组名
        QCOMPARE(firstParamIndex(CameraParamModel::NAME).data().toString(),
                 QStringLiteral("宽度"));
    }

    void valueColumnShowsDisplayText()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QCOMPARE(firstParamIndex().data().toString(), QStringLiteral("1920"));
    }

    void valueColumnRendersEnumBySymbolName()
    {
        CameraParam param = makeEnumParam(QStringLiteral("图像"), QStringLiteral("像素格式"),
                                          QStringLiteral("Mono8"));
        model->addCameraParam(param);

        QCOMPARE(firstParamIndex().data().toString(), QStringLiteral("Mono8"));
    }

    // 分组行的 name 命中分组表，值列留空——否则它会带着 UNKNOWN 类型
    // 落到 displayText() 的 "unknow" 分支上直接显示出来
    void groupRowLeavesValueColumnEmpty()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QCOMPARE(model->index(0, CameraParamModel::VALUE).data().toString(), QString());
    }

    // ParamRole 交出整包参数，委托据此造控件、上层据此回写设备
    void paramRoleCarriesWholeParameter()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        const QModelIndex index = firstParamIndex();
        CameraParam carried = index.data(CameraParamModel::ParamRole).value<CameraParam>();

        QCOMPARE(carried.name(), QStringLiteral("宽度"));
        QCOMPARE(carried.group(), QStringLiteral("图像"));
        QCOMPARE(carried.type(), INT);
        QCOMPARE(carried.GetValue().value<IntParam>().value, static_cast<int64_t>(1920));
    }

    // 说明文字单列一个 role，省得为一句注解解包整个参数对象
    void descriptionRoleCarriesTips()
    {
        CameraParam param(makeMeta(QStringLiteral("图像"), QStringLiteral("宽度"), INT,
                                   QStringLiteral("视野宽度")));
        IntParam data;
        data.value = 1920;
        param.SetValue(QVariant::fromValue(data));
        model->addCameraParam(param);

        QCOMPARE(firstParamIndex().data(CameraParamModel::ParamDescriptionRole).toString(),
                 QStringLiteral("视野宽度"));
    }

    void writeableParamIsEditable()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920, true);
        model->addCameraParam(param);

        QVERIFY(model->flags(firstParamIndex()) & Qt::ItemIsEditable);
    }

    // 只读权限在模型层就被掐掉，视图因而不会进入编辑态、连控件都不会造
    void readOnlyParamIsNotEditable()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("序列号"), 1, false);
        model->addCameraParam(param);

        const Qt::ItemFlags flags = model->flags(firstParamIndex());
        QVERIFY(!(flags & Qt::ItemIsEditable));
        QVERIFY(flags & Qt::ItemIsEnabled);
    }

    // 分组节点的类型是 UNKNOWN、权限位全 0，同样落进不可编辑那一支
    void groupRowIsNotEditable()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QVERIFY(!(model->flags(model->index(0, CameraParamModel::VALUE)) & Qt::ItemIsEditable));
    }

    // 名字列是纯标签，任何情况下都不给编辑标志
    void nameColumnIsNeverEditable()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920, true);
        model->addCameraParam(param);

        QVERIFY(!(model->flags(firstParamIndex(CameraParamModel::NAME)) & Qt::ItemIsEditable));
    }

    void setDataEmitsValueChanged()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QSignalSpy spy(model, &CameraParamModel::SigValueChanged);
        QVERIFY(spy.isValid());

        CameraParam updated = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 2048);
        QVERIFY(model->setData(firstParamIndex(), QVariant::fromValue(updated),
                               CameraParamModel::ParamRole));

        QCOMPARE(spy.count(), 1);
    }

    // 只认 ParamRole：委托走的是整包写回，不会按列拆值
    void setDataWithUnexpectedRoleIsRejected()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        CameraParam updated = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 2048);
        QVERIFY(!model->setData(firstParamIndex(), QVariant::fromValue(updated), Qt::EditRole));
    }

    // 写回成功后视图取到的就是新值——界面是乐观更新的
    void setDataUpdatesWhatTheViewReads()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        CameraParam updated = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 2048);
        model->setData(firstParamIndex(), QVariant::fromValue(updated),
                       CameraParamModel::ParamRole);

        QCOMPARE(firstParamIndex().data().toString(), QStringLiteral("2048"));
    }

    void clearEmptiesTheModel()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);
        QCOMPARE(model->rowCount(), 1);

        model->clear();
        QCOMPARE(model->rowCount(), 0);

        // 清空后还能继续用：分组缓存在 clear 里被一并重置
        CameraParam next = makeIntParam(QStringLiteral("采集"), QStringLiteral("帧率"), 30);
        model->addCameraParam(next);
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->rowCount(model->index(0, 0)), 1);
    }

    void invalidIndexYieldsNoFlags()
    {
        QCOMPARE(model->flags(QModelIndex()), Qt::NoItemFlags);
    }

    void invalidIndexYieldsNoData()
    {
        QVERIFY(!model->data(QModelIndex(), Qt::DisplayRole).isValid());
    }

    // 越界行由 child() 返回空被挡在 createIndex 之前，不产生非法索引
    void outOfRangeIndexIsInvalid()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QVERIFY(!model->index(5, 0).isValid());
        QVERIFY(!model->index(-1, 0).isValid());
    }

    // 根节点不对外充当父：树只露出一层，分组是顶层、参数挂在分组下
    void topLevelParentIsInvalid()
    {
        CameraParam param = makeIntParam(QStringLiteral("图像"), QStringLiteral("宽度"), 1920);
        model->addCameraParam(param);

        QVERIFY(!model->parent(model->index(0, 0)).isValid());

        const QModelIndex paramIndex = firstParamIndex();
        QVERIFY(paramIndex.isValid());
        QCOMPARE(model->parent(paramIndex), model->index(0, 0));
    }

    void headerDataBeyondColumnCountIsOutOfRange()
    {
        // 两列表头，取第 2 列会越出 QStringList 范围——模型直接转发给 list，
        // 这里只确认合法范围内可用
        QCOMPARE(model->headerData(1, Qt::Horizontal).toString(), QStringLiteral("值"));
    }
};

// 模型本身只用 QAbstractItemModel，不需要窗口系统；用 GUILESS 免得
// 在无显示服务器的环境里还要准备平台插件
QTEST_GUILESS_MAIN(TestCameraParamModel)
#include "test_cameraparammodel.moc"
