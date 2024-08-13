// (c)2024 Juergen Riegel, mail@juergen-riegel.net


#include "../Public/TreeItem.h"
#include <QString>


TreeItem::TreeItem(unsigned int uberId, TreeItem *parent, int rowInParent)
	:ParentItem(parent), UberId(uberId), RowInParent(rowInParent)
{
	InProgress.store(false);
	Filled.store(false);
}


TreeItem::~TreeItem()
{
	deleteChildren();
}

void TreeItem::deleteChildren()
{
	for (auto i : ChildItems)
		delete (i);
	ChildItems.clear();
}



QVariant TreeItem::data(int Column)const
{
	return QVariant(QString::fromUtf8(Name.c_str()));
}
