// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../Public/ModelAdaptor.h"
#include "../Public/TreeItem.h"

#include <QString>

#include <assert.h>

ModelAdaptor *instance=nullptr;



ModelAdaptor::ModelAdaptor(unsigned int SceneId)
	:_SceneId(SceneId)
{
	if (instance)
		throw "ModelAdaptor only use once";
	instance = this;

}

void ModelAdaptor::setSearchText(QString searchText)
{
	std::string utf8FileName = searchText.toUtf8().toStdString();
	const char* text = utf8FileName.c_str();

}

void ModelAdaptor::setVisibility(TreeItem * item)
{
	

}


void ModelAdaptor::getItemUpdate(TreeItem *item)
{
	

}


