// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#include "../../Public/Cli/Cli.h"

#include "../../Public/AppContext.h"
#include "../../Public/Commands/AppCommands.h"
#include "../../Public/Commands/CommandError.h"
#include "../../Public/Commands/CommandHistory.h"
#include "../../Public/Commands/CommandRegistry.h"
#include "../../Public/Commands/QueryRegistry.h"
#include "../../Public/Cli/Json.h"
#include "../../Public/Cli/TypedValue.h"
#include "../../Public/Document.h"

#include "pxr/pxr.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace UsdStudio::Cli
{
namespace
{
using ArgSpan = std::span<const std::string_view>;

constexpr const char* kProgram = "usdstudio";
constexpr const char* kVersion = "0.1.0";
constexpr long long kDefaultRowLimit = 200;
constexpr std::size_t kMaxSuggestions = 8;

// Undo-stack actions. They belong to the front-end rather than to the
// document, so they are implemented here instead of as ICommand objects - but
// --help/--schema advertise them exactly like commands and queries.
constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kActions = {{
    { "history.undo", "Undo the last undoable command and report what was reverted." },
    { "history.redo", "Redo the last undone command." },
}};

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct Flags
{
    bool json = true;
    bool pretty = false;
    bool dryRun = false;
    long long limit = kDefaultRowLimit;
    std::vector<std::string> fields;
    std::vector<std::string> batch;      // --command "<line>"
    bool readStdin = false;              // --commands -
    bool help = false;
    std::vector<std::string> helpTopics;
    bool schema = false;
    bool agentHelp = false;
    bool types = false;
    bool version = false;
    bool remote = false;               // --remote: run in a live UsdStudio GUI
    std::string pipe;                  // --pipe <name>: remote pipe override
    std::vector<std::string> positional; // raw argv tail: the operation line
};

void splitCsv(std::string_view text, std::vector<std::string>& out)
{
    std::size_t pos = 0;
    while (pos <= text.size())
    {
        const std::size_t comma = text.find(',', pos);
        const std::size_t end = (comma == std::string_view::npos) ? text.size() : comma;
        if (end > pos)
            out.emplace_back(text.substr(pos, end - pos));
        if (comma == std::string_view::npos)
            break;
        pos = comma + 1;
    }
}

// Flags may appear before *and* after the operation line, so the parser never
// simply stops at the first non-flag token: known flags are consumed wherever
// they occur and everything else is collected into the operation line.
bool isFlagToken(const std::string& token)
{
    return !token.empty() && token[0] == '-' && token != "-";
}

bool knownFlag(const std::string& token)
{
    static constexpr std::array<std::string_view, 22> names = {{
        "--json", "--text", "--compact", "--pretty", "--dry-run", "--yes", "-y",
        "--schema", "--agent-help", "--llms", "--types", "--version", "-v",
        "--help", "-h", "-?", "--limit", "--fields", "--command", "--commands",
        "--remote", "--pipe"
    }};
    for (const std::string_view name : names)
        if (token == name)
            return true;
    return false;
}

// A value such as '-1.5' or '-3' is an argument, not a mistyped flag.
bool looksNumeric(const std::string& token)
{
    if (token.size() < 2)
        return false;
    const char c = token[1];
    return (c >= '0' && c <= '9') || c == '.';
}

// Parses all flags. The remaining tokens are kept verbatim so file names and
// value literals keep their exact spelling.
bool parseFlags(const std::vector<std::string>& args, Flags& flags, std::string& error, std::string& hint)
{
    const std::string usageHint = "Run 'usdstudio --help' for all flags and operations.";
    const auto needValue = [&](const std::string& flag, const std::string& example)
    {
        error = "Flag '" + flag + "' needs a value.";
        hint = example;
    };

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& token = args[i];
        if (!isFlagToken(token) || looksNumeric(token))
        {
            flags.positional.push_back(token);
            continue;
        }
        if (!knownFlag(token))
        {
            error = "Unknown flag '" + token + "'.";
            hint = usageHint;
            return false;
        }

        const bool hasNext = (i + 1 < args.size());

        if (token == "--json") { flags.json = true; continue; }
        if (token == "--text") { flags.json = false; continue; }
        if (token == "--compact") { flags.pretty = false; continue; }
        if (token == "--pretty") { flags.pretty = true; continue; }
        if (token == "--dry-run") { flags.dryRun = true; continue; }
        if (token == "--yes" || token == "-y") continue; // documented no-op
        if (token == "--schema") { flags.schema = true; continue; }
        if (token == "--agent-help" || token == "--llms") { flags.agentHelp = true; continue; }
        if (token == "--types") { flags.types = true; continue; }
        if (token == "--version" || token == "-v") { flags.version = true; continue; }
        if (token == "--remote") { flags.remote = true; continue; }

        if (token == "--help" || token == "-h" || token == "-?")
        {
            flags.help = true;
            // An optional topic may follow: '--help stage.children'.
            if (hasNext && !args[i + 1].empty() && args[i + 1][0] != '-')
                flags.helpTopics.push_back(args[++i]);
            continue;
        }

        if (token == "--limit")
        {
            if (!hasNext) { needValue(token, "Use --limit <rows>, for example --limit 20."); return false; }
            try
            {
                flags.limit = std::stoll(args[++i]);
            }
            catch (const std::exception&)
            {
                error = "Flag '--limit' needs a whole number, got '" + args[i] + "'.";
                hint = "Use --limit <rows>. Pass a large number to disable truncation.";
                return false;
            }
            if (flags.limit < 0)
            {
                error = "Flag '--limit' must not be negative.";
                hint = "Use --limit <rows>, or a large number to disable truncation.";
                return false;
            }
            continue;
        }

        if (token == "--fields")
        {
            if (!hasNext) { needValue(token, "Use --fields name,name with the keys of the result."); return false; }
            splitCsv(args[++i], flags.fields);
            continue;
        }

        if (token == "--pipe")
        {
            if (!hasNext) { needValue(token, "Use --pipe <name> with the pipe name shown in the UsdStudio status bar."); return false; }
            flags.pipe = args[++i];
            continue;
        }

        if (token == "--command")
        {
            if (!hasNext) { needValue(token, "Quote the line: --command \"stage.children /Root\"."); return false; }
            flags.batch.push_back(args[++i]);
            continue;
        }

        if (token == "--commands")
        {
            if (!hasNext) { needValue(token, "Use --commands - to read one operation per line from stdin."); return false; }
            const std::string value = args[++i];
            if (value == "-")
                flags.readStdin = true;
            else
                flags.batch.push_back(value);
            continue;
        }

        error = "Unknown flag '" + token + "'.";
        hint = usageHint;
        return false;
    }
    return true;
}

// Rebuilds one operation line from the argv tail, re-quoting tokens that hold
// spaces so CommandLine's quote-aware splitter sees one argument again.
std::string joinPositional(const std::vector<std::string>& tokens)
{
    std::string line;
    for (const std::string& token : tokens)
    {
        if (!line.empty())
            line += ' ';
        if (token.find_first_of(" \t\"") == std::string::npos)
            line += token;
        else
            line += '"' + token + '"';
    }
    return line;
}

// ---------------------------------------------------------------------------
// Outcomes
// ---------------------------------------------------------------------------

struct Outcome
{
    std::string name;
    bool ok = false;
    Json data = Json::object();
    std::string code; // machine-readable category: usage/invalidValue/...
    std::string message;
    std::string hint;
    std::vector<std::string> notes; // truncation, unknown --fields keys, ...
    bool dryRun = false;
    int exitCode = ExitOk;
};

int exitCodeFor(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::InvalidValue: return ExitInvalidValue;
    case ErrorCode::NotFound:     return ExitNotFound;
    case ErrorCode::Precondition: return ExitFailed;
    case ErrorCode::Failed:       return ExitFailed;
    case ErrorCode::Internal:     return ExitInternal;
    }
    return ExitInternal;
}

