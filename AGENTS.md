# AGENTS.md: guide for AI agents and contributors

USDStudio is a Qt C++ app which utilize the OpenUSD library to load, view and change USD Stages. It uses Hydra 2 render delegates to view the scenes. It is build with AI as first class citizen. 

## Layout

* `/App` - Qt-free application core (`UsdStudioAppLib`). All data work lives
  here, including the command framework in `App/src/Public/Commands`:
  `ICommand` (execute/undo), `CommandRegistry` (name -> factory with typed
  argument parsing from string tokens), `CommandHistory` (headless undo stack)
  and the document commands. Keep these files free of Qt includes so the same
  commands can run from the GUI, tests and the CLI/AI frontend.
* `/Gui` - Qt widgets/QML shell (`UsdStudioGuiLib`). `UndoCommandAdapter` is
  the only place where commands meet `QUndoStack`; `MainWindow` owns the
  registry and the undo stack. The Scene Graph dock shows the stage through
  `UsdStageModel`, a lazy `QAbstractItemModel` shadow tree (chunked
  `fetchMore`, `SdfPath` -> node map, filtering inside the model, refresh via
  `UsdNotice::ObjectsChanged`); `SceneTreeView` and `BreadcrumbBar` are the
  view-side widgets. Never store raw pointers into a stage - the model drops
  its nodes on every `ObjectsChanged` for the touched subtree.
* `/App/Main` builds `UsdStudioCmd`, the AI-oriented CLI (`App/src/Public/Cli`
  + `App/src/Private/Cli`). Contract: never interactive; exactly one
  deterministic JSON result per operation on stdout
  (`{ok, cmd, data | error{code,message,context,hint}, notes}`); logs and
  notes go to stderr only. Exit codes mirror the error codes (0 ok, 2 usage,
  3 invalidValue, 4 notFound, 5 failed/precondition, 6 internal). Batch mode:
  repeatable `--command "op args"` or stdin lines; a process holds one
  document, so scripts open it first. Self-documentation via `--help`,
  `--schema`, `--agent-help`, `--types`; agent-friendly trimming via
  `--limit`/`--fields`, previews via `--dry-run`. When you change operations,
  flags or exit codes, update `--schema`/`--agent-help` and keep
  `App/Tests/CliTests.cpp` in sync - that test encodes the contract.
* New commands: implement `ICommand`, register it in `CommandRegistry`
  (`registerCommand<Cmd, Ctx, Args...>(name, help, context)`; the CLI-facing
  registrations live in `AppCommands.cpp`) and run it through the undo stack.
  Read-only queries go through `QueryRegistry` instead of `ICommand`. Reference prims by `SdfPath` only and batch prims
  edits in an `SdfChangeBlock`.
* Known OpenUSD quirk (prebuilt 26.11): defining a *new* prim while an
  `SdfChangeBlock` is already active can log
  `Failed to define UsdPrim <path>` even though the prim spec is written and
  the prim exists afterwards. Re-verify with a newer OpenUSD before relying on
  it; `Gui/Tests/UsdStageModelTests.cpp` currently authors prims without a
  surrounding change block because of it.
* Tests run via CTest, see `Build.md`.
