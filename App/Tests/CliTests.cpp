// (c)2024 Juergen Riegel, mail@juergen-riegel.net
//
// Headless tests for the AI-oriented CLI. Everything runs in-process through
// UsdStudio::Cli::run(), so the contract (JSON on stdout, logs on stderr,
// distinct exit codes) is asserted without spawning a process.

#include <Cli/Cli.h>

#include <filesystem>
#include <iostream>
#include <sstream>
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

struct Result
{
    int code = 0;
    std::string out;
    std::string err;
};

Result runCli(std::vector<std::string> args)
{
    std::ostringstream out;
    std::ostringstream err;
    const int code = UsdStudio::Cli::run(args, out, err);
    return { code, out.str(), err.str() };
}

bool contains(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

std::vector<std::string> lines(std::string_view text)
{
    std::vector<std::string> result;
    std::istringstream stream{ std::string(text) };
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty())
            result.push_back(line);
    }
    return result;
}

std::string kitchenFile()
{
    return std::string(DATA_DIR) + "/Kitchen_set/Kitchen_set.usd";
}

// Every CLI invocation is stateless, so a test that needs a stage prepends an
// open operation: the same batch mode an agent is expected to use.
Result kitchen(std::vector<std::string> operations)
{
    std::vector<std::string> args{ "--command", "document.open " + kitchenFile() };
    for (const std::string& operation : operations)
    {
        args.push_back("--command");
        args.push_back(operation);
    }
    return runCli(args);
}

const std::string kStove = "/Kitchen_set/Props_grp/North_grp/StoveArea_grp/Stove_1";

} // namespace

