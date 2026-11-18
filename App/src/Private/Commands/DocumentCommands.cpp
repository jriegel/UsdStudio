// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/DocumentCommands.h"
#include "../../Public/Commands/CommandError.h"
#include "../../Public/Document.h"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/sdf/layer.h"

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
        throw CommandError::notFound("Could not open USD file: " + fileName_,
            "Check the path; UsdStage::Open needs an existing, readable .usd/.usda/.usdc file.");

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

// ------------------------------------------------------------------
// SaveDocumentCommand
// ------------------------------------------------------------------

SaveDocumentCommand::SaveDocumentCommand(Document& document, std::string fileName)
    : document_(document)
    , fileName_(std::move(fileName))
{
}

void SaveDocumentCommand::execute()
{
    // An empty name means "save to the file the document came from".
    const std::string target = fileName_.empty() ? document_.getFileName() : fileName_;
    if (target.empty())
        throw CommandError::precondition("Document has no file name to save to",
            "Pass a file name: document.save <path>");

    if (!document_.Save(target))
        throw CommandError::failed("Could not write USD file: " + target,
            "Check that the folder exists and is writable, and that the extension is one USD can export.");
}

void SaveDocumentCommand::undo()
{
    // Not undoable: the file on disk is an external effect.
}

std::string SaveDocumentCommand::description() const
{
    return "Save " + (fileName_.empty() ? document_.getFileName() : fileName_);
}
