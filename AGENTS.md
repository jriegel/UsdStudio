# AGENTS.md: guide for AI agents and contributors

USDStudio is a Qt C++ app which utilize the OpenUSD library to load, view and change USD Stages. It uses Hydra 2 render delegates to view the scenes. It is build with AI as first class citizen. 

## Layout

* `/App` - Qt-free application core (`UsdStudioAppLib`). All data work lives
  here, including the command framework in `App/src/Public/Commands`:
  `ICommand` (execute/undo), `CommandRegistry` (name -> factory with typed
  argument parsing from string tokens), `CommandHistory` (headless undo stack)
  and the document commands. Keep these files free of Qt includes so the same
  commands can run from the GUI, tests and a future CLI/AI frontend.
* `/Gui` - Qt widgets/QML shell (`UsdStudioGuiLib`). `UndoCommandAdapter` is
  the only place where commands meet `QUndoStack`; `MainWindow` owns the
  registry and the undo stack. The Scene Graph dock shows the stage through
  `UsdStageModel`, a lazy `QAbstractItemModel` shadow tree (chunked
  `fetchMore`, `SdfPath` -> node map, filtering inside the model, refresh via
  `UsdNotice::ObjectsChanged`); `SceneTreeView` and `BreadcrumbBar` are the
  view-side widgets. Never store raw pointers into a stage - the model drops
  its nodes on every `ObjectsChanged` for the touched subtree.
* New commands: implement `ICommand`, register it in `CommandRegistry`
  (`registerCommand<Cmd, Ctx, Args...>(name, help, context)`) and run it
  through the undo stack. Reference prims by `SdfPath` only and batch prims
  edits in an `SdfChangeBlock`.
* Known OpenUSD quirk (prebuilt 26.11): defining a *new* prim while an
  `SdfChangeBlock` is already active can log
  `Failed to define UsdPrim <path>` even though the prim spec is written and
  the prim exists afterwards. Re-verify with a newer OpenUSD before relying on
  it; `Gui/Tests/UsdStageModelTests.cpp` currently authors prims without a
  surrounding change block because of it.
* Tests run via CTest, see `Build.md`.
