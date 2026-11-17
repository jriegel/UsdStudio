// (c)2024 Juergen Riegel, mail@juergen-riegel.net
//
// Headless smoke tests for the command framework and the document commands.
// Run via ctest (config Release/RelWithDebInfo) or the exe directly.

#include <Commands/CommandHistory.h>
#include <Commands/CommandRegistry.h>
#include <Commands/DocumentCommands.h>
#include <Document.h>

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/tf/token.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/prim.h"

#include <array>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef DATA_DIR
#define DATA_DIR "."
#endif

namespace
{

int failures = 0;

void check(bool condition, std::string_view what)
{
    std::cout << (condition ? "PASS  " : "FAIL  ") << what << std::endl;
    if (!condition)
        ++failures;
}

bool throws(const std::function<void()>& fn)
{
    try
    {
        fn();
    }
    catch (const std::exception&)
    {
        return true;
    }
    return false;
}

const pxr::SdfPath kKitchenRoot("/Kitchen_set");

} // namespace

int main()
{
    using namespace std::string_view_literals;

    Document document;
    CommandRegistry registry;
    registry.registerCommand<NewDocumentCommand, Document>(
        "document.new", "Replaces the document with a new empty stage.", &document);
    registry.registerCommand<OpenDocumentCommand, Document, std::string>(
        "document.open", "Opens a USD file: document.open <fileName>", &document);

    // Registry keys must survive registration exactly.
    check(registry.commandNames() == std::vector<std::string>{ "document.new", "document.open" },
          "registered command names are kept");
    check(registry.find("document.open") && registry.find("document.open")->argCount == 1,
          "registry reports argument count");

    // --- Registry and argument parsing ---------------------------------------------------------

    check(throws([&] { registry.create("does.not.exist", std::span<const std::string_view>{}); }),
          "unknown command is rejected");
    check(throws([&] { registry.create("document.open", std::span<const std::string_view>{}); }),
          "missing argument is rejected");
    check(throws([&] {
        std::array<std::string_view, 2> args{ "a"sv, "b"sv };
        registry.create("document.open", args);
    }), "too many arguments are rejected");
    check(throws([&] { (void)parseArg<double>("not-a-number"sv); }),
          "unparsable number is rejected");
    check(parseArg<pxr::SdfPath>("/World/Cube"sv).GetName() == pxr::TfToken("Cube"),
          "SdfPath argument parses");
    check(parseArg<pxr::GfVec3d>("1,-2.5,3"sv) == pxr::GfVec3d(1.0, -2.5, 3.0),
          "GfVec3d argument parses");

    const CommandLine line("document.new");
    check(line.name() == "document.new"sv && line.args().empty(),
          "command line tokenizes");

    // --- document.open: execute / undo / redo --------------------------------------------------

    const std::string kitchen = std::string(DATA_DIR) + "/Kitchen_set/Kitchen_set.usd";
    std::vector<std::string_view> openArgs{ std::string_view(kitchen) };

    CommandHistory history;
    check(!history.canUndo() && !history.canRedo(), "history starts empty");

    history.executeAndPush(registry.create("document.open", std::span(openArgs)));
    check(document.getStage() && document.getStage()->GetPrimAtPath(kKitchenRoot),
          "document.open loads Kitchen_set");
    check(document.getFileName() == kitchen, "document remembers the file name");

    check(history.undo(), "undo of open succeeds");
    check(document.getFileName().empty(), "undo restores the empty document name");
    check(document.getStage() && !document.getStage()->GetPrimAtPath(kKitchenRoot),
          "undo restores the previous stage");

    check(history.redo(), "redo of open succeeds");
    check(document.getStage() && document.getStage()->GetPrimAtPath(kKitchenRoot),
          "redo re-opens Kitchen_set");

    // --- document.new with undo back to the open stage -----------------------------------------

    history.executeAndPush(registry.create("document.new", std::span<const std::string_view>{}));
    check(document.getStage() && !document.getStage()->GetPrimAtPath(kKitchenRoot),
          "document.new clears the stage");
    check(history.undo() && document.getStage() && document.getStage()->GetPrimAtPath(kKitchenRoot),
          "undo of new restores the opened stage");

    // --- failing open must not touch history or document ---------------------------------------

    const std::string missing = std::string(DATA_DIR) + "/does/not/exist.usda";
    std::vector<std::string_view> badArgs{ std::string_view(missing) };
    check(throws([&] { history.executeAndPush(registry.create("document.open", std::span(badArgs))); }),
          "opening a missing file throws");
    check(history.size() == 2 && history.canUndo(),
          "failed command did not enter the history");
    check(document.getStage() && document.getStage()->GetPrimAtPath(kKitchenRoot),
          "failed open left the document unchanged");

    // --- history bookkeeping --------------------------------------------------------------------

    const auto texts = history.descriptions();
    check(texts.size() == 2 && texts[0].starts_with("Open "), "history lists entry descriptions");

    std::cout << (failures == 0 ? "\nAll command framework tests passed.\n"
                                : "\n" + std::to_string(failures) + " test(s) FAILED.\n");
    return failures == 0 ? 0 : 1;
}
