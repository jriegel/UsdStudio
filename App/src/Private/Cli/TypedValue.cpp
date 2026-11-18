// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#include "../../Public/Cli/TypedValue.h"

#include "../../Public/Commands/CommandError.h"

#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec2d.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec2h.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/gf/vec4h.h"
#include "pxr/base/gf/matrix2d.h"
#include "pxr/base/gf/matrix3d.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/quath.h"
#include "pxr/base/gf/timecode.h"
#include "pxr/base/gf/half.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace UsdStudio::Cli
{

namespace
{

// Scalar element kind. Tuple types (float2f ...) are the same kind with a
// width, arrays the same kind flagged as array - that keeps the switches short
// instead of enumerating every USD value type separately.
enum class Kind
{
    Bool, UChar, Int, UInt, Int64, UInt64, Half, Float, Double,
    TimeCode, String, Token, Asset, Quat, Matrix, Unsupported
};

// Type-name prefixes, longest first where they overlap (int64 before int).
constexpr struct { const char* prefix; Kind kind; } kPrefixes[] = {
    {"int64",    Kind::Int64},
    {"uint64",   Kind::UInt64},
    {"uchar",    Kind::UChar},
    {"uint",     Kind::UInt},
    {"int",      Kind::Int},
    {"bool",     Kind::Bool},
    {"half",     Kind::Half},
    {"float",    Kind::Float},
    {"double",   Kind::Double},
    {"timecode", Kind::TimeCode},
    {"string",   Kind::String},
    {"token",    Kind::Token},
    {"asset",    Kind::Asset},
    // Vector-valued types; the f/d/h suffix below picks the element type.
    {"normal",   Kind::Float},
    {"point",    Kind::Float},
    {"vector",   Kind::Float},
    {"color",    Kind::Float},
    {"texCoord", Kind::Float},
};

// Width encoded in the name ("float2f" -> 2) and element kind from the
// trailing f/d/h ("point3d" -> double).
std::size_t tupleWidthOf(const std::string& name, Kind& kind)
{
    for (const auto& entry : kPrefixes) {
        const std::string_view prefix(entry.prefix);
        if (name.compare(0, prefix.size(), prefix) != 0)
            continue;
        kind = entry.kind;
        std::size_t width = 1;
        std::size_t i = prefix.size();
        if (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i]))) {
            width = static_cast<std::size_t>(name[i] - '0');
            ++i;
        }
        if (i < name.size()) {
            switch (name[i]) {
            case 'f': kind = Kind::Float;  break;
            case 'd': kind = Kind::Double; break;
            case 'h': kind = Kind::Half;   break;
            default: break;
            }
        }
        return width;
    }
    return 1;
}

struct Resolved
{
    std::string typeName;      // element type name, without the Array suffix
    Kind kind = Kind::Unsupported;
    std::size_t width = 1;     // number of scalar elements (1 for scalars)
    std::size_t matrixDim = 0; // 2/3/4 for matrix types
    bool isArray = false;
    bool valid = false;
};

Resolved resolve(const SdfValueTypeName& type)
{
    Resolved r;
    if (!type)
        return r;

    r.isArray = type.IsArray();
    const SdfValueTypeName elem = r.isArray ? type.GetScalarType() : type;
    r.typeName = elem.GetAsToken().GetString();

    if (r.typeName.compare(0, 4, "quat") == 0) {
        r.kind = Kind::Quat;
    } else if (r.typeName == "frame4d") {
        r.kind = Kind::Matrix;      // frame4d is stored as a GfMatrix4d
        r.matrixDim = 4;
    } else if (r.typeName.compare(0, 6, "matrix") == 0) {
        r.kind = Kind::Matrix;
        if (r.typeName.size() > 6 && std::isdigit(static_cast<unsigned char>(r.typeName[6])))
            r.matrixDim = static_cast<std::size_t>(r.typeName[6] - '0');
        else
            r.matrixDim = 4;
    } else {
        r.width = tupleWidthOf(r.typeName, r.kind);
    }

    r.valid = r.kind != Kind::Unsupported;
    return r;
}

