#include "../Public/Document.h"

#include "pxr/usd/usdGeom/xform.h" 
#include "pxr/usd/usdGeom/sphere.h"
#include "pxr/usd/sdf/path.h"

#include "iostream"

Document::Document()
{
	stage = pxr::UsdStage::CreateInMemory();
}