void fail(Outcome& o, ErrorCode code, std::string message, std::string hint)
{
    o.ok = false;
    o.code = std::string(errorCodeName(code));
    o.message = std::move(message);
    o.hint = std::move(hint);
    o.exitCode = exitCodeFor(code);
}

void failUsage(Outcome& o, std::string message, std::string hint)
{
    o.ok = false;
    o.code = "usage";
    o.message = std::move(message);
    o.hint = std::move(hint);
    o.exitCode = ExitUsage;
}

// ---------------------------------------------------------------------------
// Result post-processing: --limit and --fields keep answers token-cheap
// ---------------------------------------------------------------------------

void applyLimit(Json& data, long long limit, bool& truncated)
{
    if (!data.isObject())
        return;
    for (Json::Pair& member : data.members())
    {
        Json& value = member.second;
        if (!value.isArray() || static_cast<long long>(value.items().size()) <= limit)
            continue;
        value.items().resize(static_cast<std::size_t>(limit));
        truncated = true;
    }
}

void applyFields(Json& data, const std::vector<std::string>& fields, std::vector<std::string>& notes)
{
    if (fields.empty() || (!data.isObject() && !data.isArray()))
        return;

    // A field is only reported as unknown when no row of the result has it.
    std::vector<bool> seen(fields.size(), false);

    const auto keep = [&](Json& object)
    {
        std::vector<Json::Pair> kept;
        kept.reserve(fields.size());
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            for (Json::Pair& member : object.members())
            {
                if (member.first != fields[i])
                    continue;
                kept.emplace_back(member.first, std::move(member.second));
                seen[i] = true;
                break;
            }
        }
        object.members().swap(kept);
    };

    if (data.isObject())
    {
        // '{ path, children: [ {...}, ... ] }' is the usual shape: filter the
        // rows, keep the row groups, and trim the surrounding scalar keys to
        // the requested fields (bookkeeping keys always survive). A flat
        // object is filtered by its own keys.
        static const std::vector<std::string> bookkeeping{
            "truncated", "limit", "matchCount", "childCount", "count", "total" };
        bool rowFiltering = false;
        for (Json::Pair& member : data.members())
        {
            Json& value = member.second;
            if (!value.isArray())
                continue;
            bool hasRows = false;
            for (Json& item : value.items())
            {
                if (item.isObject())
                    hasRows = true;
            }
            if (!hasRows)
                continue;
            rowFiltering = true;
            for (Json& item : value.items())
            {
                if (item.isObject())
                    keep(item);
            }
        }
        if (rowFiltering)
        {
            std::vector<Json::Pair> kept;
            for (Json::Pair& member : data.members())
            {
                const bool wanted = std::find(fields.begin(), fields.end(), member.first) != fields.end();
                const bool book = std::find(bookkeeping.begin(), bookkeeping.end(), member.first) != bookkeeping.end();
                if (member.second.isArray() || wanted || book)
                {
                    if (wanted)
                    {
                        // A top-level key that matches a field counts as seen.
                        const std::size_t idx =
                            static_cast<std::size_t>(std::find(fields.begin(), fields.end(), member.first) - fields.begin());
                        seen[idx] = true;
                    }
                    kept.emplace_back(std::move(member.first), std::move(member.second));
                }
            }
            data.members().swap(kept);
        }
        else
            keep(data);
    }
    else
    {
        for (Json& item : data.items())
        {
            if (item.isObject())
                keep(item);
        }
    }

    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        if (seen[i])
            continue;
        const std::string note = "Unknown field '" + fields[i] + "'.";
        if (std::find(notes.begin(), notes.end(), note) == notes.end())
            notes.push_back(note);
    }
}

// ---------------------------------------------------------------------------
// Operation dispatch
// ---------------------------------------------------------------------------

std::vector<std::string> allOperationNames(const CommandRegistry& commands, const QueryRegistry& queries)
{
    std::vector<std::string> names;
    for (const std::string& name : commands.commandNames())
        names.push_back(name);
    for (const std::string& name : queries.queryNames())
        names.push_back(name);
    for (const auto& action : kActions)
        names.emplace_back(action.first);
    std::sort(names.begin(), names.end());
    return names;
}

