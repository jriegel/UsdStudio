// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <QUndoCommand>

#include <memory>

class ICommand;

// Thin bridge between the Qt-free command framework in /App and QUndoStack.
// Keeping this the only place where commands meet Qt lets the same commands
// run headless (tests, future CLI/AI frontends).
//
// The wrapped command is expected to have been executed once already
// (MainWindow::runCommand executes before pushing so a failing execute()
// never leaves a broken entry in the undo stack). The first redo() call made
// by QUndoStack::push is therefore skipped; later calls re-execute.
class UndoCommandAdapter : public QUndoCommand
{
public:
    explicit UndoCommandAdapter(std::unique_ptr<ICommand> command);

    // Called by QUndoStack on push and on redo.
    void redo() override;
    void undo() override;

private:
    std::unique_ptr<ICommand> command_;
    bool alreadyExecuted_ = true;
};