// ---------------------------------------------------------------- parsing ---

// The message says what was expected, the hint echoes the offending text, so
// a frontend can show both without string surgery.
CommandError badValue(std::string_view text, std::string_view expectation)
{
    return CommandError::invalidValue(std::string(expectation),
                                      "got '" + std::string(text) + "'");
}

double toDouble(std::string_view s)
{
    const std::string v(s);
    double out = 0.0;
    const auto* first = v.data();
    const auto* last = first + v.size();
    const auto res = std::from_chars(first, last, out);
    if (res.ec != std::errc() || res.ptr != last || !std::isfinite(out))
        throw badValue(v, "expected a number");
    return out;
}

long long toLongLong(std::string_view s)
{
    const std::string v(s);
    long long out = 0;
    const auto* first = v.data();
    const auto* last = first + v.size();
    const auto res = std::from_chars(first, last, out);
    if (res.ec != std::errc() || res.ptr != last)
        throw badValue(v, "expected a whole number");
    return out;
}

std::uint64_t toUnsigned(std::string_view s)
{
    const std::string v(s);
    unsigned long long out = 0;
    const auto* first = v.data();
    const auto* last = first + v.size();
    const auto res = std::from_chars(first, last, out);
    if (res.ec != std::errc() || res.ptr != last)
        throw badValue(v, "expected a non-negative whole number");
    return static_cast<std::uint64_t>(out);
}

bool toBool(std::string_view s)
{
    std::string v(s);
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (v == "true" || v == "1" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "no") return false;
    throw badValue(std::string(s), "expected true or false");
}

std::string trimAndUnquote(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                          (s.front() == '\'' && s.back() == '\'')))
        s = s.substr(1, s.size() - 2);
    return std::string(s);
}

// Splits on top-level separators only, so a parenthesised element stays one
// item: "(1, 2), (3, 4)" -> ["1, 2", "3, 4"].
std::vector<std::string> splitTopLevel(std::string_view s)
{
    std::vector<std::string> out;
    int depth = 0;
    bool inQuotes = false;
    std::string cur;
    for (char c : s) {
        if (c == '"' || c == '\'')
            inQuotes = !inQuotes;
        if (!inQuotes) {
            if (c == '(' || c == '[' || c == '{') ++depth;
            if (c == ')' || c == ']' || c == '}') --depth;
            if (c == ',' && depth <= 0) {
                out.push_back(trimAndUnquote(cur));
                cur.clear();
                continue;
            }
        }
        cur.push_back(c);
    }
    out.push_back(trimAndUnquote(cur));
    return out;
}

// Splits into scalar tokens, dropping nesting: "(1, (2, 3))" -> [1, 2, 3].
std::vector<std::string> splitFlat(std::string_view s)
{
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    auto flush = [&]() {
        std::string t = trimAndUnquote(cur);
        if (!t.empty())
            out.push_back(std::move(t));
        cur.clear();
    };
    for (char c : s) {
        if (c == '"' || c == '\'') {
            inQuotes = !inQuotes;
            cur.push_back(c);
            continue;
        }
        if (!inQuotes && (c == '(' || c == '[' || c == '{' ||
                          c == ')' || c == ']' || c == '}' || c == ',')) {
            flush();
            continue;
        }
        cur.push_back(c);
    }
    flush();
    return out;
}

template <class T>
std::vector<T> parseFlatList(std::string_view text)
{
    const auto parts = splitFlat(text);
    std::vector<T> out;
    out.reserve(parts.size());
    for (const auto& p : parts) {
        if constexpr (std::is_same_v<T, bool>) {
            out.push_back(toBool(p));
        } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float> ||
                             std::is_same_v<T, GfHalf>) {
            out.push_back(static_cast<T>(toDouble(p)));
        } else if constexpr (std::is_same_v<T, TfToken>) {
            out.emplace_back(p);
        } else if constexpr (std::is_same_v<T, SdfPath>) {
            out.emplace_back(p);
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>)
                out.push_back(static_cast<T>(toUnsigned(p)));
            else
                out.push_back(static_cast<T>(toLongLong(p)));
        } else {
            out.push_back(p);   // std::string
        }
    }
    return out;
}

