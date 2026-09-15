#include "CameraParamModel.h"
#include "CameraInterface/ZCCameraParam.h"
#include "CameraParamDelegate.h"
#include "CameraParamItem.h"
#include <QtWidgets>

CameraParamModel::CameraParamModel(const QStringList& headers, QObject* parent)
    : QAbstractItemModel(parent)
    , m_headers(headers)
{

    m_pRootItem = new CameraParamItem(QVariant());
}

CameraParamModel::~CameraParamModel()
{
    delete m_pRootItem;
}

int CameraParamModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
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

    if (index.column() == ColType::NAME) {
        return Qt::ItemIsEnabled | QAbstractItemModel::flags(index);
    }

    auto varValue = data(index, CameraParamModel::ParamRole);
    CameraParam cameraParam = varValue.value<CameraParam>();
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

    if (parentItem == m_pRootItem || !parentItem)
        return QModelIndex();

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

int CameraParamModel::rowCount(const QModelIndex& parent) const
{
    const CameraParamItem* parentItem = getItem(parent);

    return parentItem ? parentItem->childCount() : 0;
}

bool CameraParamModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    CameraParamItem* item = getItem(index);
    CameraParam param = item->data().value<CameraParam>();

    if (role == ItemRoles::ParamRole) {
        if (item->setData(value)) {
            emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
            emit SigValueChanged(index);
            return true;
        }
    }

    return false;
}

void CameraParamModel::addCameraParam(CameraParam& param)
{
    CameraParamItem* pCurGroupRootItem = m_pRootItem;
    QString strCurGroupName = param.group();

    auto item = m_groups.find(strCurGroupName);
    if (item != m_groups.end()) {

        pCurGroupRootItem = item.value();
    } else {

        auto newGroup = m_pRootItem->insertChildren(m_pRootItem->childCount());

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
    m_groups.clear();
    delete m_pRootItem;
    m_pRootItem = new CameraParamItem(QVariant());
    // removeRows(0, this->rowCount());
}

