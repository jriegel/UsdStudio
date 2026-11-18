// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../../Public/Commands/StageEditCommands.h"

#include "../../Public/Commands/CommandError.h"
#include "../../Public/Cli/TypedValue.h"
#include "../../Public/Document.h"

#include "pxr/base/tf/token.h"
#include "pxr/usd/sdf/valueTypeName.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
UsdStageRefPtr stageOrThrow(Document& document, const char* what)
{
    UsdStageRefPtr stage = document.getStage();
    if (!stage)
        throw CommandError::precondition(std::string("Cannot ") + what + ": no USD stage is open.",
            "Run 'document.open <file>' or 'document.new' first.");
    return stage;
}
} // namespace

// ------------------------------------------------------------------
// PrimDefineCommand
// ------------------------------------------------------------------

PrimDefineCommand::PrimDefineCommand(Document& document, std::string pathText, std::string typeName)
    : document_(document)
    , path_(std::string(pathText))
    , typeName_(std::move(typeName))
{
}

void PrimDefineCommand::execute()
{
    UsdStageRefPtr stage = stageOrThrow(document_, "define a prim");

    if (path_.GetAsString().empty() || path_.GetAsString().front() != '/')
    {
        throw CommandError::invalidValue("Prim path must start with '/': '" + path_.GetAsString() + "'.",
            "Example: prim.define /KITCHEN_LIGHTING_MODEL/Table");
    }
    if (path_.IsAbsoluteRootPath())
    {
        throw CommandError::invalidValue("/ is the pseudo root and always exists.",
            "Define a root prim instead, e.g. prim.define /MyRoot.");
    }

    // Idempotent: an existing prim is only activated when it was inactive.
    if (UsdPrim existing = stage->GetPrimAtPath(path_))
    {
        if (!existing.IsActive())
        {
            previousActive_ = existing.IsActive();
            restoredActivity_ = true;
            if (!existing.SetActive(true))
                throw CommandError::failed("Could not activate prim '" + path_.GetAsString() + "'.",
                    "The stage's edit target may be read-only.");
        }
        return;
    }

    UsdPrim prim = typeName_.empty()
        ? stage->DefinePrim(path_)
        : stage->DefinePrim(path_, TfToken(typeName_));
    if (!prim)
    {
        throw CommandError::failed("Could not define prim '" + path_.GetAsString() + "'.",
            "The parent path may be undefined or ambiguous, or the edit target is not editable.");
    }
    createdPrim_ = true;
}

void PrimDefineCommand::undo()
{
    UsdStageRefPtr stage = document_.getStage();
    if (!stage)
        return;

    if (createdPrim_)
    {
        stage->RemovePrim(path_);
        return;
    }

    if (restoredActivity_)
    {
        if (UsdPrim prim = stage->GetPrimAtPath(path_))
            prim.SetActive(previousActive_);
    }
}

std::string PrimDefineCommand::description() const
{
    if (createdPrim_)
        return "Define " + path_.GetAsString() + (typeName_.empty() ? std::string() : " as " + typeName_);
    if (restoredActivity_)
        return "Activate " + path_.GetAsString();
    return "Ensure " + path_.GetAsString();
}

// ------------------------------------------------------------------
// AttrSetCommand
// ------------------------------------------------------------------

AttrSetCommand::AttrSetCommand(Document& document, std::string primPathText, std::string attrNameText,
    std::string valueText, std::string typeName)
    : document_(document)
    , primPath_(std::string(primPathText))
    , attrName_(std::string(attrNameText))
    , valueText_(std::move(valueText))
    , typeName_(std::move(typeName))
{
}

void AttrSetCommand::execute()
{
    UsdStageRefPtr stage = stageOrThrow(document_, "set an attribute");

    UsdPrim prim = stage->GetPrimAtPath(primPath_);
    if (!prim)
    {
        throw CommandError::notFound("No prim at path '" + primPath_.GetAsString() + "'.",
            "Try 'stage.find <name>' or 'stage.children /' to locate the prim.");
    }

    UsdAttribute attr = prim.GetAttribute(attrName_);
    if (!attr)
    {
        if (typeName_.empty())
        {
            throw CommandError::notFound("Prim '" + primPath_.GetAsString() + "' has no attribute '" +
                attrName_.GetString() + "'.",
                "Add a value type to create it, e.g. 'attr.set " + primPath_.GetAsString() + " " +
                attrName_.GetString() + " 1,2,3 float3'. See 'attr.list " + primPath_.GetAsString() + "'.");
        }

        const SdfValueTypeName type = UsdStudio::Cli::findValueType(typeName_);
        if (!type)
        {
            throw CommandError::invalidValue("Unknown USD value type '" + typeName_ + "'.",
                "Argument order is: 'attr.set <path> <attrName> <value> [typeName]'. "
                "Run 'usdstudio attr.types' for the supported types.");
        }

        attr = prim.CreateAttribute(attrName_, type);
        if (!attr)
        {
            throw CommandError::failed("Could not create attribute '" + attrName_.GetString() + "' on '" +
                primPath_.GetAsString() + "'.", "The edit target may be read-only.");
        }
        createdAttribute_ = true;
    }

    // Without an explicit type the attribute's own type drives the literal
    // syntax, so 'attr.set /p xRes 32' works without repeating "int".
    const std::string typeText = typeName_.empty() ? attr.GetTypeName().GetAsToken().GetString() : typeName_;
    const VtValue value = UsdStudio::Cli::parseTypedValue(typeText, valueText_);

    attr.Get(&previousValue_); // false = no value opinion yet
    hadPreviousValue_ = !previousValue_.IsEmpty();

    if (!attr.Set(value))
    {
        throw CommandError::failed("USD refused to set '" + attrName_.GetString() + "' on '" +
            primPath_.GetAsString() + "'.",
            "The literal must match the attribute type '" + attr.GetTypeName().GetAsToken().GetString() + "'.");
    }
}

void AttrSetCommand::undo()
{
    UsdStageRefPtr stage = document_.getStage();
    if (!stage)
        return;

    UsdPrim prim = stage->GetPrimAtPath(primPath_);
    if (!prim)
        return;

    if (createdAttribute_)
    {
        prim.RemoveProperty(attrName_);
        return;
    }

    UsdAttribute attr = prim.GetAttribute(attrName_);
    if (!attr)
        return;

    if (hadPreviousValue_)
        attr.Set(previousValue_);
    else
        attr.ClearDefault(); // back to "no authored value"
}

std::string AttrSetCommand::description() const
{
    return "Set " + primPath_.GetAsString() + "." + attrName_.GetString() + " = " + valueText_;
}