int main()
{
    using UsdStudio::Cli::ExitFailed;
    using UsdStudio::Cli::ExitInternal;
    using UsdStudio::Cli::ExitInvalidValue;
    using UsdStudio::Cli::ExitNotFound;
    using UsdStudio::Cli::ExitOk;
    using UsdStudio::Cli::ExitUsage;

    // ------------------------------------------------------------ self-doc ---

    {
        const Result r = runCli({ "--version" });
        check(r.code == ExitOk, "--version exits 0");
        check(contains(r.out, "\"ok\":true"), "--version prints an ok envelope");
        check(contains(r.out, "\"openUsd\":\""), "--version reports the OpenUSD version");
    }

    {
        const Result r = runCli({ "--schema" });
        check(r.code == ExitOk, "--schema exits 0");
        check(contains(r.out, "\"commands\""), "--schema lists commands");
        check(contains(r.out, "\"queries\""), "--schema lists queries");
        check(contains(r.out, "\"actions\""), "--schema lists the history actions");
        check(contains(r.out, "\"exitCodes\""), "--schema documents the exit codes");
        check(contains(r.out, "\"flags\""), "--schema documents the flags");
        check(contains(r.out, "\"valueTypes\""), "--schema documents the value types");
        check(contains(r.out, "\"name\":\"primPath\""), "--schema names the attr.set arguments");
        check(contains(r.out, "stage.children <path>"), "--schema carries a usage string");
        // Even with --text the schema stays machine readable.
        const Result t = runCli({ "--text", "--schema" });
        check(contains(t.out, "\"commands\""), "--schema stays JSON in text mode");
    }

    {
        const Result r = runCli({ "--agent-help" });
        check(r.code == ExitOk, "--agent-help exits 0");
        check(contains(r.out, "RULES"), "--agent-help states the usage rules");
        check(contains(r.out, "stage.children"), "--agent-help lists the operations");
        check(contains(r.out, "--command"), "--agent-help explains batch mode");
    }

    {
        const Result r = runCli({ "--types" });
        check(r.code == ExitOk, "--types exits 0");
        check(contains(r.out, "float3"), "--types lists float3");
        check(contains(r.out, "asset"), "--types lists asset");
    }

    {
        const Result r = runCli({ "--help" });
        check(r.code == ExitOk, "--help exits 0");
        check(contains(r.out, "--limit"), "--help documents --limit");
        check(contains(r.out, "--dry-run"), "--help documents --dry-run");
        const Result topic = runCli({ "--help", "stage.children" });
        check(topic.code == ExitOk && contains(topic.out, "stage.children"), "--help <topic> works");
        const Result badTopic = runCli({ "--help", "nope.nope" });
        check(badTopic.code == ExitUsage, "--help <unknown> is a usage error");
    }

    // ------------------------------------------------------ usage & errors ---

    {
        const Result r = runCli({});
        check(r.code == ExitUsage, "no arguments exits 2");
        check(contains(r.out, "No operation given."), "no arguments explains the problem");
    }

    {
        const Result r = runCli({ "--bogus", "stage.info" });
        check(r.code == ExitUsage, "unknown flag exits 2");
        check(contains(r.out, "Unknown flag '--bogus'"), "unknown flag is named");
    }

    {
        const Result r = runCli({ "stage.fine" });
        check(r.code == ExitUsage, "unknown operation exits 2");
        check(contains(r.out, "\"code\":\"usage\""), "unknown operation reports the code");
        check(contains(r.out, "Did you mean: stage.find?"), "a typo gets a suggestion");
    }

    {
        const Result r = runCli({ "document.open", "does-not-exist.usd" });
        check(r.code == ExitNotFound, "opening a missing file exits 4");
        check(contains(r.out, "\"code\":\"notFound\""), "a missing file reports notFound");
        // The USD loader complaint goes to the process' stderr; stdout must
        // stay pure JSON.
        check(!contains(r.out, "Failed to open layer"), "the loader log never lands on stdout");
    }

    {
        // A flag after the operation line still belongs to the CLI.
        const Result r = kitchen({ "stage.children /Nope --limit 1" });
        check(r.code == ExitNotFound, "flags after the operation are parsed");
        check(contains(r.out, "No prim at path"), "and the operation reports its own error");
    }

    {
        const Result r = runCli({ "--command", "stage.children / --limit" });
        check(r.code == ExitUsage, "a flag without a value is a usage error");
        check(contains(r.out, "needs a value"), "and says so");
    }

    {
        const Result r = runCli({ "--command", "--limit 3 stage.children /" });
        check(r.code == ExitUsage, "flags must come after the operation name");
    }

    // ------------------------------------------------------------ read ops ---

    {
        const Result r = kitchen({ "stage.info" });
        check(r.code == ExitOk, "stage.info succeeds on Kitchen_set");
        check(contains(r.out, "\"defaultPrim\":\"/Kitchen_set\""), "stage.info reports the default prim");
        check(contains(r.out, "\"primCount\"") == false, "stage.info stays small");
    }

    {
        const Result r = kitchen({ "stage.stats" });
        check(contains(r.out, "\"primCount\":2742"), "stage.stats counts the prims");
    }

    {
        const Result r = kitchen({ "stage.children /Kitchen_set --limit 1 --fields path" });
        const std::vector<std::string> rows = lines(r.out);
        check(rows.size() == 2, "batch mode writes one JSON line per operation");
        check(contains(rows[1], "\"truncated\":true"), "--limit marks the result truncated");
        check(contains(rows[1], "\"limit\":1"), "--limit reports the applied limit");
        check(contains(r.err, "Truncated to --limit 1"), "--limit adds a note on stderr");
        check(!contains(rows[1], "typeName"), "--fields drops the other keys");
    }

    {
        const Result r = kitchen({ "stage.info --fields defaultPrim,nope" });
        check(contains(r.out, "\"defaultPrim\":\"/Kitchen_set\""), "--fields keeps the requested key");
        check(!contains(r.out, "layerCount"), "--fields filters a flat object");
        check(contains(r.out, "Unknown field 'nope'"), "an unknown field is reported as a note");
        check(contains(r.err, "Unknown field 'nope'"), "and the note is mirrored on stderr");
    }

    {
        const Result r = kitchen({ "stage.find Stove --limit 3 --fields path" });
        check(contains(r.out, "\"matchCount\":164"), "stage.find counts every match");
        check(contains(r.out, "StoveArea_grp"), "stage.find matches by name part");
        check(contains(r.out, "\"truncated\":true"), "stage.find honours --limit");
    }

    {
        const Result r = kitchen({ "prim.get " + kStove, "attr.list " + kStove + " --fields name" });
        check(contains(r.out, "\"typeName\":\"Xform\""), "prim.get reports the prim type");
        check(contains(r.out, "\"xformOp:translate\""), "attr.list lists the attributes");
        check(!contains(r.out, "Unknown field"), "a field present in one row group is not unknown");
    }

    {
        const Result r = kitchen({ "attr.get " + kStove + " xformOp:translate" });
        check(contains(r.out, "\"typeName\":\"double3\""), "attr.get reports the value type");
        check(contains(r.out, "\"value\":["), "attr.get prints a tuple as an array");
    }

    // ------------------------------------------------------------ edit ops ---

    {
        const Result r = kitchen({ "prim.define /Kitchen_set/CliTestPrim Xform --dry-run",
                                   "history.list",
                                   "stage.children /Kitchen_set --fields path --limit 500" });
        const std::vector<std::string> rows = lines(r.out);
        check(contains(r.out, "\"dryRun\":true"), "a dry run is marked as such");
        check(rows.size() == 4 && !contains(rows[2], "CliTestPrim"), "a dry run leaves the history clean");
        check(rows.size() == 4 && !contains(rows[3], "CliTestPrim"), "a dry run leaves the stage untouched");
    }

    {
        const Result r = kitchen({ "prim.define /Kitchen_set/CliTestPrim Xform",
                                   "prim.define /Kitchen_set/CliTestPrim Xform",
                                   "stage.children /Kitchen_set --fields path --limit 500",
                                   "history.undo", "history.undo",
                                   "stage.children /Kitchen_set --fields path --limit 500" });
        check(r.code == ExitOk, "defining a prim twice succeeds");
        const std::vector<std::string> rows = lines(r.out);
        check(rows.size() == 7, "each operation answered exactly once");
        check(contains(rows[3], "CliTestPrim"), "define prim added the prim");
        // The define is idempotent, so both history entries must be undone.
        check(rows.size() == 7 && !contains(rows[6], "CliTestPrim"), "history.undo removed it again");
    }

    {
        const Result r = kitchen({ "attr.set " + kStove + " cliFloat 3.5 float",
                                   "attr.get " + kStove + " cliFloat",
                                   "attr.set " + kStove + " cliVec 1,2,3 float3",
                                   "attr.get " + kStove + " cliVec",
                                   "attr.set " + kStove + " cliTok stoveToken token",
                                   "attr.get " + kStove + " cliTok",
                                   "history.undo", "history.undo", "history.undo",
                                   "attr.get " + kStove + " cliTok" });
        // The trailing attr.get fails after the undos, and the batch reports
        // that failure through the exit code.
        check(r.code == ExitNotFound, "the batch exit code follows the failing operation");
        const std::vector<std::string> rows = lines(r.out);
        check(contains(rows[2], "\"value\":3.5"), "float round-trip");
        check(contains(rows[4], "\"value\":[1,2,3]"), "float3 round-trip");
        check(contains(rows[6], "\"value\":\"stoveToken\""), "token round-trip");
        check(contains(rows[10], "\"code\":\"notFound\""), "undo restored the cleared attributes");
    }

    {
        const Result r = kitchen({ "attr.set " + kStove + " cliArr (1,2,3),(4,5,6) float3[]",
                                   "attr.get " + kStove + " cliArr" });
        check(r.code == ExitOk, "a tuple array parses");
        check(contains(r.out, "[[1,2,3],[4,5,6]]"), "a tuple array round-trips");
    }

    {
        const Result r = kitchen({ "attr.set " + kStove + " cliBad notatype 1" });
        check(r.code == ExitInvalidValue, "an unknown value type exits 3");
        check(contains(r.out, "\"code\":\"invalidValue\""), "and reports invalidValue");
        check(contains(r.out, "attr.types"), "the hint points at the type list");
    }

    {
        const Result r = kitchen({ "attr.set /Nope/Missing cliFloat 1 float" });
        check(r.code == ExitNotFound, "writing to a missing prim exits 4");
    }

    {
        const std::string scratch = std::string(DATA_DIR) + "/CliTestsScratch.usd";
        std::error_code ec;
        std::filesystem::remove(scratch, ec);
        const Result r = kitchen({ "document.new", "prim.define /CliPrim Xform",
                                   "document.save " + scratch, "document.open " + scratch });
        check(r.code == ExitOk, "document.save writes and reopens the file");
        check(contains(r.out, "\"undoable\":false"), "document.save is reported as not undoable");
        std::filesystem::remove(scratch, ec);
    }

    {
        const Result r = kitchen({ "prim.define /Kitchen_set/CliTestPrim Xform", "document.new",
                                   "history.undo" });
        check(r.code == ExitOk, "history.undo after document.new is accepted");
        check(contains(r.out, "\"canUndo\":true"), "history.undo reports what is left");
    }

    {
        const Result r = kitchen({ "history.undo", "history.undo" });
        check(contains(r.out, "Nothing to undo") || contains(r.out, "cannot be undone") ||
              contains(r.out, "\"ok\":false"), "undoing an empty history fails gracefully");
    }

    // --------------------------------------------------------------- text ----

    {
        const Result r = runCli({ "--text", "--version" });
        check(r.code == ExitOk && !contains(r.out, "{"), "--text --version prints plain text");
    }

    {
        const Result r = runCli({ "--text", "stage.fine" });
        check(contains(r.err, "Error [usage] stage.fine"), "text mode reports errors on stderr");
        check(contains(r.err, "Hint:"), "text mode keeps the hint");
        check(!contains(r.out, "\"ok\""), "text mode writes no JSON");
    }

    {
        const Result r = runCli({ "--limit", "not-a-number", "stage.info" });
        check(r.code == ExitUsage, "a bad --limit value exits 2");
    }

    check(ExitUsage == 2 && ExitInvalidValue == 3 && ExitNotFound == 4 && ExitFailed == 5 &&
              ExitInternal == 6, "exit codes are distinct and documented");

    std::cout << (failures == 0 ? "ALL CLI CHECKS PASSED" : "FAILURES: ") << (failures ? failures : 0)
              << std::endl;
    return failures == 0 ? 0 : 1;
}
