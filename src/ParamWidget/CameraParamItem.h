#ifndef CAMERAPARAMITEM_H
#define CAMERAPARAMITEM_H

#include <QVariant>
#include <QVector>

class CameraParamItem {
public:
    explicit CameraParamItem(const QVariant& data, CameraParamItem* parent = nullptr);
    ~CameraParamItem();

    CameraParamItem* child(int number);
    int childCount() const;
    QVariant data() const;
    CameraParamItem* insertChildren(int position);
    CameraParamItem* parent();
    bool removeChildren(int position);
    int childNumber() const;
    bool setData(const QVariant& value);

private:
    QVector<CameraParamItem*> childItems;
    QVariant itemData;
    CameraParamItem* parentItem;
};

#endif