// Tuple: "1,2,3" or "(1, 2, 3)".
template <class V>
V parseTuple(std::string_view text, std::size_t width, const std::string& typeName)
{
    auto v = parseFlatList<typename V::ScalarType>(text);
    if (v.size() != width)
        throw badValue(std::string(text),
            "expected " + std::to_string(width) + " components for " + typeName);
    return V(v.data());
}

// Quaternion: "w,x,y,z" (real first, as in the USD text format).
template <class Q>
Q parseQuat(std::string_view text)
{
    const auto parts = splitFlat(text);
    if (parts.size() != 4)
        throw badValue(std::string(text),
            "expected 4 numbers (real, i, j, k) for a quaternion");
    const auto w = static_cast<typename Q::ScalarType>(toDouble(parts[0]));
    const auto x = static_cast<typename Q::ScalarType>(toDouble(parts[1]));
    const auto y = static_cast<typename Q::ScalarType>(toDouble(parts[2]));
    const auto z = static_cast<typename Q::ScalarType>(toDouble(parts[3]));
    return Q(w, x, y, z);
}

// Matrix: row-major numbers, 4/9/16 of them. USD's {{ }} groups collapse into
// the same flat list.
template <class M>
M parseMatrix(std::string_view text, std::size_t dim)
{
    const auto v = parseFlatList<double>(text);
    const std::size_t n = dim * dim;
    if (v.size() != n)
        throw badValue(std::string(text),
            "expected " + std::to_string(n) + " numbers (row-major) for a matrix");
    M m;
    double* data = m.GetArray();
    for (std::size_t i = 0; i < n; ++i)
        data[i] = v[i];
    return m;
}

VtValue parseScalar(const Resolved& r, std::string_view text)
{
    const std::string v = trimAndUnquote(text);
    switch (r.kind) {
    case Kind::Bool:     return VtValue(toBool(v));
    case Kind::UChar:    return VtValue(static_cast<unsigned char>(toUnsigned(v)));
    case Kind::Int:      return VtValue(static_cast<int>(toLongLong(v)));
    case Kind::UInt:     return VtValue(static_cast<unsigned int>(toUnsigned(v)));
    case Kind::Int64:    return VtValue(static_cast<std::int64_t>(toLongLong(v)));
    case Kind::UInt64:   return VtValue(static_cast<std::uint64_t>(toUnsigned(v)));
    case Kind::Half:     return VtValue(static_cast<GfHalf>(toDouble(v)));
    case Kind::Float:    return VtValue(static_cast<float>(toDouble(v)));
    case Kind::Double:   return VtValue(toDouble(v));
    case Kind::TimeCode: return VtValue(GfTimeCode(toDouble(v)));
    case Kind::String:   return VtValue(v);
    case Kind::Token:    return VtValue(TfToken(v));
    case Kind::Asset:    return VtValue(SdfAssetPath(v));
    case Kind::Quat:
        if (r.typeName.compare(0, 5, "quatd") == 0) return VtValue(parseQuat<GfQuatd>(text));
        if (r.typeName.compare(0, 5, "quath") == 0) return VtValue(parseQuat<GfQuath>(text));
        return VtValue(parseQuat<GfQuatf>(text));
    case Kind::Matrix:
        if (r.matrixDim == 2) return VtValue(parseMatrix<GfMatrix2d>(text, 2));
        if (r.matrixDim == 3) return VtValue(parseMatrix<GfMatrix3d>(text, 3));
        return VtValue(parseMatrix<GfMatrix4d>(text, 4));
    case Kind::Unsupported: break;
    }
    throw CommandError::internal("unhandled value kind");
}

