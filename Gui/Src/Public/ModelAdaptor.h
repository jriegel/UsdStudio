// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <vector>
#include <atomic>
#include <QObject>



class TreeItem;

class ModelAdaptor: public QObject
{
	Q_OBJECT

public:
    explicit ModelAdaptor(unsigned int SceneId);

	void getItemUpdate(TreeItem *item);

	void setSearchText(QString searchText);
	void setVisibility(TreeItem *item);

Q_SIGNALS:
	void signalItemUpdate(TreeItem *item);

private:
	unsigned int _SceneId;

};
