// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "../AppContext.h"
#include "../Commands/CommandRegistry.h"
#include "../Commands/QueryRegistry.h"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

// Machine-first command line front-end for USDStudio.
//
// Design rules (see Doc/USD-Studion-Research/Clippings/"wie sieht eine CLI aus
// wenn sie fuer KI ideal ist".md):
//   * one JSON document per operation on stdout, everything else on stderr
//   * never interactive: every option is a flag, no prompts, no paging
//   * deterministic, compact output; --limit/--fields keep answers cheap
//   * errors carry a machine-readable code plus a hint that says what to run
//   * distinct exit codes so a caller can branch without parsing text
//   * self-documenting: --help, --schema, --agent-help
namespace UsdStudio::Cli
{
// Exit codes. 0/2 follow the usual conventions; the rest separate the failure
// classes an agent has to react to differently.
enum ExitCode
{
    ExitOk = 0,           // the operation(s) succeeded
    ExitUsage = 2,        // unknown command/flag, wrong number of arguments
    ExitInvalidValue = 3, // an argument or typed value could not be parsed
    ExitNotFound = 4,     // a file, prim or attribute does not exist
    ExitFailed = 5,       // the operation was refused or failed
    ExitInternal = 6,     // unexpected error (OpenUSD or internal invariant)
};

// Default name of the local pipe a running UsdStudio GUI listens on for
// remote control ('usdstudio --remote'). A second GUI instance falls back
// to "<name>-<pid>"; the GUI status bar shows the active name.
inline constexpr const char* kRemotePipeName = "usdstudio-remote";

// Reusable evaluator: runs one operation line against a caller-owned
// Document/CommandHistory pair. The CLI process uses it internally, and the
// GUI's remote-control server reuses it, so a remote agent sees exactly the
// same JSON contract (envelope, flags, exit codes) as the local CLI.
class CliSession
{
public:
    CliSession(AppContext& context, const CommandRegistry& commands, const QueryRegistry& queries);

    struct Result
    {
        std::string json; // one compact JSON envelope line, no newline
        int exitCode = ExitOk;
    };

    // 'line' is one operation: "<name> [args] [--limit N] [--fields a,b] [--dry-run]".
    // Never throws: failures arrive as an ok:false envelope with an exit code.
    Result evaluate(std::string_view line) const;

private:
    AppContext* context_;
    const CommandRegistry* commands_;
    const QueryRegistry* queries_;
    std::vector<std::string> allOperationNames_;
};

// Runs one usdstudio invocation with 'args' given WITHOUT the program name.
// Results are written to 'out', diagnostics and text-mode errors to 'err'.
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// Same, driven straight from main()'s argv.
int run(int argc, char* const* argv, std::ostream& out, std::ostream& err);

} // namespace UsdStudio::Cli
