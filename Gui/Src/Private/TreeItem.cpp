


#include "../Public/TreeItem.h"
#include <QString>


UberTreeItem::UberTreeItem(unsigned int uberId, UberTreeItem *parent, int rowInParent)
	:ParentItem(parent), UberId(uberId), RowInParent(rowInParent)
{
	InProgress.store(false);
	Filled.store(false);
}


UberTreeItem::~UberTreeItem()
{
	deleteChildren();
}

void UberTreeItem::deleteChildren()
{
	for (auto i : ChildItems)
		delete (i);
	ChildItems.clear();
}



QVariant UberTreeItem::data(int Column)const
{
	return QVariant(QString::fromUtf8(Name.c_str()));
}