VtValue parseTupleValue(const Resolved& r, std::string_view text)
{
    switch (r.kind) {
    case Kind::Int:
        if (r.width == 2) return VtValue(parseTuple<GfVec2i>(text, 2, r.typeName));
        if (r.width == 3) return VtValue(parseTuple<GfVec3i>(text, 3, r.typeName));
        return VtValue(parseTuple<GfVec4i>(text, 4, r.typeName));
    case Kind::Half:
        if (r.width == 2) return VtValue(parseTuple<GfVec2h>(text, 2, r.typeName));
        if (r.width == 3) return VtValue(parseTuple<GfVec3h>(text, 3, r.typeName));
        return VtValue(parseTuple<GfVec4h>(text, 4, r.typeName));
    case Kind::Double:
        if (r.width == 2) return VtValue(parseTuple<GfVec2d>(text, 2, r.typeName));
        if (r.width == 3) return VtValue(parseTuple<GfVec3d>(text, 3, r.typeName));
        return VtValue(parseTuple<GfVec4d>(text, 4, r.typeName));
    case Kind::Float:
    default:
        if (r.width == 2) return VtValue(parseTuple<GfVec2f>(text, 2, r.typeName));
        if (r.width == 3) return VtValue(parseTuple<GfVec3f>(text, 3, r.typeName));
        return VtValue(parseTuple<GfVec4f>(text, 4, r.typeName));
    }
}

// USD attributes and metadata store arrays as VtArray<T>; a plain
// std::vector<T> in a VtValue is rejected by UsdAttribute::Set.
template <class T>
VtValue toVtArray(std::vector<T>&& v)
{
    VtArray<T> arr;
    arr.resize(v.size());
    std::move(v.begin(), v.end(), arr.begin());
    return VtValue(std::move(arr));
}

// Array of tuples: every top-level item is one tuple.
template <class V>
VtValue parseTupleArray(const Resolved& r, std::string_view body,
                        const std::vector<std::string>& items)
{
    std::vector<V> out;
    out.reserve(items.size());
    for (const auto& item : items)
        out.push_back(parseTuple<V>(item, r.width, r.typeName));
    return toVtArray(std::move(out));
}

VtValue parseArrayValue(const Resolved& r, std::string_view text)
{
    std::string body = trimAndUnquote(text);
    if (!body.empty() && body.front() == '[' && body.back() == ']')
        body = body.substr(1, body.size() - 2);

    std::vector<std::string> items;
    if (!trimAndUnquote(body).empty())
        items = splitTopLevel(body);

    if (r.width > 1) {
        switch (r.kind) {
        case Kind::Float:
            if (r.width == 2) return parseTupleArray<GfVec2f>(r, body, items);
            if (r.width == 3) return parseTupleArray<GfVec3f>(r, body, items);
            return parseTupleArray<GfVec4f>(r, body, items);
        case Kind::Double:
            if (r.width == 2) return parseTupleArray<GfVec2d>(r, body, items);
            if (r.width == 3) return parseTupleArray<GfVec3d>(r, body, items);
            return parseTupleArray<GfVec4d>(r, body, items);
        case Kind::Int:
            if (r.width == 2) return parseTupleArray<GfVec2i>(r, body, items);
            if (r.width == 3) return parseTupleArray<GfVec3i>(r, body, items);
            return parseTupleArray<GfVec4i>(r, body, items);
        default:
            throw badValue(std::string(text),
                "arrays of '" + r.typeName + "' are not supported by the CLI parser");
        }
    }

    switch (r.kind) {
    case Kind::Bool:    return toVtArray(parseFlatList<bool>(body));
    case Kind::Int:     return toVtArray(parseFlatList<int>(body));
    case Kind::UInt:    return toVtArray(parseFlatList<unsigned int>(body));
    case Kind::Int64:   return toVtArray(parseFlatList<std::int64_t>(body));
    case Kind::UInt64:  return toVtArray(parseFlatList<std::uint64_t>(body));
    case Kind::UChar:   return toVtArray(parseFlatList<unsigned char>(body));
    case Kind::Float:   return toVtArray(parseFlatList<float>(body));
    case Kind::Double:  return toVtArray(parseFlatList<double>(body));
    case Kind::String:  return toVtArray(parseFlatList<std::string>(body));
    case Kind::Token:   return toVtArray(parseFlatList<TfToken>(body));
    case Kind::Asset: {
        std::vector<SdfAssetPath> paths;
        for (const std::string& s : parseFlatList<std::string>(body))
            paths.emplace_back(s);
        return toVtArray(std::move(paths));
    }
    case Kind::TimeCode: {
        std::vector<GfTimeCode> out;
        for (const double v : parseFlatList<double>(body))
            out.emplace_back(v);
        return toVtArray(std::move(out));
    }
    default:
        throw badValue(std::string(text),
            "arrays of '" + r.typeName + "' are not supported by the CLI parser");
    }
}