// Bounded Levenshtein distance: only used to suggest operations for typos.
std::size_t editDistance(std::string_view a, std::string_view b)
{
    std::vector<std::size_t> previous(b.size() + 1);
    std::vector<std::size_t> current(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j)
        previous[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i)
    {
        current[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j)
        {
            const std::size_t substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0u : 1u);
            current[j] = std::min({ previous[j] + 1u, current[j - 1] + 1u, substitution });
        }
        std::swap(previous, current);
    }
    return previous[b.size()];
}

std::string suggestNames(std::string_view name, const std::vector<std::string>& all)
{
    std::string out;
    std::size_t found = 0;
    for (const std::string& candidate : all)
    {
        const bool similar = editDistance(name, candidate) <= 2;
        if (candidate.rfind(name, 0) != 0 && candidate.find(name) == std::string::npos && !similar)
            continue;
        if (found)
            out += ", ";
        out += candidate;
        if (++found == kMaxSuggestions)
            break;
    }
    return out;
}

bool checkArgCount(Outcome& o, const std::string& usage, const std::vector<ArgSpec>& specs, std::size_t given)
{
    std::size_t required = 0;
    for (const ArgSpec& spec : specs)
    {
        if (!spec.optional)
            ++required;
    }
    if (given >= required && given <= specs.size())
        return true;

    std::string expected = std::to_string(required);
    if (specs.size() != required)
        expected += " to " + std::to_string(specs.size());
    failUsage(o,
        "Wrong number of arguments for '" + usage + "'. Got " + std::to_string(given) +
        ", expected " + expected + ".",
        "Usage: usdstudio " + usage);
    return false;
}

Json documentState(AppContext& ctx)
{
    Json data = Json::object();
    data.setString("fileName", ctx.document.getFileName());
    data.set("isModified", ctx.document.isModified());
    return data;
}

void runHistoryAction(const std::string& name, AppContext& ctx, Outcome& o)
{
    const bool undo = (name == "history.undo");
    if (undo && !ctx.history.canUndo())
        throw CommandError::failed("Nothing to undo.", "Run 'history.list' to see the undo stack.");
    if (!undo && !ctx.history.canRedo())
        throw CommandError::failed("Nothing to redo.", "Run 'history.list' to see the redo stack.");

    // Read the label before moving the cursor: the entry being undone sits just
    // below the cursor, the one being redone just above it.
    const std::vector<std::string> entries = ctx.history.descriptions();
    const std::size_t cursor = ctx.history.undoDepth();
    const std::string label = entries[cursor - (undo ? 1u : 0u)];

    if (undo)
        ctx.history.undo();
    else
        ctx.history.redo();

    o.data = documentState(ctx);
    o.data.setString("action", undo ? "undo" : "redo");
    o.data.setString("description", label);
    o.data.set("undoDepth", static_cast<long long>(ctx.history.undoDepth()));
    o.data.set("redoDepth", static_cast<long long>(ctx.history.redoDepth()));
    o.data.set("canUndo", ctx.history.canUndo());
    o.data.set("canRedo", ctx.history.canRedo());
    o.ok = true;
}

void runCommand(Outcome& o, const CommandRegistry::CommandInfo& info, ArgSpan args, AppContext& ctx,
    const Flags& flags, const CommandRegistry& commands)
{
    std::unique_ptr<ICommand> command = commands.create(info.name, args);
    const std::string description = command->description();

    if (flags.dryRun)
    {
        // Execute and roll back, so everything the document would complain
        // about (missing prim, wrong value type) is checked for real while no
        // state - not even the undo history - is left changed.
        if (!command->isUndoable())
            throw CommandError::precondition(
                "'" + info.name + "' has effects that cannot be rolled back, so it is not executed under --dry-run.",
                "Run it without --dry-run, or inspect the current state with 'stage.info'.");
        command->execute();
        command->undo();
        o.dryRun = true;
    }
    else
    {
        ctx.history.executeAndPush(std::move(command));
    }

    o.data = documentState(ctx);
    o.data.setString("description", description);
    o.data.set("undoable", info.undoable);
    if (flags.dryRun)
        o.data.set("dryRun", true);
    o.ok = true;
}

// A batch line may shrink its own answer, so --limit/--fields/--dry-run are
// also understood *inside* an operation line. Output-format flags stay global
// to keep one batch readable.
bool extractLineFlags(ArgSpan raw, Flags& flags, std::vector<std::string>& kept, Outcome& o)
{
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        const std::string token(raw[i]);
        if (!isFlagToken(token) || looksNumeric(token))
        {
            kept.push_back(token);
            continue;
        }

        if (token == "--limit" || token == "--fields")
        {
            if (i + 1 >= raw.size())
            {
                failUsage(o, "Flag '" + token + "' needs a value.",
                    token == "--limit" ? "Use --limit <rows>, for example: stage.children / --limit 20."
                                       : "Use --fields name,name, for example: stage.children / --fields path,typeName.");
                return false;
            }
            const std::string value(raw[++i]);
            if (token == "--limit")
            {
                try
                {
                    flags.limit = std::stoll(value);
                }
                catch (const std::exception&)
                {
                    failUsage(o, "Flag '--limit' needs a whole number, got '" + value + "'.",
                        "Use --limit <rows>, or a large number to disable truncation.");
                    return false;
                }
                if (flags.limit < 0)
                {
                    failUsage(o, "Flag '--limit' must not be negative.", "Use --limit <rows>.");
                    return false;
                }
            }
            else
            {
                splitCsv(value, flags.fields);
            }
            continue;
        }

        if (token == "--dry-run") { flags.dryRun = true; continue; }

        failUsage(o, "Flag '" + token + "' cannot be used inside an operation line.",
            "Pass it before the operation, for example: usdstudio " + token +
            " --command \"stage.children /\".");
        return false;
    }
    return true;
}

