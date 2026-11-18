// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <string>

// Base interface of every data-modifying operation in UsdStudio.
//
// Commands are deliberately Qt-free so they can run from the GUI (via the
// QUndoCommand adapter in /Gui), from scripts and from the headless CLI/AI
// frontend in App/src/Cli. Keep this header free of Qt includes.
//
// OpenUSD guidelines for implementations:
//   * Reference prims by pxr::SdfPath only; never store UsdPrim objects,
//     because a prim can become invalid when it is deleted and re-created
//     by an undo. Re-resolve with stage->GetPrimAtPath() in execute/undo.
//   * Batch multi-prim edits inside a pxr::SdfChangeBlock to avoid
//     per-attribute change notifications.
//   * Store deltas (old/new values), not full stage snapshots.
class ICommand
{
public:
    virtual ~ICommand() = default;

    // Applies the operation. Throw std::runtime_error (or derived) on
    // failure; the command must leave the document unmodified in that case.
    virtual void execute() = 0;

    // Reverts the operation. Only called after a successful execute().
    virtual void undo() = 0;

    // Human readable one-liner for undo views, logs and CLI echo.
    virtual std::string description() const = 0;

    // False for operations whose effect cannot be reverted (writing a file,
    // for example). Undo/redo stacks and the CLI schema use this to decide
    // whether the command belongs on an undo stack at all. Commands that
    // return false must also declare `static constexpr bool undoable = false;`
    // so the registry can document it without an instance.
    virtual bool isUndoable() const { return true; }

    // Hook to fold a following command into this one (e.g. many small
    // transform updates while dragging a gizmo). Default: never merge.
    virtual bool mergeWith(const ICommand& /*other*/) { return false; }
};