// ----------------------------------------------------------------- output ---

template <class T>
Json scalarToJson(Kind kind, const T& v)
{
    static_cast<void>(kind);   // the C++ type alone decides how to serialise
    if constexpr (std::is_same_v<T, bool>)
        return Json(v);
    else if constexpr (std::is_same_v<T, TfToken>)
        return Json(std::string(v.GetString()));
    else if constexpr (std::is_same_v<T, std::string>)
        return Json(v);
    else if constexpr (std::is_same_v<T, SdfPath>)
        return Json(std::string(v.GetAsString()));
    else if constexpr (std::is_same_v<T, SdfAssetPath>)
        return Json(std::string(v.GetAssetPath()));
    else if constexpr (std::is_same_v<T, GfTimeCode>)
        return Json(v.GetValue());
    else if constexpr (std::is_same_v<T, unsigned char>)
        return Json(static_cast<int>(v));
    else if constexpr (std::is_integral_v<T>)
        return Json(static_cast<long long>(v));
    else
        return Json(static_cast<double>(v));
}

// Flat array of components; GfVec* keep them contiguous.
template <class V>
Json tupleToJson(const V& v)
{
    Json out{ std::vector<Json>() };
    for (std::size_t i = 0; i < V::dimension; ++i)
        out.pushBack(Json(static_cast<double>(v.data()[i])));
    return out;
}

// Gf tuple types expose 'dimension' + 'data()'; scalars (including GfHalf)
// do not. Lets arrayToJson pick the element writer without instantiating
// tupleToJson for scalars.
template <class T>
concept HasTupleLayout = requires (const T& v) { T::dimension; v.data(); };

template <class Q>
Json quatToJson(const Q& q)
{
    Json out{ std::vector<Json>() };
    out.pushBack(Json(static_cast<double>(q.GetReal())));
    const auto& im = q.GetImaginary();
    for (std::size_t i = 0; i < GfVec3f::dimension; ++i)
        out.pushBack(Json(static_cast<double>(im.data()[i])));
    return out;
}

template <class M>
Json matrixToJson(const M& m, std::size_t dim)
{
    Json out{ std::vector<Json>() };
    const double* data = m.GetArray();
    for (std::size_t i = 0; i < dim * dim; ++i)
        out.pushBack(Json(data[i]));
    return out;
}

