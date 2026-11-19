// (c)2024 Juergen Riegel, mail@juergen-riegel.net
//
// Checks the Qt bridge semantics of UndoCommandAdapter: QUndoStack::push
// triggers a redo() that must not re-execute an already executed command,
// while later redo() calls must re-execute it. QUndoStack works without a
// QApplication, so this runs headless on any platform.

#include <UndoCommandAdapter.h>

#include <Commands/ICommand.h>
#include <QUndoStack>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace
{

int failures = 0;

void check(bool condition, std::string_view what)
{
    std::cout << (condition ? "PASS  " : "FAIL  ") << what << std::endl;
    if (!condition)
        ++failures;
}

struct Counts
{
    int executes = 0;
    int undos = 0;
};

class CountingCommand : public ICommand
{
public:
    explicit CountingCommand(Counts& counts) : counts_(counts) {}
    void execute() override { ++counts_.executes; }
    void undo() override { ++counts_.undos; }
    std::string description() const override { return "Counting"; }

private:
    Counts& counts_;
};

} // namespace

int main()
{
    QUndoStack stack;
    Counts counts;
    auto command = std::make_unique<CountingCommand>(counts);

    // MainWindow executes before pushing; the adapter must swallow the
    // redo() that QUndoStack::push performs.
    command->execute();
    stack.push(new UndoCommandAdapter(std::move(command)));

    check(counts.executes == 1, "push does not re-execute an executed command");
    check(counts.undos == 0, "push does not undo");

    stack.undo();
    check(counts.undos == 1 && counts.executes == 1, "stack undo calls command undo");

    stack.redo();
    check(counts.executes == 2 && counts.undos == 1, "stack redo re-executes the command");

    stack.clear();
    check(counts.executes == 2 && counts.undos == 1, "clear does not replay commands");

    std::cout << (failures == 0 ? "\nAll undo adapter tests passed.\n"
                                : "\n" + std::to_string(failures) + " test(s) FAILED.\n");
    return failures == 0 ? 0 : 1;
}
