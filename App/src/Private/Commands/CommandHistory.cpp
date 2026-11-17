// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/CommandHistory.h"

void CommandHistory::executeAndPush(std::unique_ptr<ICommand> command)
{
    if (!command)
        return;

    command->execute(); // throws -> command is dropped, history untouched

    commands_.resize(cursor_); // drop redoable tail
    commands_.push_back(std::move(command));
    ++cursor_;
}

bool CommandHistory::undo()
{
    if (!canUndo())
        return false;
    commands_[cursor_ - 1]->undo();
    --cursor_;
    return true;
}

bool CommandHistory::redo()
{
    if (!canRedo())
        return false;
    commands_[cursor_]->execute();
    ++cursor_;
    return true;
}

std::vector<std::string> CommandHistory::descriptions() const
{
    std::vector<std::string> texts;
    texts.reserve(commands_.size());
    for (const auto& command : commands_)
        texts.push_back(command->description());
    return texts;
}

void CommandHistory::clear()
{
    commands_.clear();
    cursor_ = 0;
}
