#pragma once


#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

class UberTreeItem;
class ModelAdaptor;


class TreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
	explicit TreeModel(ModelAdaptor *adaptor, QObject *parent = 0);
    ~TreeModel();

    QVariant data(const QModelIndex &index, int role) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role);
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;


	void updateRoot();
	QString getPathByIndex(const QModelIndex &index);
	int getRenderIdByIndex(const QModelIndex &index);

public slots:
	void slotSceneGraphLoaded();
	void slotItemUpdate(UberTreeItem *item);

private:

    UberTreeItem *rootItem;
	unsigned int _SceneId;

	ModelAdaptor *_adaptor;
};


