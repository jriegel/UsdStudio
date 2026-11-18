// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/QueryRegistry.h"

#include "../../Public/Commands/CommandError.h"

#include <algorithm>
#include <stdexcept>

void QueryRegistry::registerQuery(std::string name, std::string help, std::string returns,
    std::vector<ArgSpec> args, QueryFn fn)
{
    const auto [it, inserted] = queries_.try_emplace(name);
    if (!inserted)
        throw std::logic_error("Query registered twice: " + it->first);

    it->second.name = std::move(name);
    it->second.help = std::move(help);
    it->second.returns = std::move(returns);
    it->second.args = std::move(args);
    it->second.usage = makeUsage(it->second.name, it->second.args);
    it->second.fn = std::move(fn);
}

QueryRegistry::Json QueryRegistry::run(std::string_view name, ArgSpan args, AppContext& context) const
{
    const QueryInfo* info = find(name);
    if (!info)
        throw std::runtime_error("Unknown query: " + std::string(name));

    std::size_t required = 0;
    for (const ArgSpec& spec : info->args)
        if (!spec.optional)
            ++required;

    if (args.size() < required || args.size() > info->args.size())
        throw CommandError::invalidValue("Wrong number of arguments for " + info->usage,
            "Expected " + std::to_string(required) + " to " + std::to_string(info->args.size()) +
            " argument(s): " + info->usage);

    // Omitted optional arguments arrive as an empty token, matching how the
    // command registry feeds constructors.
    std::vector<std::string_view> padded(args.begin(), args.end());
    while (padded.size() < info->args.size())
        padded.emplace_back();

    return info->fn(context, std::span<const std::string_view>(padded));
}

std::string QueryRegistry::helpText() const
{
    std::string out = "Available queries:\n";
    std::vector<std::string> names;
    names.reserve(queries_.size());
    for (const auto& [name, info] : queries_)
        names.push_back(name);
    std::sort(names.begin(), names.end());

    for (const std::string& name : names)
    {
        const QueryInfo& info = queries_.at(name);
        out += "  " + info.usage;
        if (!info.help.empty())
            out += "\n      " + info.help;
        if (!info.returns.empty())
            out += "  [returns: " + info.returns + "]";
        out += "\n";
    }
    return out;
}

std::vector<std::string> QueryRegistry::queryNames() const
{
    std::vector<std::string> names;
    names.reserve(queries_.size());
    for (const auto& [name, info] : queries_)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

const QueryRegistry::QueryInfo* QueryRegistry::find(std::string_view name) const
{
    const auto it = queries_.find(std::string(name));
    return it == queries_.end() ? nullptr : &it->second;
}
