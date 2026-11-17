// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#pragma once

#include "pxr/usd/usd/stage.h"

#include <QString>


class Document
{
	pxr::UsdStageRefPtr stage;

public:
	Document();

	// Replaces the current stage with a new empty in memory stage.
	void New();

	// Opens the USD file at fileName. Returns false if the file could not be opened.
	bool Open(const QString& fileName);

	pxr::UsdStageRefPtr getStage() const { return stage; }
};
