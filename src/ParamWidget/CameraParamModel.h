#ifndef CAMERAPARAMMODEL_H
#define CAMERAPARAMMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QVector>

class CameraParamItem;
class CameraParam;
class CameraParamModel : public QAbstractItemModel {
    Q_OBJECT
public:
    // 列数恒定两列，与参数种类多少无关：新增参数类型只改变"值"列里挂什么编辑
    // 控件，不会给表加列，所以模型和视图对参数规模天生无感。
    enum ColType {
        NAME = 0,
        VALUE
    };

    enum ItemRoles {

        // 整包 CameraParam 的 QVariant：委托据此造控件，上层据此回写设备
        ParamRole = Qt::UserRole + 1,
        // 只带 tips 一句话，供下方说明框取用，省得为一句注解解包整个参数对象
        ParamDescriptionRole = Qt::UserRole + 2,
    };

    CameraParamModel(const QStringList& headers, QObject* parent = nullptr);
    ~CameraParamModel();

    void addCameraParam(CameraParam& param);
    void clear();

    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
        int role = Qt::EditRole) override;

signals:
    // 模型已接受新值、但设备还没收到时发出，上层据此触发真正的下发动作。
    // "改界面"与"写相机"被这道信号隔开，写失败的处置也就留在了上层。
    void SigValueChanged(const QModelIndex& index);

protected:
    CameraParamItem* getItem(const QModelIndex& index) const;

private:
    // 根节点由模型自己 new/delete，其余节点都挂在它下面跟着递归回收；
    // m_groups 只是"分组名 → 分组节点"的查找缓存，不拥有那些节点，
    // 所以 clear() 必须先清缓存再删根，顺序反了就会把悬空指针留在缓存里。
    CameraParamItem* m_pRootItem;
    QStringList m_headers;
    QMap<QString, CameraParamItem*> m_groups;
};

#endif

