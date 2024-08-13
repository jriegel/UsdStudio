#pragma once

#include <vector>
#include <atomic>
#include <QObject>



class UberTreeItem;

class ModelAdaptor: public QObject
{
	Q_OBJECT

public:
    explicit ModelAdaptor(unsigned int SceneId);

	void getItemUpdate(UberTreeItem *item);

	void setSearchText(QString searchText);
	void setVisibility(UberTreeItem *item);

signals:
	void signalItemUpdate(UberTreeItem *item);

private:
	unsigned int _SceneId;

};
