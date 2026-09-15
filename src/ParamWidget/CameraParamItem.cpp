#include "CameraParamItem.h"

CameraParamItem::CameraParamItem(const QVariant& data, CameraParamItem* parent)
    : itemData(data)
    , parentItem(parent)
{
}

CameraParamItem::~CameraParamItem()
{
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

    delete childItems.takeAt(position);
    return true;
}

bool CameraParamItem::setData(const QVariant& value)
{
    itemData = value;
    return true;
}

