// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/DocumentCommands.h"
#include "../../Public/Document.h"

#include "pxr/usd/usd/stage.h"

#include <stdexcept>
#include <utility>

// ------------------------------------------------------------------
// OpenDocumentCommand
// ------------------------------------------------------------------

OpenDocumentCommand::OpenDocumentCommand(Document& document, std::string fileName)
    : document_(document)
    , fileName_(std::move(fileName))
{
}

void OpenDocumentCommand::execute()
{
    // Open first so a failure leaves the document untouched.
    pxr::UsdStageRefPtr newStage = pxr::UsdStage::Open(fileName_);
    if (!newStage)
        throw std::runtime_error("Could not open USD file: " + fileName_);

    previousStage_ = document_.getStage();
    previousFileName_ = document_.getFileName();

    document_.setStage(newStage, fileName_);
}

void OpenDocumentCommand::undo()
{
    document_.setStage(previousStage_, previousFileName_);
}

std::string OpenDocumentCommand::description() const
{
    return "Open " + fileName_;
}

// ------------------------------------------------------------------
// NewDocumentCommand
// ------------------------------------------------------------------

NewDocumentCommand::NewDocumentCommand(Document& document)
    : document_(document)
{
}

void NewDocumentCommand::execute()
{
    previousStage_ = document_.getStage();
    previousFileName_ = document_.getFileName();

    document_.New();
}

void NewDocumentCommand::undo()
{
    document_.setStage(previousStage_, previousFileName_);
}

std::string NewDocumentCommand::description() const
{
    return "New document";
}
