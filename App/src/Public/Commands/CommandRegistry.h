// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "ICommand.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/usd/sdf/path.h"

#include <cctype>
#include <charconv>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// Converts one command-line token into a C++ argument type.
// Extend this function to support new command parameter types; commands
// registered in CommandRegistry then parse them automatically.
template <typename T>
T parseArg(std::string_view token)
{
    if constexpr (std::is_same_v<T, std::string_view>)
    {
        return token;
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return std::string(token);
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        return token == "true" || token == "1";
    }
    else if constexpr (std::is_same_v<T, pxr::SdfPath>)
    {
        return pxr::SdfPath(std::string(token));
    }
    else if constexpr (std::is_same_v<T, pxr::GfVec3d>)
    {
        // Format: "x,y,z"
        pxr::GfVec3d value;
        std::size_t pos = 0;
        for (std::size_t i = 0; i < 3; ++i)
        {
            const std::size_t comma = token.find(',', pos);
            const std::string_view part = (comma == std::string_view::npos)
                ? token.substr(pos)
                : token.substr(pos, comma - pos);
            value[i] = parseArg<double>(part);
            pos = (comma == std::string_view::npos) ? token.size() : comma + 1;
        }
        return value;
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
        T value{};
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec != std::errc() || ptr != token.data() + token.size())
            throw std::invalid_argument("Invalid number: " + std::string(token));
        return value;
    }
    else
    {
        static_assert(sizeof(T) == 0, "No parseArg defined for this command argument type");
    }
}

// Splits "command.name arg1 arg2" into tokens for headless invocation
// (tests, scripts, future CLI/AI). Whitespace separated only; quoted
// arguments with spaces are not supported yet.
class CommandLine
{
public:
    explicit CommandLine(std::string_view line)
    {
        std::size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
                ++i;
            const std::size_t start = i;
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
                ++i;
            if (start < i)
                owned_.emplace_back(line.substr(start, i - start));
        }
        for (const auto& part : owned_)
            tokens_.push_back(part);
    }

    std::string_view name() const { return tokens_.empty() ? std::string_view{} : tokens_.front(); }

    std::span<const std::string_view> args() const
    {
        return tokens_.empty() ? std::span<const std::string_view>{}
                               : std::span<const std::string_view>(tokens_).subspan(1);
    }

private:
    std::vector<std::string> owned_;
    std::vector<std::string_view> tokens_; // views into owned_
};

// Central registry mapping command names ("document.open") to factories
// that build a command from string tokens with compile-time typed parsing.
class CommandRegistry
{
public:
    using ArgSpan = std::span<const std::string_view>;
    using FactoryFn = std::function<std::unique_ptr<ICommand>(ArgSpan)>;

    struct CommandInfo
    {
        std::string name;
        std::string help;
        std::size_t argCount = 0;
        FactoryFn factory;
    };

    // Registers CmdType whose constructor takes (Args...), parsed from the
    // tokens left to right.
    template <typename CmdType, typename... Args>
    void registerCommand(std::string name, std::string help)
    {
        registerImpl<CmdType, NoContext, Args...>(std::move(name), std::move(help), nullptr);
    }

    // Same, but binds a context object (e.g. the Document the app edits) as
    // hidden first constructor argument: CmdType(*context, Args...).
    template <typename CmdType, typename Ctx, typename... Args>
    void registerCommand(std::string name, std::string help, Ctx* context)
    {
        registerImpl<CmdType, Ctx, Args...>(std::move(name), std::move(help), context);
    }

    // Builds (does not execute) the command for 'name'. Throws
    // std::runtime_error for unknown names and std::invalid_argument for
    // wrong argument counts or unparsable arguments.
    std::unique_ptr<ICommand> create(std::string_view name, ArgSpan args) const;

    // Convenience overload for whole "name arg1 arg2" lines.
    std::unique_ptr<ICommand> create(const CommandLine& line) const
    {
        return create(line.name(), line.args());
    }

    // Self-documenting command listing; suitable for "app --help" output
    // or as prompt context for an AI frontend.
    std::string helpText() const;

    std::vector<std::string> commandNames() const;

    const CommandInfo* find(std::string_view name) const;

private:
    struct NoContext;

    template <typename CmdType, typename Ctx, typename... Args>
    void registerImpl(std::string name, std::string help, Ctx* context)
    {
        // Note: 'key' is copied before the call so the map key and the
        // lambda's copy of the name are independent regardless of argument
        // evaluation order.
        std::string key = name;
        registerFactory(std::move(key), std::move(help), sizeof...(Args),
            [context, cmdName = std::move(name)](ArgSpan args) -> std::unique_ptr<ICommand>
            {
                if (args.size() != sizeof...(Args))
                    throw std::invalid_argument("Wrong number of arguments for command: " + cmdName);
                return construct<CmdType, Ctx, Args...>(context, args, std::index_sequence_for<Args...>{});
            });
    }

    template <typename CmdType, typename Ctx, typename... Args, std::size_t... Is>
    static std::unique_ptr<ICommand> construct(Ctx* context, ArgSpan args, std::index_sequence<Is...>)
    {
        if constexpr (std::is_same_v<Ctx, NoContext>)
            return std::make_unique<CmdType>(parseArg<Args>(args[Is])...);
        else
            return std::make_unique<CmdType>(*context, parseArg<Args>(args[Is])...);
    }

    void registerFactory(std::string name, std::string help, std::size_t argCount, FactoryFn factory);

    std::unordered_map<std::string, CommandInfo> commands_;
};
