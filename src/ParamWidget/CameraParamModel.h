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
    enum ColType {
        NAME = 0,
        VALUE
    };

    enum ItemRoles {

        ParamRole = Qt::UserRole + 1,
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
    void SigValueChanged(const QModelIndex& index);

protected:
    CameraParamItem* getItem(const QModelIndex& index) const;

private:
    CameraParamItem* m_pRootItem;
    QStringList m_headers;
    QMap<QString, CameraParamItem*> m_groups;
};

#endif