void runOperation(const std::string& line, AppContext& ctx, const CommandRegistry& commands,
    const QueryRegistry& queries, Flags flags, const std::vector<std::string>& allNames, Outcome& o)
{
    const CommandLine parsed(line);
    o.name = std::string(parsed.name());

    if (o.name.empty())
    {
        failUsage(o, "Empty operation line.", "Run 'usdstudio --help', or pass an operation such as 'stage.info'.");
        return;
    }
    if (isFlagToken(o.name))
    {
        failUsage(o, "Expected an operation name, got '" + o.name + "'.",
            "Flags come before the operation: usdstudio --limit 20 --command \"stage.children /\".");
        return;
    }

    std::vector<std::string> kept;
    if (!extractLineFlags(parsed.args(), flags, kept, o))
        return;
    std::vector<std::string_view> storage;
    storage.reserve(kept.size());
    for (const std::string& token : kept)
        storage.push_back(token);
    const ArgSpan args{ storage };

    try
    {
        if (o.name == "history.undo" || o.name == "history.redo")
        {
            runHistoryAction(o.name, ctx, o);
        }
        else if (const CommandRegistry::CommandInfo* info = commands.find(o.name))
        {
            if (!checkArgCount(o, info->usage, info->args, args.size()))
                return;
            runCommand(o, *info, args, ctx, flags, commands);
        }
        else if (const QueryRegistry::QueryInfo* info = queries.find(o.name))
        {
            if (!checkArgCount(o, info->usage, info->args, args.size()))
                return;
            o.data = queries.run(o.name, args, ctx);
            o.ok = true;
        }
        else
        {
            const std::string nearby = suggestNames(o.name, allNames);
            failUsage(o, "Unknown operation '" + o.name + "'.",
                nearby.empty() ? "Run 'usdstudio --help' (or --schema) for the full list."
                               : "Did you mean: " + nearby + "?");
            return;
        }
    }
    catch (const CommandError& e)
    {
        fail(o, e.code(), e.what(), e.hint());
    }
    catch (const std::invalid_argument& e)
    {
        fail(o, ErrorCode::InvalidValue, e.what(),
            "Check the argument spelling. 'usdstudio --schema' lists each argument's type.");
    }
    catch (const std::exception& e)
    {
        fail(o, ErrorCode::Internal, e.what(),
            "Unexpected failure. Run 'stage.info' to check the document state, then retry.");
    }

    if (!o.ok)
        return;

    bool truncated = false;
    applyLimit(o.data, flags.limit, truncated);
    if (truncated)
    {
        o.data.set("truncated", true);
        o.data.set("limit", flags.limit);
        o.notes.push_back("Truncated to --limit " + std::to_string(flags.limit) + ". Raise --limit or narrow the query.");
    }
    applyFields(o.data, flags.fields, o.notes);
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

Json responseJson(const Outcome& o)
{
    Json response = Json::object();
    response.set("ok", o.ok);
    response.setString("cmd", o.name);
    if (o.dryRun)
        response.set("dryRun", true);
    if (o.ok)
        response.set("data", o.data);
    else
    {
        Json error = Json::object();
        error.setString("code", o.code);
        error.setString("message", o.message);
        error.setString("hint", o.hint);
        response.set("error", std::move(error));
    }
    if (!o.notes.empty())
    {
        Json notes = Json::array();
        for (const std::string& note : o.notes)
            notes.pushBack(Json(note));
        response.set("notes", std::move(notes));
    }
    return response;
}

std::string scalarText(const Json& value)
{
    switch (value.type())
    {
    case Json::Type::Null:   return "null";
    case Json::Type::Bool:   return value.dump();
    case Json::Type::Number: return value.dump();
    case Json::Type::String: return std::string(value.asString());
    case Json::Type::Array:  return "[" + std::to_string(value.size()) + " values]";
    case Json::Type::Object: return "{" + std::to_string(value.size()) + " fields}";
    }
    return {};
}

// Compact indented rendering for '--text'. Structure is never dropped: an
// LLM or human reading the same answer sees every key that JSON would carry.
void writeText(std::ostream& os, const Json& value, unsigned depth = 0)
{
    const std::string indent(2 * depth, ' ');
    if (value.isObject())
    {
        for (const Json::Pair& member : value.members())
        {
            const Json& child = member.second;
            if (child.isObject() || child.isArray())
            {
                if (child.size() == 0)
                {
                    os << indent << member.first << ": (empty)\n";
                    continue;
                }
                os << indent << member.first << ":\n";
                writeText(os, child, depth + 1);
                continue;
            }
            os << indent << member.first << ": " << scalarText(child) << "\n";
        }
        return;
    }
    if (value.isArray())
    {
        for (const Json& item : value.items())
        {
            if (item.isObject() || item.isArray())
            {
                os << indent << "-\n";
                writeText(os, item, depth + 1);
            }
            else
                os << indent << "- " << scalarText(item) << "\n";
        }
    }
}

void writeOutcome(std::ostream& out, std::ostream& err, const Outcome& o, const Flags& flags, bool batch)
{
    if (flags.json)
    {
        // JSONL: one line per operation. Pretty printing is off in batch mode
        // so a caller can pipe the whole run into a line-based parser.
        out << responseJson(o).dump(!flags.pretty || batch) << "\n";
        for (const std::string& note : o.notes)
            err << "Note: " << note << "\n";
        return;
    }

    if (o.ok)
    {
        if (o.dryRun)
            out << "Dry run - nothing changed: " << o.name << "\n";
        else
            out << "OK: " << o.name << "\n";
        writeText(out, o.data);
    }
    else
    {
        err << "Error [" << o.code << "] " << o.name << ": " << o.message << "\n";
        if (!o.hint.empty())
            err << "Hint: " << o.hint << "\n";
    }
    for (const std::string& note : o.notes)
        err << "Note: " << note << "\n";
}

void writeUsageError(std::ostream& out, std::ostream& err, const Flags& flags, const std::string& message,
    const std::string& hint)
{
    Outcome o;
    failUsage(o, message, hint);
    writeOutcome(out, err, o, flags, false);
}

// ---------------------------------------------------------------------------
// Self-documentation: one table feeds --help, --schema and --agent-help
// ---------------------------------------------------------------------------

struct FlagEntry
{
    std::string_view flags;
    std::string_view value;  // empty for booleans
    std::string_view help;
};

struct ExitCodeEntry
{
    long long code;
    std::string_view meaning;
};

constexpr std::array<ExitCodeEntry, 6> kExitCodes = {{
    { ExitOk, "Success." },
    { ExitUsage, "Usage error: unknown operation or flag, or wrong argument count." },
    { ExitInvalidValue, "An argument could not be parsed as the declared type." },
    { ExitNotFound, "A prim, attribute or file was not found." },
    { ExitFailed, "A precondition or the operation itself failed." },
    { ExitInternal, "Unexpected internal failure." },
}};

constexpr std::array<FlagEntry, 16> kFlags = {{
    { "--json", "", "Default. One compact JSON line per operation on stdout." },
    { "--text", "", "Human readable output instead of JSON." },
    { "--pretty", "", "Indent the JSON. Ignored in batch mode." },
    { "--compact", "", "Undo --pretty." },
    { "--limit", "<rows>", "Rows per array in a result (default 200). Sets data.truncated." },
    { "--fields", "<a,b>", "Keep only these keys of the result (rows are filtered per row)." },
    { "--dry-run", "", "Validate a command by executing and rolling it back. History untouched." },
    { "--command", "\"<line>\"", "Queue one operation. Repeatable. Also --commands <line>." },
    { "--commands", "-", "Read one operation per line from stdin, then stop." },
    { "--schema", "", "Machine readable description of every operation, flag and exit code." },
    { "--agent-help", "", "Condensed instruction sheet for an AI agent (also --llms)." },
    { "--types", "", "Value type names accepted by attr.set." },
    { "--help", "[topic]", "Usage, or the JSON description of one operation/flag." },
    { "--yes", "", "Accepted and ignored: the CLI is never interactive." },
    { "--remote", "", "Run the operations in a running UsdStudio GUI (live document, shared undo) instead of a fresh empty one." },
    { "--pipe", "<name>", "Remote pipe name (default \"usdstudio-remote\"). Needed when several GUIs are running." },
}};

Json flagSpec()
{
    Json flags = Json::array();
    for (const FlagEntry& entry : kFlags)
    {
        Json flag = Json::object();
        flag.setString("name", entry.flags);
        if (!entry.value.empty())
            flag.setString("value", entry.value);
        flag.setString("help", entry.help);
        flags.pushBack(std::move(flag));
    }
    return flags;
}

Json exitCodeSpec()
{
    Json out = Json::array();
    for (const ExitCodeEntry& entry : kExitCodes)
    {
        Json item = Json::object();
        item.set("code", entry.code);
        item.setString("meaning", entry.meaning);
        out.pushBack(std::move(item));
    }
    return out;
}

Json argSpecJson(const std::vector<ArgSpec>& specs)
{
    Json args = Json::array();
    for (const ArgSpec& spec : specs)
    {
        Json arg = Json::object();
        arg.setString("name", spec.name);
        arg.setString("type", spec.type);
        arg.set("optional", spec.optional);
        args.pushBack(std::move(arg));
    }
    return args;
}

void appendArgs(std::string& usage, const std::vector<ArgSpec>& specs)
{
    for (const ArgSpec& spec : specs)
    {
        usage += ' ';
        if (spec.optional)
            usage += '[' + std::string(spec.name) + ']';
        else
            usage += std::string(spec.name);
    }
}

Json commandSpecJson(const CommandRegistry::CommandInfo& info)
{
    Json entry = Json::object();
    entry.setString("name", info.name);
    entry.setString("kind", "command");
    entry.setString("help", info.help);
    entry.setString("usage", std::string(kProgram) + " " + info.usage);
    entry.set("undoable", info.undoable);
    entry.set("args", argSpecJson(info.args));
    return entry;
}

Json querySpecJson(const QueryRegistry::QueryInfo& info)
{
    Json entry = Json::object();
    entry.setString("name", info.name);
    entry.setString("kind", "query");
    entry.setString("help", info.help);
    entry.setString("usage", std::string(kProgram) + " " + info.usage);
    entry.setString("returns", info.returns);
    entry.set("args", argSpecJson(info.args));
    return entry;
}

Json actionSpecJson(std::string_view name, std::string_view help)
{
    Json entry = Json::object();
    entry.setString("name", name);
    entry.setString("kind", "action");
    entry.setString("help", help);
    entry.setString("usage", std::string(kProgram) + " " + std::string(name));
    entry.set("args", Json::array());
    return entry;
}

Json operationSpecs(const CommandRegistry& commands, const QueryRegistry& queries, std::string_view kind)
{
    Json out = Json::array();
    if (kind == "command")
    {
        for (const std::string& name : commands.commandNames())
        {
            if (const CommandRegistry::CommandInfo* info = commands.find(name))
                out.pushBack(commandSpecJson(*info));
        }
    }
    else if (kind == "query")
    {
        for (const std::string& name : queries.queryNames())
        {
            if (const QueryRegistry::QueryInfo* info = queries.find(name))
                out.pushBack(querySpecJson(*info));
        }
    }
    else
    {
        for (const auto& action : kActions)
            out.pushBack(actionSpecJson(action.first, action.second));
    }
    return out;
}

Json valueTypesJson()
{
    Json out = Json::array();
    for (const std::string& type : supportedValueTypes())
        out.pushBack(Json(type));
    return out;
}

Json buildSchema(const CommandRegistry& commands, const QueryRegistry& queries)
{
    Json schema = Json::object();
    schema.setString("name", kProgram);
    schema.setString("version", kVersion);
    schema.setString("openUsd", std::to_string(PXR_VERSION));
    schema.setString("protocol",
        "stdout is JSON only: one line per operation {\"ok\":bool,\"cmd\":name,\"data\":object} on success, "
        "{\"ok\":false,\"cmd\":name,\"error\":{\"code\",\"message\",\"hint\"}} on failure. "
        "Logs and warnings go to stderr. Never interactive. "
        "With --remote the same envelopes come back from the running GUI, each with an extra \"exit\":<code> member.");
    schema.setString("invocation",
        "usdstudio [flags] <operation> [args]    |    usdstudio [flags] --command \"<operation> [args]\" "
        "   |   usdstudio [flags] --commands -    |   usdstudio --remote [flags] <operation> [args]");
    schema.setString("result", "data is an object; every array in it is capped by --limit (default 200) "
        "and sets data.truncated. --fields keeps only the listed keys.");
    schema.set("exitCodes", exitCodeSpec());
    schema.set("flags", flagSpec());
    schema.set("commands", operationSpecs(commands, queries, "command"));
    schema.set("queries", operationSpecs(commands, queries, "query"));
    schema.set("actions", operationSpecs(commands, queries, "action"));
    schema.set("valueTypes", valueTypesJson());
    return schema;
}

std::string usageText()
{
    std::string text;
    text += std::string(kProgram) + " " + kVersion + " - headless USD document tool (JSON on stdout)\n\n";
    text += "Usage:\n";
    text += std::string(kProgram) + " [flags] <operation> [args]\n";
    text += std::string(kProgram) + " [flags] --command \"<operation> [args]\" [--command \"...\"]\n";
    text += std::string(kProgram) + " [flags] --commands -    (one operation per line from stdin)\n";
    text += std::string(kProgram) + " --remote [flags] <operation> [args]    (run inside a running UsdStudio GUI)\n\n";

    text += "Flags:\n";
    for (const FlagEntry& entry : kFlags)
    {
        text += "  ";
        text += entry.flags;
        if (!entry.value.empty())
        {
            text += ' ';
            text += entry.value;
        }
        text += "\n      ";
        text += entry.help;
        text += '\n';
    }

    text += "\nExit codes:\n";
    for (const ExitCodeEntry& entry : kExitCodes)
    {
        text += "  " + std::to_string(entry.code) + "  ";
        text += entry.meaning;
        text += '\n';
    }
    text += "\nRun '";
    text += kProgram;
    text += " --schema' for a machine readable description of every operation, and '";
    text += kProgram;
    text += " --agent-help' for the AI-agent cheat sheet.\n";
    return text;
}

bool printTopicHelp(std::ostream& out, std::ostream& err, const Flags& flags, std::string_view topic,
    const CommandRegistry& commands, const QueryRegistry& queries, const std::vector<std::string>& allNames)
{
    for (const FlagEntry& entry : kFlags)
    {
        if (topic != entry.flags)
            continue;
        if (flags.json)
        {
            Json spec = Json::object();
            spec.setString("name", entry.flags);
            if (!entry.value.empty())
                spec.setString("value", entry.value);
            spec.setString("help", entry.help);
            out << spec.dump() << "\n";
        }
        else
            out << entry.flags << ' ' << entry.value << "\n  " << entry.help << "\n";
        return true;
    }

    if (const CommandRegistry::CommandInfo* info = commands.find(std::string(topic)))
    {
        if (flags.json)
            out << commandSpecJson(*info).dump() << "\n";
        else
        {
            out << info->usage << "\n  " << info->help
                << (info->undoable ? "\n  Undoable: yes" : "\n  Undoable: no") << "\n";
            for (const ArgSpec& arg : info->args)
                out << "    " << arg.name << ": " << arg.type << (arg.optional ? " (optional)" : "") << "\n";
        }
        return true;
    }

    if (const QueryRegistry::QueryInfo* info = queries.find(std::string(topic)))
    {
        if (flags.json)
            out << querySpecJson(*info).dump() << "\n";
        else
        {
            out << info->usage << "\n  " << info->help << "\n  Returns: " << info->returns << "\n";
            for (const ArgSpec& arg : info->args)
                out << "    " << arg.name << ": " << arg.type << (arg.optional ? " (optional)" : "") << "\n";
        }
        return true;
    }

    for (const auto& action : kActions)
    {
        if (topic != action.first)
            continue;
        const Json spec = actionSpecJson(action.first, action.second);
        if (flags.json)
            out << spec.dump() << "\n";
        else
            out << action.first << "\n  " << action.second << "\n";
        return true;
    }

    const std::string nearby = suggestNames(topic, allNames);
    writeUsageError(out, err, flags, "No help topic '" + std::string(topic) + "'.",
        nearby.empty() ? "Run 'usdstudio --help' for the flag list, or --schema for all operations."
                       : "Did you mean: " + nearby + "?");
    return false;
}

std::string agentHelpText(const CommandRegistry& commands, const QueryRegistry& queries)
{
    std::string t;
    t += kProgram;
    t += ' ';
    t += kVersion;
    t += " - headless USD document tool for AI agents (OpenUSD ";
    t += std::to_string(PXR_VERSION);
    t += ")\n\nHOW TO READ THE OUTPUT\n";
    t += "* stdout is machine readable and holds nothing else. Logs, warnings and progress go to stderr.\n";
    t += "* Success:  {\"ok\":true,\"cmd\":\"<name>\",\"data\":{...}}\n";
    t += "* Failure:  {\"ok\":false,\"cmd\":\"<name>\",\"error\":{\"code\":\"notFound\",\"message\":\"...\",\"hint\":\"...\"}}\n";
    t += "* Read error.hint before retrying: it names the command to run next.\n";
    t += "* Exit codes: ";
    for (std::size_t i = 0; i < kExitCodes.size(); ++i)
    {
        if (i)
            t += ", ";
        t += std::to_string(kExitCodes[i].code);
        t += "=";
        t += kExitCodes[i].meaning;
    }
    t += "\n\nRULES\n";
    t += "1. Inspect before editing: stage.info, stage.stats, stage.children <path>, prim.get <path>, "
         "attr.list <path>, attr.get <path> <attr>.\n";
    t += "2. Edit with prim.define and attr.set. Every edit is undoable: history.undo / history.redo. "
         "document.save writes to disk and is NOT undoable.\n";
    t += "3. Never guess value syntax. attr.types (or --types) lists type names and literal forms; "
         "attr.set takes a type name and a literal, e.g. attr.set /Root xfloat3 float3 \"1,2,3\".\n";
    t += "4. Validate with --dry-run before applying: the command runs and is rolled back, so errors surface "
         "without changing the document or the undo history.\n";
    t += "5. Keep answers small: --limit <rows> caps every array in a result (default 200, sets data.truncated), "
         "--fields a,b keeps only the keys you need.\n";
    t += "6. Batch independent steps in one process: --command \"<line>\" (repeatable) or --commands - "
         "(one operation per line on stdin). Output is JSON Lines; the exit code is the first failure.\n";
    t += "7. It is never interactive: no prompts, no paging. --yes is accepted and ignored.\n";
    t += "8. For the machine readable version of this contract, run --schema (flags, args, types, exit codes).\n";
    t += "9. --remote sends the operations to the running UsdStudio GUI over a local pipe: live document, "
         "same undo history as the user. Remote answers add an \"exit\":<code> member. No GUI listening: "
         "exit 5 with a hint to start the GUI (remote control is on by default; its status bar shows the pipe "
         "name; several GUIs: pick the name there and pass --pipe <name>). --help/--schema/--agent-help "
         "always answer locally.\n";

    t += "\nOPERATIONS\n";
    for (const std::string& name : queries.queryNames())
    {
        const QueryRegistry::QueryInfo* info = queries.find(name);
        if (!info)
            continue;
        std::string usage = info->usage;
        appendArgs(usage, info->args);
        t += "  " + usage + "\n      " + std::string(info->help) + "\n      returns: " + std::string(info->returns) + "\n";
    }
    for (const std::string& name : commands.commandNames())
    {
        const CommandRegistry::CommandInfo* info = commands.find(name);
        if (!info)
            continue;
        std::string usage = info->usage;
        appendArgs(usage, info->args);
        t += "  " + usage + "\n      " + std::string(info->help) +
             (info->undoable ? "   [undoable]" : "   [NOT undoable]") + "\n";
    }
    for (const auto& action : kActions)
        t += "  usdstudio " + std::string(action.first) + "\n      " + std::string(action.second) + "\n";

    t += "\nVALUE TYPES for attr.set: ";
    bool first = true;
    for (const std::string& type : supportedValueTypes())
    {
        t += first ? type : ", " + type;
        first = false;
    }
    t += "\n\nEXAMPLES\n";
    t += "  usdstudio document.open Data/Kitchen_set/Kitchen_set.usd\n";
    t += "  usdstudio stage.info\n";
    t += "  usdstudio stage.children /KITCHEN_LIGHTING_MODEL --limit 10 --fields path,typeName\n";
    t += "  usdstudio prim.define /Kitchen_set/Set0/Marker\n";
    t += "  usdstudio --dry-run attr.set /Kitchen_set/Set0/Marker inputs:lightingIntensity float4 \"1,1,1,1\"\n";
    t += "  usdstudio --command \"prim.define /M\" --command \"attr.set /M xray_visibility bool true\" --command \"history.undo\"\n";
    t += "  usdstudio --remote stage.info            (drive the running GUI; see Doc/Agent-CLI-Skill.md)\n";
    return t;
}

Json selfDocResponse(std::string_view cmd, Json data)
{
    Json response = Json::object();
    response.set("ok", true);
    response.setString("cmd", cmd);
    response.set("data", std::move(data));
    return response;
}

// Reading stdin is only meaningful when something is piped in. Without this
// guard an interactive '--commands -' would sit at a prompt, which the "never
// interactive" contract forbids.
bool stdinIsInteractive()
{
#ifdef _MSC_VER
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

// ---------------------------------------------------------------------------
// Remote mode: forward operation lines to a running UsdStudio GUI
//
// The wire protocol is line-based so the client needs no JSON parser: write
// one operation line, read one compact JSON envelope back. The server appends
// an '"exit":<code>' member as the last field of every answer, which the
// client mirrors as its own exit code (last occurrence wins, so data strings
// that mention the key cannot shadow it).
// ---------------------------------------------------------------------------

int extractRemoteExit(const std::string& answer)
{
    static constexpr std::string_view key = "\"exit\":";
    const std::size_t pos = answer.rfind(key);
    if (pos == std::string::npos)
        return ExitInternal; // not an envelope we understand
    try
    {
        return std::stoi(answer.substr(pos + key.size()));
    }
    catch (const std::exception&)
    {
        return ExitInternal;
    }
}

#ifdef _WIN32

class PipeHandle
{
public:
    PipeHandle() = default;
    explicit PipeHandle(HANDLE handle) : handle_(handle) {}
    ~PipeHandle() { if (valid()) CloseHandle(handle_); }
    PipeHandle(const PipeHandle&) = delete;
    PipeHandle& operator=(const PipeHandle&) = delete;
    PipeHandle(PipeHandle&& other) noexcept : handle_(other.handle_) { other.handle_ = INVALID_HANDLE_VALUE; }
    PipeHandle& operator=(PipeHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (valid()) CloseHandle(handle_);
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    bool valid() const { return handle_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return handle_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

bool connectRemotePipe(const std::string& pipeName, PipeHandle& pipe, std::string& message, std::string& hint)
{
    std::wstring wide;
    wide.reserve(pipeName.size());
    for (const char c : pipeName)
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    const std::wstring path = L"\\\\.\\pipe\\" + wide;

    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_PIPE_BUSY)
        {
            message = "The UsdStudio remote pipe '" + pipeName + "' is busy.";
            hint = "Another CLI is talking to that instance. Wait for it to finish, or target another "
                   "instance with --pipe <name> (the GUI status bar shows its pipe name).";
        }
        else
        {
            message = "No running UsdStudio instance is listening for remote control on pipe '" + pipeName + "'.";
            hint = "Start the UsdStudio GUI: remote control is on by default and its status bar shows the pipe "
                   "name. If several GUIs run, pass the name shown there with --pipe <name>.";
        }
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(handle, &mode, nullptr, nullptr);
    pipe = PipeHandle(handle);
    return true;
}

bool writeRemoteLine(const PipeHandle& pipe, const std::string& line)
{
    const std::string data = line + "\n";
    DWORD written = 0;
    return WriteFile(pipe.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) != 0 &&
        written == data.size();
}

bool readRemoteLine(const PipeHandle& pipe, std::string& line)
{
    line.clear();
    char ch = 0;
    DWORD got = 0;
    for (;;)
    {
        if (!ReadFile(pipe.get(), &ch, 1, &got, nullptr) || got == 0)
            return !line.empty();
        if (ch == '\n')
            return true;
        if (ch != '\r')
            line.push_back(ch);
    }
}

#endif // _WIN32

int runRemote(const std::vector<std::string>& operations, const Flags& flags, std::ostream& out, std::ostream& err)
{
    const std::string pipeName = flags.pipe.empty() ? std::string(kRemotePipeName) : flags.pipe;

    auto reportLocalError = [&](const std::string& message, const std::string& hint)
    {
        Outcome o;
        o.name = "remote";
        fail(o, ErrorCode::Precondition, message, hint);
        writeOutcome(out, err, o, flags, false);
        return o.exitCode;
    };

#ifdef _WIN32
    // Global --limit/--fields/--dry-run travel inside the operation line;
    // the server-side evaluator understands them there.
    std::string suffix;
    if (flags.limit != kDefaultRowLimit)
        suffix += " --limit " + std::to_string(flags.limit);
    if (!flags.fields.empty())
    {
        suffix += " --fields ";
        for (std::size_t i = 0; i < flags.fields.size(); ++i)
            suffix += (i ? "," : "") + flags.fields[i];
    }
    if (flags.dryRun)
        suffix += " --dry-run";

    PipeHandle pipe;
    std::string message;
    std::string hint;
    if (!connectRemotePipe(pipeName, pipe, message, hint))
        return reportLocalError(message, hint);

    int exitCode = ExitOk;
    for (const std::string& line : operations)
    {
        if (!writeRemoteLine(pipe, line + suffix))
            return reportLocalError("Connection to the UsdStudio remote pipe was lost while sending.",
                "The GUI may have closed. Restart it and retry.");
        std::string answer;
        if (!readRemoteLine(pipe, answer))
            return reportLocalError("The UsdStudio remote pipe closed before an answer arrived.",
                "The GUI may have closed during the operation. Restart it and check the document state.");
        out << answer << "\n";
        const int code = extractRemoteExit(answer);
        if (code != ExitOk && exitCode == ExitOk)
            exitCode = code;
    }
    return exitCode;
#else
    (void)operations;
    return reportLocalError("Remote control is only implemented with Windows named pipes in this build.",
        "Use the local CLI without --remote, or run the UsdStudio GUI on Windows.");
#endif
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CliSession: the evaluator shared by the CLI loop and the GUI remote server
// ---------------------------------------------------------------------------

CliSession::CliSession(AppContext& context, const CommandRegistry& commands, const QueryRegistry& queries)
    : context_(&context)
    , commands_(&commands)
    , queries_(&queries)
    , allOperationNames_(allOperationNames(commands, queries))
{
}

CliSession::Result CliSession::evaluate(std::string_view line) const
{
    Outcome outcome;
    runOperation(std::string(line), *context_, *commands_, *queries_, Flags{}, allOperationNames_, outcome);

    // Remote clients mirror this exit code without parsing the error object;
    // it is appended last so rfind("\"exit\":") finds our member.
    Json response = responseJson(outcome);
    response.set("exit", static_cast<long long>(outcome.exitCode));

    Result result;
    result.json = response.dump(true);
    result.exitCode = outcome.exitCode;
    return result;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    Document document;
    CommandHistory history;
    AppContext context{ document, history };

    CommandRegistry commands;
    QueryRegistry queries;
    registerAppCommands(commands, queries, context);

    Flags flags;
    std::string error;
    std::string hint;
    if (!parseFlags(args, flags, error, hint))
    {
        writeUsageError(out, err, flags, error, hint);
        return ExitUsage;
    }

    const std::vector<std::string> allNames = allOperationNames(commands, queries);
    const bool pretty = flags.pretty;

    if (flags.version)
    {
        if (!flags.json)
            out << kProgram << ' ' << kVersion << " (OpenUSD " << PXR_VERSION << ")\n";
        else
        {
            Json data = Json::object();
            data.setString("name", kProgram);
            data.setString("version", kVersion);
            data.setString("openUsd", std::to_string(PXR_VERSION));
            out << selfDocResponse("--version", std::move(data)).dump(!pretty) << "\n";
        }
        return ExitOk;
    }

    if (flags.types)
    {
        if (!flags.json)
        {
            for (const std::string& type : supportedValueTypes())
                out << type << '\n';
        }
        else
        {
            Json data = Json::object();
            data.set("types", valueTypesJson());
            out << selfDocResponse("--types", std::move(data)).dump(!pretty) << "\n";
        }
        return ExitOk;
    }

    if (flags.schema)
    {
        // Always JSON: '--schema' exists to be parsed by a machine.
        out << selfDocResponse("--schema", buildSchema(commands, queries)).dump(!pretty) << "\n";
        return ExitOk;
    }

    if (flags.agentHelp)
    {
        out << agentHelpText(commands, queries);
        return ExitOk;
    }

    int exitCode = ExitOk;
    if (flags.help)
    {
        if (flags.helpTopics.empty())
            out << usageText();
        else
        {
            for (const std::string& topic : flags.helpTopics)
            {
                if (!printTopicHelp(out, err, flags, topic, commands, queries, allNames))
                    exitCode = ExitUsage;
            }
        }
        return exitCode;
    }

    std::vector<std::string> operations = flags.batch;
    if (flags.readStdin)
    {
        if (stdinIsInteractive())
        {
            writeUsageError(out, err, flags, "'--commands -' needs piped input.",
                "Pipe the operations, for example: echo stage.info | usdstudio --commands -");
            return ExitUsage;
        }
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (!line.empty())
                operations.push_back(line);
        }
    }
    if (!flags.positional.empty())
        operations.push_back(joinPositional(flags.positional));

    if (operations.empty())
    {
        writeUsageError(out, err, flags, "No operation given.",
            "Pass an operation such as 'stage.info'. Run 'usdstudio --help' for the flags, "
            "'usdstudio --agent-help' for the AI quick start.");
        return ExitUsage;
    }

    const bool batch = operations.size() > 1;
    if (flags.remote)
        return runRemote(operations, flags, out, err);
    for (const std::string& line : operations)
    {
        Outcome outcome;
        runOperation(line, context, commands, queries, flags, allNames, outcome);
        if (outcome.exitCode != ExitOk && exitCode == ExitOk)
            exitCode = outcome.exitCode;
        writeOutcome(out, err, outcome, flags, batch);
    }
    return exitCode;
}

int run(int argc, char* const* argv, std::ostream& out, std::ostream& err)
{
    std::vector<std::string> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i])
            args.emplace_back(argv[i]);
    }
    return run(args, out, err);
}

} // namespace UsdStudio::Cli
