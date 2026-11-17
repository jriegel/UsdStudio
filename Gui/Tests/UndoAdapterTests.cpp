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

class CountingCommand : public ICommand
{
public:
    void execute() override { ++executes; }
    void undo() override { ++undos; }
    std::string description() const override { return "Counting"; }

    int executes = 0;
    int undos = 0;
};

} // namespace

int main()
{
    QUndoStack stack;
    auto command = std::make_unique<CountingCommand>();
    CountingCommand* observed = command.get();

    // MainWindow executes before pushing; the adapter must swallow the
    // redo() that QUndoStack::push performs.
    observed->execute();
    stack.push(new UndoCommandAdapter(std::move(command)));

    check(observed->executes == 1, "push does not re-execute an executed command");
    check(observed->undos == 0, "push does not undo");

    stack.undo();
    check(observed->undos == 1 && observed->executes == 1, "stack undo calls command undo");

    stack.redo();
    check(observed->executes == 2 && observed->undos == 1, "stack redo re-executes the command");

    stack.clear();
    check(observed->executes == 2 && observed->undos == 1, "clear does not replay commands");

    std::cout << (failures == 0 ? "\nAll undo adapter tests passed.\n"
                                : "\n" + std::to_string(failures) + " test(s) FAILED.\n");
    return failures == 0 ? 0 : 1;
}
