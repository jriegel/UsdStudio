#pragma once

#include <vector>
#include <atomic>
#include <QVariant>


class UberTreeItem
{
public:
    explicit UberTreeItem(unsigned int uberId, UberTreeItem *parent, int rowInParent);
    ~UberTreeItem();

	void deleteChildren();

	int columnCount() const { return 1; }

	QVariant data(int Column)const;

	std::string Name;

	unsigned int UberId;
	std::vector<UberTreeItem*> ChildItems;

	enum NodeVisibility {
		hidden = 0,
		visible = 1,
		partially_visible = 2,
		no_geometry = 3,
		invalid = 4
	}VisibilityState;

    UberTreeItem *ParentItem;
	unsigned int RowInParent=0;
	std::atomic<bool> InProgress;
	std::atomic<bool> Filled;
};
