


#pragma once

#include "pxr/usd/usd/stage.h"

#include <string>


class Document
{
	pxr::UsdStageRefPtr stage ;


public:
	Document();

	// Replaces the current stage with a new empty in memory stage.
	void New();

	// Opens the USD file 'fileName' and replaces the current stage with it.
	// Returns false if the file could not be opened.
	bool Open(const std::string& fileName);

	// Saves the current stage to 'fileName'. Returns false on failure.
	bool Save(const std::string& fileName);

	pxr::UsdStageRefPtr getStage() const { return stage; }
};

