#include "CameraParamModel.h"
#include "CameraInterface/ZCCameraParam.h"
#include "CameraParamDelegate.h"
#include "CameraParamItem.h"
#include <QtWidgets>

CameraParamModel::CameraParamModel(const QStringList& headers, QObject* parent)
    : QAbstractItemModel(parent)
    , m_headers(headers)
{

    // 根节点不对应任何参数，用空 QVariant 占位，它只负责托住所有分组节点
    m_pRootItem = new CameraParamItem(QVariant());
}

CameraParamModel::~CameraParamModel()
{
    // 根节点递归回收整棵子树，除此之外模型不持有任何条目
    delete m_pRootItem;
}

int CameraParamModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    // 任何层级都报两列：Qt 对叶节点也会问列数，这里返回 0 会让值列整个消失
    return ColType::VALUE + 1;
}

QVariant CameraParamModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    CameraParamItem* item = getItem(index);
    QVariant varData = item->data();
    CameraParam paramData = varData.value<CameraParam>();

    switch (role) {
    case ItemRoles::ParamRole: {
        return varData;
        break;
    }
    case Qt::DisplayRole: {
        if (index.column() == ColType::NAME) {
            return paramData.name();
        } else if (index.column() == ColType::VALUE) {

            // 分组节点的 name 就是分组名，命中即说明这是分组行，值列留空；否则它
            // 没带类型，displayText() 会回落到 "unknow" 直接显示出来。
            // 这里 keys() 会拷一份完整列表，属于取值热路径，参数上千时值得改 contains()
            if (m_groups.keys().contains(paramData.name())) {
                return "";
            }
            return paramData.displayText();
        }
        break;
    }
    case ItemRoles::ParamDescriptionRole: {
        return paramData.tips();
        break;
    }
    default:
        break;
    }

    return QVariant();
}

Qt::ItemFlags CameraParamModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    // 名字列是纯标签，任何情况下都不给编辑标志
    if (index.column() == ColType::NAME) {
        return Qt::ItemIsEnabled | QAbstractItemModel::flags(index);
    }

    auto varValue = data(index, CameraParamModel::ParamRole);
    CameraParam cameraParam = varValue.value<CameraParam>();
    // 只读权限在模型层就被掐掉，而不是留给委托去拒绝：视图看到不可编辑的 index
    // 压根不会进入编辑态，连控件都不会造，比事后拦截少一轮创建/销毁。
    // 分组节点类型为 UNKNOWN、权限位全 0，同样落在这一支
    if (cameraParam.isWriteable()) {
        return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
    } else {
        return Qt::ItemIsEnabled | QAbstractItemModel::flags(index);
    }
}

CameraParamItem* CameraParamModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        CameraParamItem* item = static_cast<CameraParamItem*>(index.internalPointer());
        if (item)
            return item;
    }
    return m_pRootItem;
}

QVariant CameraParamModel::headerData(int section, Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_headers.at(section);
    return QVariant();
}

QModelIndex CameraParamModel::index(int row, int column, const QModelIndex& parent) const
{
    CameraParamItem* parentItem = getItem(parent);
    if (!parentItem)
        return QModelIndex();

    CameraParamItem* childItem = parentItem->child(row);
    // 靠 child() 的越界返回空把非法行挡在 createIndex 之前；internalPointer 存的是
    // 条目裸指针，行号不入索引，需要时由 childNumber() 反查——这也意味着模型
    // reset/clear 之后旧的 QModelIndex 一律失效，不可跨重建复用
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex CameraParamModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    CameraParamItem* childItem = getItem(index);
    CameraParamItem* parentItem = childItem ? childItem->parent() : nullptr;

    // 根节点不对外充当父：只有它的直接子节点（分组）成为顶层项，树便只露出一层
    if (parentItem == m_pRootItem || !parentItem)
        return QModelIndex();

    // 列固定传 0：Qt 只会为第 0 列索要父索引，父索引的列没有意义
    return createIndex(parentItem->childNumber(), 0, parentItem);
}

int CameraParamModel::rowCount(const QModelIndex& parent) const
{
    const CameraParamItem* parentItem = getItem(parent);

    return parentItem ? parentItem->childCount() : 0;
}

bool CameraParamModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    // 刻意只认 ParamRole、不处理 EditRole：委托走的是"整包写回"，不会按列拆值，
    // 多实现一套反而会被视图默认编辑流程重写一次。
    // 索引必须先判有效：getItem() 对无效索引会回落到根节点，不拦就会把值写到根上，
    // 还会发出一个指向无效索引的 dataChanged，视图那边随即错乱
    if (!index.isValid())
        return false;

    CameraParamItem* item = getItem(index);
    CameraParam param = item->data().value<CameraParam>();

    if (role == ItemRoles::ParamRole) {
        if (item->setData(value)) {
            // 先刷新视图再通知上层写设备：界面是乐观更新的，设备写失败
            // 不会回滚这里的值（回滚责任的缺失见 ParamWidget::writeCameraParam）
            emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
            emit SigValueChanged(index);
            return true;
        }
    }

    return false;
}

void CameraParamModel::addCameraParam(CameraParam& param)
{
    // 分组节点是懒建的：头一次碰到某分组才补一个，之后命中缓存复用。
    // 全程没有 beginInsertRows/endInsertRows，所以视图收不到行数变化——现有调用方
    // 靠随后的 reset()/expandAll() 整树重挂兜底，换到别处复用必须自己补模型通知。
    CameraParamItem* pCurGroupRootItem = m_pRootItem;
    QString strCurGroupName = param.group();

    auto item = m_groups.find(strCurGroupName);
    if (item != m_groups.end()) {

        pCurGroupRootItem = item.value();
    } else {

        auto newGroup = m_pRootItem->insertChildren(m_pRootItem->childCount());

        // 分组节点也往条目里塞一张 CameraParam：name 写成分组名、group 记 "root"，
        // 这样 data() 里"名字命中分组表 => 值列留空"的判断才成立；类型给 UNKNOWN，
        // 顺带让它落进 flags() 的不可编辑分支
        auto info = CameraParamMetaInfo { "root", strCurGroupName, UNKNOWN, "", "" };
        newGroup->setData(QVariant::fromValue(CameraParam(info)));

        m_groups[strCurGroupName] = newGroup;
        pCurGroupRootItem = newGroup;
    }

    auto pNewItem = pCurGroupRootItem->insertChildren(pCurGroupRootItem->childCount());
    pNewItem->setData(QVariant::fromValue(param));
}

void CameraParamModel::clear()
{
    // 先清缓存再删根：缓存里存的是子节点裸指针，delete 之后它们立刻悬空
    m_groups.clear();
    delete m_pRootItem;
    m_pRootItem = new CameraParamItem(QVariant());
    // removeRows(0, this->rowCount());
}

