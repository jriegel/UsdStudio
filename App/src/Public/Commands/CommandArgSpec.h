// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "pxr/base/gf/vec3d.h"
#include "pxr/usd/sdf/path.h"

// Self-description support for the command and query registries.
//
// An agent must be able to ask "which commands exist and what do they take?"
// instead of reading C++, so registries record one ArgSpec per parameter: the
// value type name (derived from parseArg<T>) and the parameter name (taken
// from the command's optional static argNames(), else positional).
struct ArgSpec
{
    std::string_view name;
    std::string_view type;
    bool optional = false;
};

// Human-readable name of a parseable argument type. Must stay in sync with
// the parseArg<T> overloads in CommandRegistry.h.
template <typename T>
consteval std::string_view argTypeName()
{
    using Base = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<Base, std::string>)            return "string";
    else if constexpr (std::is_same_v<Base, std::string_view>)  return "string";
    else if constexpr (std::is_same_v<Base, bool>)              return "bool";
    else if constexpr (std::is_same_v<Base, pxr::SdfPath>)      return "path";
    else if constexpr (std::is_same_v<Base, pxr::GfVec3d>)      return "x,y,z";
    else if constexpr (std::is_floating_point_v<Base>)          return "number";
    else if constexpr (std::is_integral_v<Base>)                return "int";
    else                                                         return "value";
}

template <typename... Args>
inline std::vector<std::string_view> argTypeNames()
{
    return { argTypeName<Args>()... };
}

// Commands may expose parameter names with
// `static std::array<std::string_view, N> argNames()`. Without it the CLI
// falls back to positional names (arg1, arg2, ...).
template <typename CmdType>
concept HasArgNames = requires { CmdType::argNames(); };

// Commands that cannot be undone (file I/O, ...) declare `static constexpr
// bool undoable = false;` so the schema can say so before running anything.
// Such commands must also override ICommand::isUndoable() to return false.
template <typename CmdType>
concept HasUndoableTrait = requires { CmdType::undoable; };

template <typename CmdType>
consteval bool commandUndoable()
{
    if constexpr (HasUndoableTrait<CmdType>)
        return static_cast<bool>(CmdType::undoable);
    else
        return true;
}

// Parameter specs for a signature: 'names' wins where non-empty, otherwise a
// positional fallback. 'types' comes from argTypeNames<Args...>(). Trailing
// arguments beyond (types.size() - optionalTrailing) may be omitted.
inline std::vector<ArgSpec> makeArgSpecs(std::vector<std::string_view> names,
    const std::vector<std::string_view>& types, std::size_t optionalTrailing = 0)
{
    static constexpr std::array<std::string_view, 16> positional = {
        "arg1", "arg2", "arg3", "arg4", "arg5", "arg6", "arg7", "arg8",
        "arg9", "arg10", "arg11", "arg12", "arg13", "arg14", "arg15", "arg16" };

    std::vector<ArgSpec> specs;
    specs.reserve(types.size());
    const std::size_t required = types.size() > optionalTrailing ? types.size() - optionalTrailing : 0;
    for (std::size_t i = 0; i < types.size(); ++i)
    {
        const bool hasName = i < names.size() && !names[i].empty();
        specs.push_back(ArgSpec{
            hasName ? names[i] : positional[i < positional.size() ? i : positional.size() - 1],
            types[i], i >= required });
    }
    return specs;
}

// "document.open <fileName>" or "prim.define <path> [typeName]" - used in
// --help, --schema and error messages.
inline std::string makeUsage(const std::string& name, const std::vector<ArgSpec>& specs)
{
    std::string usage = name;
    for (const ArgSpec& spec : specs)
    {
        usage += spec.optional ? " [" : " <";
        usage += spec.name;
        usage += spec.optional ? "]" : ">";
    }
    return usage;
}
