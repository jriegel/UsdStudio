// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "ICommand.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// Qt-free undo/redo stack for headless execution (tests, scripts and a
// future CLI/AI frontend). The GUI uses a QUndoStack with the Qt-side
// UndoCommandAdapter instead; both drive the same ICommand objects.
class CommandHistory
{
public:
    // Runs the command and, on success, makes it the newest undoable entry
    // (discarding any redoable tail). A command whose execute() throws is
    // dropped and the history stays unchanged.
    void executeAndPush(std::unique_ptr<ICommand> command);

    bool undo();
    bool redo();

    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ < commands_.size(); }

    // Number of commands in the history (applied and redoable).
    std::size_t size() const { return commands_.size(); }

    // Split of size(): entries below the cursor are applied, the rest can be
    // redone. Used by the CLI's 'history.list' query.
    std::size_t undoDepth() const { return cursor_; }
    std::size_t redoDepth() const { return commands_.size() - cursor_; }

    // Text of all entries from oldest to newest (for logs and AI feedback).
    std::vector<std::string> descriptions() const;

    void clear();

private:
    std::vector<std::unique_ptr<ICommand>> commands_;
    std::size_t cursor_ = 0; // number of applied commands
};
