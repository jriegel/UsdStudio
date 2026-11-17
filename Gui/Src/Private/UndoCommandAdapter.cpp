// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../Public/UndoCommandAdapter.h"

#include <Commands/ICommand.h>

UndoCommandAdapter::UndoCommandAdapter(std::unique_ptr<ICommand> command)
    : command_(std::move(command))
{
    setText(QString::fromStdString(command_->description()));
}

void UndoCommandAdapter::redo()
{
    if (alreadyExecuted_)
    {
        alreadyExecuted_ = false; // first call comes from QUndoStack::push
        return;
    }
    command_->execute();
}

void UndoCommandAdapter::undo()
{
    command_->undo();
}
