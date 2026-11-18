// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

class AppContext;
class CommandRegistry;
class QueryRegistry;

// Registers the app-level operations once and for all: the undoable commands
// (document.*, prim.*, attr.*) bound to 'context' and the read-only stage
// queries. Every frontend - CLI, tests, and later the GUI - calls this instead
// of listing commands itself, so the self-documentation can never drift.
void registerAppCommands(CommandRegistry& commands, QueryRegistry& queries, AppContext& context);