Json arrayToJson(const VtValue& value, const Resolved& r)
{
    Json out{ std::vector<Json>() };   // an empty array still serialises as []
    const auto append = [&](const auto& vec) {
        for (const auto& e : vec) {
            if constexpr (HasTupleLayout<std::remove_cvref_t<decltype(e)>>)
                out.pushBack(tupleToJson(e));
            else
                out.pushBack(scalarToJson(r.kind, e));
        }
    };
    // Picks the array the value actually holds; returns false if it holds
    // something else (a type alias or a legacy mapping). USD-authored values
    // hold VtArray<T>, values set through this parser hold VtArray<T> too,
    // but stage metadata can still arrive as plain std::vector<T>.
    const auto tryVector = [&](auto tag) -> bool {
        using T = decltype(tag);
        using Vec = std::vector<T>;
        using Arr = VtArray<T>;
        // 'template' keeps MSVC from parsing the dependent '<' as less-than.
        if (value.template IsHolding<Arr>()) {
            append(value.Get<Arr>());
            return true;
        }
        if (value.template IsHolding<Vec>()) {
            append(value.Get<Vec>());
            return true;
        }
        return false;
    };

    switch (r.kind) {
    case Kind::Bool:    tryVector(bool{}); break;
    case Kind::Int:     tryVector(int{}); break;
    case Kind::UInt:    tryVector(unsigned int{}); break;
    case Kind::Int64:   tryVector(std::int64_t{}); break;
    case Kind::UInt64:  tryVector(std::uint64_t{}); break;
    case Kind::UChar:   tryVector(unsigned char{}); break;
    case Kind::Float:
        if (r.width == 1) { if (!tryVector(float{}))   tryVector(GfHalf{}); }
        else if (r.width == 2) { if (!tryVector(GfVec2f{})) tryVector(GfVec2h{}); }
        else if (r.width == 3) { if (!tryVector(GfVec3f{})) tryVector(GfVec3h{}); }
        else                   { if (!tryVector(GfVec4f{})) tryVector(GfVec4h{}); }
        break;
    case Kind::Double:
        if (r.width == 1) tryVector(double{});
        else if (r.width == 2) tryVector(GfVec2d{});
        else if (r.width == 3) tryVector(GfVec3d{});
        else tryVector(GfVec4d{});
        break;
    case Kind::Half:
        if (r.width == 1) tryVector(GfHalf{});
        else if (r.width == 2) tryVector(GfVec2h{});
        else if (r.width == 3) tryVector(GfVec3h{});
        else tryVector(GfVec4h{});
        break;
    case Kind::String:
        if (!tryVector(std::string{})) tryVector(TfToken{});
        break;
    case Kind::Token:
        if (!tryVector(TfToken{})) tryVector(std::string{});
        break;
    case Kind::Asset:   tryVector(SdfAssetPath{}); break;
    default: break;
    }
    return out;
}

// Values the schema lookup could not describe: metadata, relationship targets
// and custom types. Falls back to the C++ type name so output stays valid JSON.
Json fallbackToJson(const VtValue& value)
{
    if (value.IsHolding<TfToken>())     return Json(value.Get<TfToken>().GetString());
    if (value.IsHolding<std::string>()) return Json(value.Get<std::string>());
    if (value.IsHolding<bool>())        return Json(value.Get<bool>());
    if (value.IsHolding<int>())         return Json(value.Get<int>());
    if (value.IsHolding<double>())      return Json(value.Get<double>());
    if (value.IsHolding<float>())       return Json(value.Get<float>());
    if (value.IsHolding<SdfPath>())     return Json(value.Get<SdfPath>().GetAsString());
    if (value.IsHolding<SdfAssetPath>()) return Json(value.Get<SdfAssetPath>().GetAssetPath());
    if (value.IsHolding<GfVec2f>())     return tupleToJson(value.Get<GfVec2f>());
    if (value.IsHolding<GfVec3f>())     return tupleToJson(value.Get<GfVec3f>());
    if (value.IsHolding<GfVec3d>())     return tupleToJson(value.Get<GfVec3d>());
    if (value.IsHolding<GfVec4f>())     return tupleToJson(value.Get<GfVec4f>());
    if (value.IsHolding<GfMatrix4d>())  return matrixToJson(value.Get<GfMatrix4d>(), 4);

    const auto tryVector = [&](auto tag, auto&& emit) -> bool {
        using T = decltype(tag);
        using Vec = std::vector<T>;
        using Arr = VtArray<T>;
        if (value.template IsHolding<Arr>()) {
            for (const auto& e : value.Get<Arr>())
                emit(e);
            return true;
        }
        if (value.template IsHolding<Vec>()) {
            for (const auto& e : value.Get<Vec>())
                emit(e);
            return true;
        }
        return false;
    };

    Json out{ std::vector<Json>() };
    bool emitted = false;
    if (tryVector(SdfPath{}, [&](const SdfPath& p) { out.pushBack(Json(p.GetAsString())); }))
        emitted = true;
    else if (tryVector(TfToken{}, [&](const TfToken& t) { out.pushBack(Json(t.GetString())); }))
        emitted = true;
    else if (tryVector(std::string{}, [&](const std::string& s) { out.pushBack(Json(s)); }))
        emitted = true;
    else if (tryVector(float{}, [&](float v) { out.pushBack(Json(static_cast<double>(v))); }))
        emitted = true;
    else if (tryVector(double{}, [&](double v) { out.pushBack(Json(v)); }))
        emitted = true;
    else if (tryVector(int{}, [&](int v) { out.pushBack(Json(v)); }))
        emitted = true;
    else if (tryVector(GfVec3f{}, [&](const GfVec3f& v) { out.pushBack(tupleToJson(v)); }))
        emitted = true;
    else if (tryVector(GfVec3d{}, [&](const GfVec3d& v) { out.pushBack(tupleToJson(v)); }))
        emitted = true;

    if (emitted)
        return out;

    Json unknown;
    unknown.set("valueType", value.GetTypeName());
    unknown.set("unsupported", true);
    return unknown;
}

} // namespace

