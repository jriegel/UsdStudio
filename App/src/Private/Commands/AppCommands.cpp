// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#include "../../Public/Commands/AppCommands.h"

#include "../../Public/AppContext.h"
#include "../../Public/Commands/DocumentCommands.h"
#include "../../Public/Commands/QueryRegistry.h"
#include "../../Public/Commands/StageEditCommands.h"
#include "../../Public/Commands/StageQueries.h"

#include <string>

void registerAppCommands(CommandRegistry& commands, QueryRegistry& queries, AppContext& context)
{
    // 'Document' is bound as hidden first constructor argument, so the command
    // line only carries the value arguments the agent has to type.
    commands.registerCommand<OpenDocumentCommand, Document, std::string>(
        "document.open",
        "Open a USD file and make it the active document. Undo restores the previous stage.",
        &context.document);

    commands.registerCommand<NewDocumentCommand, Document>(
        "document.new",
        "Replace the document with an empty in-memory stage.",
        &context.document);

    // Without an explicit file name the document's current name is reused,
    // which is why the argument is optional.
    commands.registerCommand<SaveDocumentCommand, Document, std::string>(
        "document.save",
        "Write the document to disk. Omit the file name to save into the current file. Not undoable.",
        &context.document,
        CommandRegistry::OptionalTrailingArgs{ 1 });

    commands.registerCommand<PrimDefineCommand, Document, std::string, std::string>(
        "prim.define",
        "Define a prim, or activate it when it already exists as an inactive or 'over' prim. "
        "'typeName' may be omitted for a plain (untyped) prim.",
        &context.document,
        CommandRegistry::OptionalTrailingArgs{ 1 });

    commands.registerCommand<AttrSetCommand, Document, std::string, std::string, std::string, std::string>(
        "attr.set",
        "Set an attribute value on an existing prim; the attribute is created when needed. "
        "'typeName' is required if the attribute does not exist yet. Run 'attr.types' to list types.",
        &context.document,
        CommandRegistry::OptionalTrailingArgs{ 1 });

    registerStageQueries(queries);
}
