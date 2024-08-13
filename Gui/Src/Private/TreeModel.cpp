// (c)2024 Juergen Riegel, mail@juergen-riegel.net


#include "../Public/TreeModel.h"
#include "../Public/TreeItem.h"
#include "../Public/ModelAdaptor.h"


TreeModel::TreeModel(ModelAdaptor *adaptor, QObject *parent)
    : QAbstractItemModel(parent), _adaptor(adaptor)
{
	// create root object wrapper, 1 is root object id in Uber
	rootItem = new TreeItem(1, nullptr,0);

	QObject::connect(_adaptor, &ModelAdaptor::signalItemUpdate, this, &TreeModel::slotItemUpdate, Qt::QueuedConnection);

	_adaptor->getItemUpdate(rootItem);
}



TreeModel::~TreeModel()
{
    delete rootItem;
	delete _adaptor;
}



int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}



QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

	TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

	if (role == Qt::CheckStateRole && index.column() == 0)
		switch (item->VisibilityState) {
		case TreeItem::hidden: return  Qt::Unchecked;
		case TreeItem::visible: return  Qt::Checked;
		case TreeItem::partially_visible: return  Qt::PartiallyChecked;
		default: return  Qt::Checked;
		}

    if (role != Qt::DisplayRole)
        return QVariant();


	if (item->InProgress.load())
		return QVariant("Loading...");


	for (TreeItem *actItem : item->ChildItems)
		if(!actItem->Filled.load() && !actItem->InProgress.load())
			_adaptor->getItemUpdate(actItem);

	return item->data(index.column());
}

bool TreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (role == Qt::CheckStateRole && index.column() == 0) {

		TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

		if ((Qt::CheckState)value.toInt() == Qt::Checked)
		{
			item->VisibilityState = TreeItem::visible;
			_adaptor->setVisibility(item);
			return true;
		}
		else
		{
			item->VisibilityState = TreeItem::hidden;
			_adaptor->setVisibility(item);
			return true;
		}

		return true;
	}

	return false;
}




Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return (Qt::ItemFlags) 0;

	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.column() == 0)
		flags |= Qt::ItemIsUserCheckable;

	return flags;
}



QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}



QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

	if (parentItem->InProgress.load())
		return QModelIndex();

	if (row < parentItem->ChildItems.size()) {
		TreeItem *childItem = parentItem->ChildItems[row];
		return createIndex(row, column, childItem);
	}
	else
        return QModelIndex();
}



QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->ParentItem;

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(childItem->RowInParent, 0, parentItem);
}


int TreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

	if (parentItem->InProgress.load())
		return 0;
	else
		return (int) parentItem->ChildItems.size();
}

void TreeModel::slotSceneGraphLoaded()
{
	rootItem->deleteChildren();
	_adaptor->getItemUpdate(rootItem);

}

void TreeModel::slotItemUpdate(TreeItem *item)
{
	if (item->InProgress.load())
		return;

	if (item->ParentItem) {
		QModelIndex index = createIndex(item->RowInParent, 0, item);
		emit dataChanged(index,index);
	}
	else {
		for (TreeItem *actItem : item->ChildItems )
			_adaptor->getItemUpdate(actItem);
		beginResetModel();
		endResetModel();
	}
}

void TreeModel::updateRoot()
{
	this->beginResetModel();
	rootItem->deleteChildren();
	_adaptor->getItemUpdate(rootItem);
	endResetModel();
}

QString TreeModel::getPathByIndex(const QModelIndex & index)
{
	QString result;
	TreeItem *Item = static_cast<TreeItem*>( index.internalPointer());
	result.insert(0, Item->Name.c_str());
	while (Item->ParentItem != 0) {
		Item = Item->ParentItem;
		if (Item->ParentItem != 0) {
			result.insert(0, "/");
			result.insert(0, Item->Name.c_str());
		}
	}
	return result;
}

int TreeModel::getRenderIdByIndex(const QModelIndex & index)
{
	return static_cast<TreeItem*>(index.internalPointer())->UberId;
}