SdfValueTypeName findValueType(std::string_view typeName)
{
    const SdfSchema& schema = SdfSchema::GetInstance();
    SdfValueTypeName type = schema.FindType(std::string(typeName));
    if (type || typeName.size() < 5)
        return type;

    // The USD API calls arrays "float3Array", the text/token form is
    // "float3[]". Accept both so an AI does not have to remember which.
    if (typeName.compare(typeName.size() - 5, 5, "Array") == 0) {
        const std::string alt(typeName.substr(0, typeName.size() - 5));
        return schema.FindType(alt + "[]");
    }
    return type;
}

VtValue parseTypedValue(std::string_view typeName, std::string_view text)
{
    const SdfValueTypeName type = findValueType(typeName);
    if (!type)
        throw badValue(std::string(typeName),
            "unknown USD value type. Run 'usdstudio attr.types' for the list.");

    const Resolved r = resolve(type);
    if (!r.valid)
        throw badValue(type.GetAsToken().GetString(),
            "this type cannot be written with the CLI text syntax");

    if (r.isArray)
        return parseArrayValue(r, text);
    if (r.width > 1)
        return parseTupleValue(r, text);
    return parseScalar(r, text);
}

Json vtValueToJson(const VtValue& value)
{
    if (value.IsEmpty())
        return Json();

    const SdfValueTypeName type = SdfSchema::GetInstance().FindType(value.GetType());
    if (type) {
        const Resolved r = resolve(type);
        if (r.valid) {
            if (r.isArray)
                return arrayToJson(value, r);
            if (r.kind == Kind::Quat) {
                if (value.IsHolding<GfQuatf>()) return quatToJson(value.Get<GfQuatf>());
                if (value.IsHolding<GfQuatd>()) return quatToJson(value.Get<GfQuatd>());
                if (value.IsHolding<GfQuath>()) return quatToJson(value.Get<GfQuath>());
            } else if (r.kind == Kind::Matrix) {
                if (value.IsHolding<GfMatrix2d>()) return matrixToJson(value.Get<GfMatrix2d>(), 2);
                if (value.IsHolding<GfMatrix3d>()) return matrixToJson(value.Get<GfMatrix3d>(), 3);
                if (value.IsHolding<GfMatrix4d>()) return matrixToJson(value.Get<GfMatrix4d>(), 4);
            } else if (r.width > 1) {
                if (value.IsHolding<GfVec2f>()) return tupleToJson(value.Get<GfVec2f>());
                if (value.IsHolding<GfVec3f>()) return tupleToJson(value.Get<GfVec3f>());
                if (value.IsHolding<GfVec4f>()) return tupleToJson(value.Get<GfVec4f>());
                if (value.IsHolding<GfVec2d>()) return tupleToJson(value.Get<GfVec2d>());
                if (value.IsHolding<GfVec3d>()) return tupleToJson(value.Get<GfVec3d>());
                if (value.IsHolding<GfVec4d>()) return tupleToJson(value.Get<GfVec4d>());
                if (value.IsHolding<GfVec2i>()) return tupleToJson(value.Get<GfVec2i>());
                if (value.IsHolding<GfVec3i>()) return tupleToJson(value.Get<GfVec3i>());
                if (value.IsHolding<GfVec4i>()) return tupleToJson(value.Get<GfVec4i>());
                if (value.IsHolding<GfVec2h>()) return tupleToJson(value.Get<GfVec2h>());
                if (value.IsHolding<GfVec3h>()) return tupleToJson(value.Get<GfVec3h>());
                if (value.IsHolding<GfVec4h>()) return tupleToJson(value.Get<GfVec4h>());
            } else {
                if (r.kind == Kind::Token && value.IsHolding<TfToken>())
                    return scalarToJson(r.kind, value.Get<TfToken>());
                if (r.kind == Kind::String && value.IsHolding<std::string>())
                    return scalarToJson(r.kind, value.Get<std::string>());
                if (r.kind == Kind::Asset && value.IsHolding<SdfAssetPath>())
                    return scalarToJson(r.kind, value.Get<SdfAssetPath>());
                if (r.kind == Kind::Bool && value.IsHolding<bool>())
                    return scalarToJson(r.kind, value.Get<bool>());
                if (r.kind == Kind::Int && value.IsHolding<int>())
                    return scalarToJson(r.kind, value.Get<int>());
                if (r.kind == Kind::UInt && value.IsHolding<unsigned int>())
                    return scalarToJson(r.kind, value.Get<unsigned int>());
                if (r.kind == Kind::Int64 && value.IsHolding<std::int64_t>())
                    return scalarToJson(r.kind, value.Get<std::int64_t>());
                if (r.kind == Kind::UInt64 && value.IsHolding<std::uint64_t>())
                    return scalarToJson(r.kind, value.Get<std::uint64_t>());
                if (r.kind == Kind::UChar && value.IsHolding<unsigned char>())
                    return scalarToJson(r.kind, value.Get<unsigned char>());
                if (r.kind == Kind::Float && value.IsHolding<float>())
                    return scalarToJson(r.kind, value.Get<float>());
                if (r.kind == Kind::Double && value.IsHolding<double>())
                    return scalarToJson(r.kind, value.Get<double>());
                if (r.kind == Kind::Half && value.IsHolding<GfHalf>())
                    return scalarToJson(r.kind, value.Get<GfHalf>());
                if (r.kind == Kind::TimeCode && value.IsHolding<GfTimeCode>())
                    return scalarToJson(r.kind, value.Get<GfTimeCode>());
            }
        }
    }

    return fallbackToJson(value);
}

