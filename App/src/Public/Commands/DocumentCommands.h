// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "ICommand.h"

#include "pxr/usd/usd/stage.h"

#include <string>

class Document;

// Opens a USD file and swaps it into the document. Undo restores the
// stage (and file name) that was active before, so no data is lost.
class OpenDocumentCommand : public ICommand
{
public:
    OpenDocumentCommand(Document& document, std::string fileName);

    void execute() override; // throws std::runtime_error if the file cannot be opened
    void undo() override;
    std::string description() const override;

private:
    Document& document_;
    std::string fileName_;

    // Saved for undo: a stage reference, never UsdPrims (see ICommand.h).
    pxr::UsdStageRefPtr previousStage_;
    std::string previousFileName_;
};

// Replaces the document with a fresh empty in-memory stage.
class NewDocumentCommand : public ICommand
{
public:
    explicit NewDocumentCommand(Document& document);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    Document& document_;

    pxr::UsdStageRefPtr previousStage_;
    std::string previousFileName_;
};
