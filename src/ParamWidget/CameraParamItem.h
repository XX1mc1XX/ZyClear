#ifndef CAMERAPARAMITEM_H
#define CAMERAPARAMITEM_H

#include <QVariant>
#include <QVector>

// 树的通用节点：不透明地存一个 QVariant，所以同一个类既能当"分组"节点也能当
// "参数"节点，类型含义留给模型层解释。所有权父持有子，析构任一节点会连带删掉整棵子树。
class CameraParamItem {
public:
    explicit CameraParamItem(const QVariant& data, CameraParamItem* parent = nullptr);
    ~CameraParamItem();

    CameraParamItem* child(int number);
    int childCount() const;
    QVariant data() const;
    // 只挂一个空节点，值要调用方紧接着 setData 填；位置越界返回空指针而不抛异常
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