std::vector<std::string> supportedValueTypes()
{
    // Names registered by the Sdf schema (schema.cpp _AddValueTypes). Array
    // variants are the element name plus the USD "[]" suffix; the "Array"
    // spelling is accepted as an alias by findValueType().
    static const char* const types[] = {
        "bool", "uchar", "int", "uint", "int64", "uint64",
        "half", "float", "double", "timecode",
        "string", "token", "asset",
        "int2", "int3", "int4",
        "float2", "float3", "float4",
        "double2", "double3", "double4",
        "half2", "half3", "half4",
        "point3f", "point3d", "point3h",
        "vector3f", "vector3d", "vector3h",
        "normal3f", "normal3d", "normal3h",
        "color3f", "color3d", "color3h",
        "color4f", "color4d", "color4h",
        "texCoord2f", "texCoord2d", "texCoord2h",
        "texCoord3f", "texCoord3d", "texCoord3h",
        "quatf", "quatd", "quath",
        "matrix2d", "matrix3d", "matrix4d", "frame4d",
        "bool[]", "int[]", "int64[]", "float[]", "double[]",
        "string[]", "token[]", "asset[]",
        "point3f[]", "point3d[]", "vector3f[]",
        "normal3f[]", "color3f[]", "color4f[]",
        "texCoord2f[]", "texCoord3f[]",
        "float2[]", "float3[]", "float4[]",
        "double3[]", "int3[]", "quatf[]", "matrix4d[]",
    };
    return { types, types + (sizeof(types) / sizeof(types[0])) };
}

} // namespace UsdStudio::Cli
