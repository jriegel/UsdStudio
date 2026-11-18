// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "Json.h"

#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/valueTypeName.h"

#include <string>
#include <string_view>
#include <vector>

// Value conversion between command-line text and USD values.
//
// The prebuilt OpenUSD here has no VtValue::FromString, so the CLI parses the
// text itself against an SdfValueTypeName. That also gives precise, actionable
// errors ("expected 3 numbers for point3f") instead of a silent fallback.
namespace UsdStudio::Cli
{

// Parses 'text' as the USD value type 'typeName' ("float", "point3f",
// "token", "point3f[]", ...). Throws CommandError when the type is unknown,
// not supported, or the text does not fit it.
//
// Text formats: scalars use their literal form ("3.5", "true", "MyToken");
// tuples accept "(1, 2, 3)" or "1,2,3"; matrices accept nested parens or a
// flat comma list; arrays accept "1, 2, 3" or "[1, 2, 3]" with parenthesised
// elements for tuple arrays. Surrounding double quotes are stripped.
pxr::VtValue parseTypedValue(std::string_view typeName, std::string_view text);

// Resolves a type name through the Sdf schema. Returns an empty type name
// when the name is unknown.
pxr::SdfValueTypeName findValueType(std::string_view typeName);

// JSON for an attribute value: numbers stay numbers, tuples/matrices become
// arrays, arrays become arrays. Types the CLI cannot introspect degrade to
// { "typeName": ..., "value": null, "note": ... } so output stays valid JSON.
Json vtValueToJson(const pxr::VtValue& value);

// Type names the CLI can parse and print, for '--schema', '--types' and the
// hints in error messages.
std::vector<std::string> supportedValueTypes();

} // namespace UsdStudio::Cli
