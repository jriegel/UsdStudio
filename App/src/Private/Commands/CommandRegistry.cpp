// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/CommandRegistry.h"

#include <algorithm>
#include <sstream>

void CommandRegistry::registerFactory(std::string name, std::string help, std::size_t argCount,
    std::size_t requiredCount, const std::vector<ArgSpec>& args, bool undoable, std::string usage, FactoryFn factory)
{
    // Fill the entry after insertion: building CommandInfo while also using
    // 'name' as the key would depend on argument evaluation order.
    const auto [it, inserted] = commands_.try_emplace(name);
    if (!inserted)
        throw std::logic_error("Command registered twice: " + it->first);

    it->second.name = std::move(name);
    it->second.help = std::move(help);
    it->second.argCount = argCount;
    it->second.requiredCount = requiredCount;
    it->second.args = args;
    it->second.undoable = undoable;
    it->second.usage = std::move(usage);
    it->second.factory = std::move(factory);
}

std::unique_ptr<ICommand> CommandRegistry::create(std::string_view name, ArgSpan args) const
{
    const auto it = commands_.find(std::string(name));
    if (it == commands_.end())
        throw std::runtime_error("Unknown command: " + std::string(name));
    return it->second.factory(args);
}

const CommandRegistry::CommandInfo* CommandRegistry::find(std::string_view name) const
{
    const auto it = commands_.find(std::string(name));
    return (it == commands_.end()) ? nullptr : &it->second;
}

std::vector<std::string> CommandRegistry::commandNames() const
{
    std::vector<std::string> names;
    names.reserve(commands_.size());
    for (const auto& [name, info] : commands_)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

std::string CommandRegistry::helpText() const
{
    std::ostringstream out;
    out << "Available commands:\n";
    for (const auto& name : commandNames())
    {
        const CommandInfo& info = commands_.at(name);
        out << "  " << info.usage;
        if (!info.undoable)
            out << "  [not undoable]";
        out << " - " << info.help << "\n";
    }
    return out.str();
}
