#include "CameraParamItem.h"

CameraParamItem::CameraParamItem(const QVariant& data, CameraParamItem* parent)
    : itemData(data)
    , parentItem(parent)
{
}

CameraParamItem::~CameraParamItem()
{
    // 递归删除：每个子节点析构时又会删掉自己的子树，因此删根即清空全树
    qDeleteAll(childItems);
}

CameraParamItem* CameraParamItem::child(int number)
{
    if (number < 0 || number >= childItems.size())
        return nullptr;
    return childItems.at(number);
}

int CameraParamItem::childCount() const
{
    return childItems.count();
}

int CameraParamItem::childNumber() const
{
    // 每次线性查找。宁可每次 O(n) 也不在节点里缓存行号：一旦增删节点，
    // 缓存的行号就要逐个同步，漏一个就会让整棵树的行号错位。
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<CameraParamItem*>(this));
    return 0;
}

QVariant CameraParamItem::data() const
{
    return itemData;
}

CameraParamItem* CameraParamItem::insertChildren(int position)
{
    // 边界只能自己挡：越界位置交给 QVector::insert 是未定义行为，
    // 而这里的 row 来自视图，视图在模型通知缺失时可能给出过期行号
    if (position < 0 || position > childItems.size())
        return nullptr;

    CameraParamItem* item = new CameraParamItem(QVariant(), this);
    childItems.insert(position, item);
    return item;
}

CameraParamItem* CameraParamItem::parent()
{
    return parentItem;
}

bool CameraParamItem::removeChildren(int position)
{
    if (position < 0 || position + 1 > childItems.size())
        return false;

    // 名字是复数，实际只摘掉这一个节点（当前没有调用方，属预留的增量删行接口）；
    // 摘出来必须立刻 delete：takeAt 之后它就脱离了 childItems，不会再被析构回收
    delete childItems.takeAt(position);
    return true;
}

bool CameraParamItem::setData(const QVariant& value)
{
    itemData = value;
    return true;
}

