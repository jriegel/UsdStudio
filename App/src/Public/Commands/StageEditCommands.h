// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include "ICommand.h"

#include "pxr/base/tf/token.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/base/vt/value.h"

#include <array>
#include <string>
#include <string_view>

class Document;

// Defines (or activates) a prim. Registration is "prim.define <path> [typeName]".
//
// Undo removes a prim that this command created. If the prim already existed
// as an "over", undo restores its previous activity instead of deleting it.
// The command stores the path, never a UsdPrim (see ICommand.h).
//
// Note on SdfChangeBlock: wrapping a *single* DefinePrim in one makes this
// prebuilt OpenUSD 26.11 report "Failed to define UsdPrim" (see AGENTS.md),
// so the command intentionally runs without a change block.
class PrimDefineCommand : public ICommand
{
public:
    PrimDefineCommand(Document& document, std::string pathText, std::string typeName);

    void execute() override;
    void undo() override;
    std::string description() const override;

    static constexpr std::array<std::string_view, 2> argNames() { return { "path", "typeName" }; };

private:
    Document& document_;
    pxr::SdfPath path_;
    std::string typeName_;

    // Undo state.
    bool createdPrim_ = false;
    bool restoredActivity_ = false;
    bool previousActive_ = true;
};

// Sets an attribute value. Registration is
// "attr.set <primPath> <attrName> <valueText> [typeName]"; the value text is
// converted with Cli::parseTypedValue, which reports unknown types or
// malformed literals as CommandError::InvalidValue.
//
// Undo restores the previously authored value (or clears the attribute when
// it had none). Attributes created by this command are removed again.
class AttrSetCommand : public ICommand
{
public:
    AttrSetCommand(Document& document, std::string primPathText, std::string attrNameText,
        std::string valueText, std::string typeName);

    // Throws CommandError for a missing prim/attribute or a bad literal.
    void execute() override;
    void undo() override;
    std::string description() const override;

    static constexpr std::array<std::string_view, 4> argNames()
    {
        return { "primPath", "attrName", "valueText", "typeName" };
    };

private:
    Document& document_;
    pxr::SdfPath primPath_;
    pxr::TfToken attrName_;
    std::string valueText_;
    std::string typeName_;

    bool createdAttribute_ = false;
    bool hadPreviousValue_ = false;
    pxr::VtValue previousValue_;
};
