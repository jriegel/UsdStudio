// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <vector>
#include <atomic>
#include <QVariant>


class TreeItem
{
public:
    explicit TreeItem(unsigned int uberId, TreeItem *parent, int rowInParent);
    ~TreeItem();

	void deleteChildren();

	int columnCount() const { return 1; }

	QVariant data(int Column)const;

	std::string Name;

	unsigned int UberId;
	std::vector<TreeItem*> ChildItems;

	enum NodeVisibility {
		hidden = 0,
		visible = 1,
		partially_visible = 2,
		no_geometry = 3,
		invalid = 4
	}VisibilityState;

    TreeItem *ParentItem;
	unsigned int RowInParent=0;
	std::atomic<bool> InProgress;
	std::atomic<bool> Filled;
};
