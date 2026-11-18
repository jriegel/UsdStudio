// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

class Document;
class CommandHistory;

// What every app-level command and query operates on: the active document
// plus the undo stack the frontend drives. Frontends (GUI, CLI, tests) build
// one of these and pass it to registerAppCommands().
struct AppContext
{
    Document& document;
    CommandHistory& history;
};
