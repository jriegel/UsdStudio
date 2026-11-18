# Agent Skill: Driving a running UsdStudio instance from the CLI

This document is written for AI agents and harnesses. It describes how to
inspect, search, edit and undo inside a **running UsdStudio GUI** without
touching mouse or keyboard, using `UsdStudioCmd --remote`.

## Mental model

* The UsdStudio GUI listens on a local named pipe (`usdstudio-remote` by
  default). Its status bar shows the active pipe name.
* `UsdStudioCmd --remote <operation>` connects to that pipe, sends the
  operation and prints one JSON answer line. The operation runs **inside the
  GUI process**: it sees the document the user has open and its edits land in
  the same undo history the user can step through with Ctrl+Z.
* One operation per process call is fine; a harness may also batch several
  lines with repeatable `--command`, or pipe lines to stdin.
* Without a running GUI (or with remote control switched off in the File
  menu) the CLI exits `5` and the error hint explains how to fix it. In that
  case you can still run everything *locally* (same commands, fresh empty
  document) by dropping `--remote`.

## Discovery first

Everything the CLI can do is machine-readable. Do not guess - ask:

```
UsdStudioCmd --agent-help            # compact human/agent briefing
UsdStudioCmd --schema                # full JSON: ops, args, flags, exit codes
UsdStudioCmd --types                 # value types accepted by attr.set
UsdStudioCmd --help <operation>      # one operation in detail
```

`--agent-help` and `--schema` always answer locally (no GUI needed) and stay
current because they are generated from the live registries.

## Session workflow

1. **Check the connection and state**

   ```
   UsdStudioCmd --remote stage.info
   ```

   `data.fileName` is the open file (`""` for an unsaved/new stage), plus
   default prim, time range and edit target.

2. **Explore before editing**

   ```
   UsdStudioCmd --remote stage.stats                    # what is in the scene
   UsdStudioCmd --remote stage.children /Some/Path      # one level only
   UsdStudioCmd --remote stage.find Stove --limit 20    # substring search
   UsdStudioCmd --remote prim.get /Some/Path            # one prim in detail
   UsdStudioCmd --remote attr.list /Some/Path           # attributes present
   UsdStudioCmd --remote attr.get /Some/Path local:width
   ```

   Use `--fields path,name` to shrink answers and `--limit N` to cap lists;
   a truncated answer sets `"truncated":true` and adds a note.

3. **Edit undoably**

   ```
   UsdStudioCmd --remote prim.define /New/Thing --type Xform
   UsdStudioCmd --remote attr.set /New/Thing width 1.5
   UsdStudioCmd --remote attr.set /Some/Path xformOp:translate "1,2,3"
   ```

   Every edit reports `"undoable":true`. Use `--dry-run` to preview an edit
   without applying it.

4. **Verify, then checkpoint**

   Re-read with `prim.get`/`attr.get` after each edit. When the scene is in a
   good state, save it (`document.save` writes the current file; with no file
   yet, pass one: `document.save C:\out\scene.usd`).

5. **Undo freely**

   ```
   UsdStudioCmd --remote history.list     # see the stack (index, description)
   UsdStudioCmd --remote history.undo     # step back
   UsdStudioCmd --remote history.redo     # step forward
   ```

   Undo/redo affect the same stack the user sees in the "Command History"
   dock - never undo more than you own; check `history.list` first.

## Reading answers

stdout is exactly one compact JSON line per operation:

```
{"ok":true,"cmd":"stage.find","data":{...},"exit":0}
{"ok":false,"cmd":"attr.set","error":{"code":"notFound","message":"...","hint":"..."},"exit":4}
```

* Always branch on `ok` first; then read `error.code` and `error.hint` - the
  hint names the command to run next (e.g. run `stage.find` to get the path).
* Exit codes equal the CLI contract: `0` ok, `2` usage, `3` invalidValue,
  `4` notFound, `5` failed/precondition, `6` internal. The remote transport
  mirrors the code the GUI operation produced.
* Logs, warnings and USD loader chatter go to stderr - never parse stdout
  for them.

## Failure handling

* `No running UsdStudio instance is listening for remote control on pipe
  '<name>'` (exit 5): start the GUI, or ask the user; several GUIs running:
  the status bar of the wanted one shows its pipe name, pass it via
  `--pipe <name>`.
* `The UsdStudio remote pipe ... is busy` (exit 5): another CLI is talking to
  that instance; retry shortly or target another instance.
* Unknown operation (exit 2): the answer carries `Did you mean: ...?`
  suggestions; `--schema` lists every valid name.

## Rules of thumb

1. Query before you write; write small; verify after each write.
2. Prefer `stage.find`/`stage.children` over dumping whole trees.
3. One logical change per operation so the user's undo steps stay meaningful.
4. Never `document.new` or `document.open` over unsaved user work without
   asking - the change is undoable, but the user might not notice.
5. Trust the envelopes, not the process exit code alone; `"ok":true` means
   the GUI applied the operation.
