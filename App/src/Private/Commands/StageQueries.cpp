// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/StageQueries.h"

#include "../../Public/AppContext.h"
#include "../../Public/Commands/CommandError.h"
#include "../../Public/Commands/CommandHistory.h"
#include "../../Public/Commands/QueryRegistry.h"
#include "../../Public/Cli/Json.h"
#include "../../Public/Cli/TypedValue.h"
#include "../../Public/Document.h"

#include "pxr/base/tf/token.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdf/valueTypeName.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/primFlags.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usd/relationship.h"
#include "pxr/usd/usd/stage.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
using Json = UsdStudio::Cli::Json;
using ArgSpan = std::span<const std::string_view>;

// Walking a stage stops here. The CLI applies --limit on top of this, so an
// answer always fits into an agent's context window.
constexpr std::size_t kRowCap = 5000;

std::string toLower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool containsCI(std::string_view haystack, std::string_view needle)
{
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

UsdStageRefPtr stageOrThrow(AppContext& ctx)
{
    UsdStageRefPtr stage = ctx.document.getStage();
    if (!stage)
        throw CommandError::precondition("No USD stage is open.",
            "Run 'document.open <file>' or 'document.new' first.");
    return stage;
}

// QueryRegistry::run already checks the argument count, so an empty token can
// only come from an explicitly empty string on the command line.
std::string_view requiredText(ArgSpan args, std::size_t i, std::string_view name)
{
    if (i >= args.size() || args[i].empty())
    {
        throw CommandError::invalidValue(std::string("Argument '") + std::string(name) +
            "' must not be empty.",
            "Quote the argument if it contains spaces.");
    }
    return args[i];
}

SdfPath requiredPath(ArgSpan args, std::size_t i, std::string_view name)
{
    const std::string_view text = requiredText(args, i, name);
    if (text.front() != '/')
        throw CommandError::invalidValue(std::string("Argument '") + std::string(name) +
            "' is not a prim path: " + std::string(text),
            "Prim paths start with '/', e.g. '/KITCHEN_LIGHTING_MODEL'.");
    return SdfPath(std::string(text));
}

UsdPrim requiredPrim(const UsdStageRefPtr& stage, ArgSpan args, std::size_t i, std::string_view name)
{
    const SdfPath path = requiredPath(args, i, name);
    UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim)
    {
        throw CommandError::notFound("No prim at path '" + path.GetAsString() + "'.",
            "Try 'stage.find <name> --json' or 'stage.children / --json'.");
    }
    return prim;
}

std::string typeNameText(const SdfValueTypeName& type)
{
    return type ? type.GetAsToken().GetString() : std::string();
}

Json primRow(const UsdPrim& prim)
{
    Json row = Json::object();
    row.setString("path", prim.GetPath().GetAsString());
    row.setString("name", prim.GetName().GetString());
    row.setString("typeName", prim.GetTypeName().GetString());
    row.set("active", prim.IsActive());
    row.set("instanceProxy", prim.IsInstanceProxy());
    return row;
}

// ── stage.info ────────────────────────────────────────────────────────────
Json queryStageInfo(AppContext& ctx)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);
    SdfLayerHandle root = stage->GetRootLayer();

    Json j = Json::object();
    j.setString("fileName", ctx.document.getFileName());
    j.setString("rootLayer", root ? root->GetIdentifier() : std::string());
    j.set("isModified", ctx.document.isModified());
    j.setString("defaultPrim", stage->GetDefaultPrim() ? stage->GetDefaultPrim().GetPath().GetAsString() : std::string());
    j.set("startTimeCode", stage->GetStartTimeCode());
    j.set("endTimeCode", stage->GetEndTimeCode());
    j.set("timeCodesPerSecond", stage->GetTimeCodesPerSecond());
    j.set("framesPerSecond", stage->GetFramesPerSecond());
    j.set("layerCount", static_cast<long long>(stage->GetUsedLayers().size()));
    j.setString("editTarget", stage->GetEditTarget().GetLayer() ? stage->GetEditTarget().GetLayer()->GetIdentifier() : std::string());
    return j;
}

