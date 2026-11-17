


#pragma once

#include "pxr/usd/usd/stage.h"

#include <string>


class Document
{
	pxr::UsdStageRefPtr stage ;
	std::string fileName ;


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

	// Name of the file the stage was last opened from or saved to;
	// empty for a new, unsaved document.
	std::string getFileName() const { return fileName; }

	// Replaces stage and file name in one step. Used by the undoable
	// document commands to restore a previously active stage.
	void setStage(const pxr::UsdStageRefPtr& newStage, const std::string& newFileName);
};

