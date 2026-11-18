// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "CommandArgSpec.h"
#include "CommandRegistry.h"

#include "../Cli/Json.h"

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct AppContext;

// Read-only counterpart of CommandRegistry: a query takes string tokens and
// returns JSON instead of an undoable command. Keeping the two registries
// apart means the schema never has to pretend that a lookup can be undone.
//
// Queries report problems with CommandError so a frontend can map them onto
// its own exit codes.
class QueryRegistry
{
public:
    using ArgSpan = std::span<const std::string_view>;
    using Json = UsdStudio::Cli::Json;
    using QueryFn = std::function<Json(AppContext&, ArgSpan)>;

    struct QueryInfo
    {
        std::string name;
        std::string help;
        std::string returns;         // what the JSON holds, for --schema
        std::vector<ArgSpec> args;   // names + value types
        std::string usage;           // "stage.children <path>"
        QueryFn fn;
    };

    void registerQuery(std::string name, std::string help, std::string returns,
        std::vector<ArgSpec> args, QueryFn fn);

    // Runs the query and returns its JSON. Throws std::runtime_error for
    // unknown names and CommandError for bad arguments or app state.
    Json run(std::string_view name, ArgSpan args, AppContext& context) const;

    Json run(const CommandLine& line, AppContext& context) const
    {
        return run(line.name(), line.args(), context);
    }

    // Compact listing for "--help". The machine-readable schema is built by
    // the CLI front-end from find()/queryNames() so this registry stays free
    // of output-format code.
    std::string helpText() const;

    std::vector<std::string> queryNames() const;

    const QueryInfo* find(std::string_view name) const;

private:
    std::unordered_map<std::string, QueryInfo> queries_;
};

// Helpers for declaring argument specs at registration.
inline ArgSpec requiredArg(std::string_view name, std::string_view type)
{
    return ArgSpec{ name, type, false };
}

inline ArgSpec optionalArg(std::string_view name, std::string_view type)
{
    return ArgSpec{ name, type, true };
}