// ── stage.stats ───────────────────────────────────────────────────────────
Json queryStageStats(AppContext& ctx)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);

    long long prims = 0;
    long long proxies = 0;
    long long inactive = 0;
    long long attributes = 0;
    long long relationships = 0;
    bool truncated = false;
    std::unordered_map<std::string, long long> byType;

    for (const UsdPrim& prim : stage->Traverse(UsdTraverseInstanceProxies()))
    {
        ++prims;
        if (prim.IsInstanceProxy())
            ++proxies;
        else if (!prim.IsActive())
            ++inactive;

        ++byType[prim.GetTypeName().GetString()];

        // Counting every attribute of a huge stage is needlessly slow; past
        // the cap only the prim numbers keep growing.
        if (static_cast<std::size_t>(prims) <= kRowCap)
        {
            attributes += static_cast<long long>(prim.GetAttributes().size());
            relationships += static_cast<long long>(prim.GetRelationships().size());
        }
        else
            truncated = true;
    }

    std::vector<std::pair<std::string, long long>> sorted(byType.begin(), byType.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });

    Json types = Json::array();
    for (std::size_t i = 0; i < sorted.size() && i < 50; ++i)
    {
        Json row = Json::object();
        row.setString("typeName", sorted[i].first);
        row.set("count", sorted[i].second);
        types.pushBack(std::move(row));
    }

    Json j = Json::object();
    j.set("primCount", prims);
    j.set("instanceProxyCount", proxies);
    j.set("inactiveCount", inactive);
    j.set("attributeCount", attributes);
    j.set("relationshipCount", relationships);
    j.set("typesTruncated", sorted.size() > 50);
    j.set("truncated", truncated);
    j.set("typesByCount", std::move(types));
    return j;
}

// ── stage.children <path> ─────────────────────────────────────────────────
Json queryStageChildren(AppContext& ctx, ArgSpan args)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);
    const UsdPrim prim = requiredPrim(stage, args, 0, "path");

    long long total = 0;
    Json rows = Json::array();
    for (const UsdPrim& child : prim.GetChildren())
    {
        ++total;
        if (rows.size() < kRowCap)
            rows.pushBack(primRow(child));
    }

    Json j = Json::object();
    j.setString("path", prim.GetPath().GetAsString());
    j.setString("typeName", prim.GetTypeName().GetString());
    j.set("childCount", total);
    j.set("truncated", total > static_cast<long long>(rows.size()));
    j.set("children", std::move(rows));
    return j;
}

// ── stage.find <needle> [caseSensitive] ───────────────────────────────────
Json queryStageFind(AppContext& ctx, ArgSpan args)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);
    const std::string_view needle = requiredText(args, 0, "needle");
    const bool caseSensitive = args.size() > 1 && args[1] == "true";

    long long total = 0;
    Json rows = Json::array();
    for (const UsdPrim& prim : stage->Traverse(UsdTraverseInstanceProxies()))
    {
        const std::string path = prim.GetPath().GetAsString();
        const std::string name = prim.GetName().GetString();
        const bool hit = caseSensitive
            ? path.find(needle) != std::string::npos || name.find(needle) != std::string::npos
            : containsCI(path, needle) || containsCI(name, needle);
        if (!hit)
            continue;

        ++total;
        if (rows.size() < kRowCap)
            rows.pushBack(primRow(prim));
    }

    Json j = Json::object();
    j.setString("needle", needle);
    j.set("matchCount", total);
    j.set("truncated", total > static_cast<long long>(rows.size()));
    j.set("matches", std::move(rows));
    return j;
}

