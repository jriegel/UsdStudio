#include "../Public/Document.h"

#include "pxr/usd/usdGeom/xform.h" 
#include "pxr/usd/usdGeom/sphere.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/layer.h"

#include "iostream"

Document::Document()
{
	stage = pxr::UsdStage::CreateInMemory();
}

void Document::New()
{
	stage = pxr::UsdStage::CreateInMemory();
	fileName.clear();
}

bool Document::Open(const std::string& fileName)
{
	pxr::UsdStageRefPtr newStage = pxr::UsdStage::Open(fileName);
	if (!newStage)
	{
		std::cerr << "Could not open USD file: " << fileName << std::endl;
		return false;
	}

	stage = newStage;
	this->fileName = fileName;
	return true;
}

void Document::setStage(const pxr::UsdStageRefPtr& newStage, const std::string& newFileName)
{
	stage = newStage;
	fileName = newFileName;
}

bool Document::Save(const std::string& fileName)
{
	if (!stage)
		return false;

	pxr::SdfLayerHandle rootLayer = stage->GetRootLayer();
	if (!rootLayer)
		return false;

	if (!rootLayer->Export(fileName))
		return false;

	this->fileName = fileName;
	return true;
}
