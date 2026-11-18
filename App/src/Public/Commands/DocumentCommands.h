// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "ICommand.h"

#include "pxr/usd/usd/stage.h"

#include <array>
#include <string>
#include <string_view>

class Document;

// Opens a USD file and swaps it into the document. Undo restores the
// stage (and file name) that was active before, so no data is lost.
class OpenDocumentCommand : public ICommand
{
public:
    OpenDocumentCommand(Document& document, std::string fileName);

    // Throws CommandError (file not found / unreadable) so a caller can act
    // on the failure instead of parsing the message.
    void execute() override;
    void undo() override;
    std::string description() const override;

    static constexpr std::array<std::string_view, 1> argNames() { return { "fileName" }; }

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

// Writes the document's root layer to disk. Deliberately not undoable -
// writing a file is an external effect the undo stack cannot revert.
// Such commands must also declare 'undoable = false' so the registry can
// report them as such (see CommandArgSpec.h).
class SaveDocumentCommand : public ICommand
{
public:
    SaveDocumentCommand(Document& document, std::string fileName);

    // Throws CommandError when the layer cannot be written.
    void execute() override;
    void undo() override; // no-op: nothing was recorded
    std::string description() const override;

    static constexpr bool undoable = false;
    bool isUndoable() const override { return false; }

    static constexpr std::array<std::string_view, 1> argNames() { return { "fileName" }; }

private:
    Document& document_;
    std::string fileName_;
};