// ── prim.get <path> ───────────────────────────────────────────────────────
Json queryPrimGet(AppContext& ctx, ArgSpan args)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);
    const UsdPrim prim = requiredPrim(stage, args, 0, "path");

    Json schemas = Json::array();
    for (const TfToken& schema : prim.GetAppliedSchemas())
        schemas.pushBack(Json(schema.GetString()));

    Json children = Json::array();
    long long childCount = 0;
    for (const UsdPrim& child : prim.GetChildren())
    {
        ++childCount;
        if (children.size() < 50)
            children.pushBack(Json(child.GetPath().GetAsString()));
    }

    Json j = Json::object();
    j.setString("path", prim.GetPath().GetAsString());
    j.setString("name", prim.GetName().GetString());
    j.setString("typeName", prim.GetTypeName().GetString());
    j.setString("specifier", prim.GetSpecifier() == SdfSpecifierDef ? "def"
        : prim.GetSpecifier() == SdfSpecifierOver ? "over" : "class");
    j.set("active", prim.IsActive());
    j.set("defined", prim.IsDefined());
    j.set("instanceProxy", prim.IsInstanceProxy());
    j.set("childCount", childCount);
    j.set("appliedSchemas", std::move(schemas));
    j.set("children", std::move(children));
    return j;
}

// ── attr.get <primPath> <attrName> [timeCode] ─────────────────────────────
Json queryAttrGet(AppContext& ctx, ArgSpan args)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);
    const UsdPrim prim = requiredPrim(stage, args, 0, "primPath");
    const TfToken attrName(std::string(requiredText(args, 1, "attrName")));

    UsdAttribute attr = prim.GetAttribute(attrName);
    if (!attr)
    {
        throw CommandError::notFound("Prim '" + prim.GetPath().GetAsString() + "' has no attribute '" +
            attrName.GetString() + "'.",
            "Run 'attr.list " + prim.GetPath().GetAsString() + " --json' to see what exists.");
    }

    double time = stage->GetStartTimeCode();
    if (args.size() > 2 && !args[2].empty())
    {
        try
        {
            time = std::stod(std::string(args[2]));
        }
        catch (const std::exception&)
        {
            throw CommandError::invalidValue("Time code '" + std::string(args[2]) + "' is not a number.",
                "Usage: attr.get <primPath> <attrName> [timeCode]");
        }
    }

    VtValue value;
    attr.Get(&value, time); // false = no value at this time; 'value' stays empty

    Json j = Json::object();
    j.setString("primPath", prim.GetPath().GetAsString());
    j.setString("attrName", attrName.GetString());
    j.setString("typeName", typeNameText(attr.GetTypeName()));
    j.set("time", time);
    j.set("hasValue", attr.HasValue());
    j.set("authored", attr.HasAuthoredValue());
    j.set("timeVarying", attr.ValueMightBeTimeVarying());
    j.set("value", UsdStudio::Cli::vtValueToJson(value));
    return j;
}

// ── attr.list <primPath> ──────────────────────────────────────────────────
Json queryAttrList(AppContext& ctx, ArgSpan args)
{
    const UsdStageRefPtr stage = stageOrThrow(ctx);
    const UsdPrim prim = requiredPrim(stage, args, 0, "primPath");

    long long total = 0;
    Json rows = Json::array();
    for (const UsdAttribute& attr : prim.GetAttributes())
    {
        ++total;
        if (rows.size() >= kRowCap)
            continue;

        Json row = Json::object();
        row.setString("name", attr.GetName().GetString());
        row.setString("typeName", typeNameText(attr.GetTypeName()));
        row.set("hasValue", attr.HasValue());
        row.set("authored", attr.HasAuthoredValue());
        row.set("timeVarying", attr.ValueMightBeTimeVarying());
        rows.pushBack(std::move(row));
    }

    Json rels = Json::array();
    for (const UsdRelationship& rel : prim.GetRelationships())
    {
        if (rels.size() >= 100)
            break;

        std::vector<SdfPath> targets;
        rel.GetTargets(&targets);
        Json targetRows = Json::array();
        for (const SdfPath& target : targets)
            targetRows.pushBack(Json(target.GetAsString()));

        Json row = Json::object();
        row.setString("name", rel.GetName().GetString());
        row.set("targets", std::move(targetRows));
        rels.pushBack(std::move(row));
    }

    Json j = Json::object();
    j.setString("primPath", prim.GetPath().GetAsString());
    j.setString("typeName", prim.GetTypeName().GetString());
    j.set("attributeCount", total);
    j.set("truncated", total > static_cast<long long>(rows.size()));
    j.set("attributes", std::move(rows));
    j.set("relationships", std::move(rels));
    return j;
}

// ── history.list ──────────────────────────────────────────────────────────
Json queryHistoryList(AppContext& ctx)
{
    const std::vector<std::string> entries = ctx.history.descriptions();
    const std::size_t undoDepth = ctx.history.undoDepth();

    Json rows = Json::array();
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        Json row = Json::object();
        row.set("index", static_cast<long long>(i));
        row.setString("description", entries[i]);
        row.set("applied", i < undoDepth);
        rows.pushBack(std::move(row));
    }

    Json j = Json::object();
    j.set("size", static_cast<long long>(ctx.history.size()));
    j.set("undoDepth", static_cast<long long>(undoDepth));
    j.set("redoDepth", static_cast<long long>(ctx.history.redoDepth()));
    j.set("canUndo", ctx.history.canUndo());
    j.set("canRedo", ctx.history.canRedo());
    j.set("entries", std::move(rows));
    return j;
}

Json queryAttrTypes(AppContext&, ArgSpan)
{
    // The literal syntax rules live in Cli/TypedValue.h; repeating them here
    // keeps the answer useful without a second round trip.
    Json types = Json::array();
    for (const std::string& name : UsdStudio::Cli::supportedValueTypes())
        types.pushBack(name);

    Json j = Json::object();
    j.set("types", std::move(types));
    j.setString("syntax", "Scalars: numbers, true/false, quoted text for strings/tokens/assets. "
        "Tuples: \"1,2,3\". Arrays: \"1,2,3\" or \"a,b\" with an array typeName. "
        "Both float3[] and float3Array are accepted.");
    j.setString("note", "Pass the typeName as the last argument of 'attr.set' when the attribute "
        "does not exist yet; otherwise the existing attribute's type is used.");
    return j;
}

} // namespace

void registerStageQueries(QueryRegistry& registry)
{
    registry.registerQuery("stage.info", "Describe the open stage: file, default prim, time range.",
        "object", {},
        [](AppContext& ctx, ArgSpan) { return queryStageInfo(ctx); });

    registry.registerQuery("stage.stats", "Count prims, attributes and relationships, grouped by type.",
        "object", {},
        [](AppContext& ctx, ArgSpan) { return queryStageStats(ctx); });

    registry.registerQuery("stage.children", "List the direct children of a prim.",
        "object with children[]: {path,name,typeName,active,instanceProxy}",
        { requiredArg("path", "path") },
        [](AppContext& ctx, ArgSpan args) { return queryStageChildren(ctx, args); });

    registry.registerQuery("stage.find", "Find prims whose name or path contains a substring.",
        "object with matches[]: {path,name,typeName,active,instanceProxy}",
        { requiredArg("needle", "string"), optionalArg("caseSensitive", "bool") },
        [](AppContext& ctx, ArgSpan args) { return queryStageFind(ctx, args); });

    registry.registerQuery("prim.get", "Read one prim: specifier, type, activity, schemas, children.",
        "object",
        { requiredArg("path", "path") },
        [](AppContext& ctx, ArgSpan args) { return queryPrimGet(ctx, args); });

    registry.registerQuery("attr.get", "Read one attribute value at an optional time code.",
        "object",
        { requiredArg("primPath", "path"), requiredArg("attrName", "string"), optionalArg("timeCode", "double") },
        [](AppContext& ctx, ArgSpan args) { return queryAttrGet(ctx, args); });

    registry.registerQuery("attr.list", "List the attributes and relationship targets of a prim.",
        "object",
        { requiredArg("primPath", "path") },
        [](AppContext& ctx, ArgSpan args) { return queryAttrList(ctx, args); });

    registry.registerQuery("attr.types", "List the value types 'attr.set' can parse, with literal syntax.",
        "object with types[]", {},
        [](AppContext& ctx, ArgSpan args) { return queryAttrTypes(ctx, args); });

    registry.registerQuery("history.list", "Show the undo/redo stacks with their descriptions.",
        "object", {},
        [](AppContext& ctx, ArgSpan) { return queryHistoryList(ctx); });
}
