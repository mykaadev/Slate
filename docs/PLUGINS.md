# Slate plugins and mods

Slate plugins are small Lua 5.4 scripts. A plugin can add commands, slash commands, home screen entries, status line text, key bindings, and themes. It can also react to app events and edit the open note.

Each plugin runs in its own sandboxed Lua state. A broken plugin shows an error in **config → plugins** and leaves the rest of Slate running.

## Quick start

1. Run `:plugin new my-plugin`. Slate creates the plugin and opens its folder.
2. Edit `main.lua` and save. Slate reloads the plugin within one second.
3. Press `ctrl+shift+p` (or `:`) and type `hello`. This runs the command the template made.

For personal tweaks, run `:init` to create `%APPDATA%/Slate/init.lua`. Slate runs this file on startup as an always-on plugin.

## Where plugins live

| Folder | Source | Enabled by default |
| --- | --- | --- |
| `<Slate>/assets/slate/plugins/` | bundled with Slate | yes |
| `%APPDATA%/Slate/plugins/` | yours | yes |
| `<workspace>/.slate/plugins/` | shipped with a workspace | **no**: enable them in config |
| `%APPDATA%/Slate/init.lua` | your init script | yes |

If two plugins use the same id, the later source in this table replaces the earlier one. Use this to override a bundled plugin.

Workspace plugins come from folders that you may not have written yourself, so they start disabled.

## Plugin folder

```text
my-plugin/
  plugin.json        optional manifest
  main.lua           entry script
  lib/util.lua       loaded with require("lib.util")
  themes/
    shell/*.tsv      shell palettes (same format as assets/slate/presets/shell)
    markdown/*.tsv   markdown accents (same format as assets/slate/presets/markdown)
```

```json
{
  "id": "my-plugin",
  "name": "My Plugin",
  "version": "0.1.0",
  "description": "One line about what it does",
  "author": "you",
  "main": "main.lua",
  "api": 1
}
```

If there is no `plugin.json`, the folder name is the id and `main.lua` is the entry.

**Mods without code.** A plugin that only ships `themes/` adds new palettes. Pick them in config, or call `slate.config.set("shell_theme", "<id>")`. See the bundled `phosphor-theme`.

## Sandbox

Plugins get these standard libraries: `string`, `table`, `math`, `utf8`, `coroutine`, and `os` (time and date functions only).

Plugins do not get `io`, `os.execute`, `os.remove`, `dofile`, `loadfile`, binary chunks, or native modules.

- Disk access goes through `slate.workspace`, which refuses paths outside the open workspace.
- `print` writes to the plugin log.
- `require` loads modules from the plugin folder only.
- Each call into a plugin has a two-second budget. A runaway loop fails with an error and does not freeze the app.

## API reference

All APIs live on the global `slate` table. Paths are workspace-relative with `/` separators.

### Core

| Function | Description |
| --- | --- |
| `slate.log(...)` | Write to the plugin log, shown in config → plugins |
| `slate.notify(...)` / `slate.error(...)` | Show text in the status line |
| `slate.time()` | Seconds since Slate started |
| `slate.open_url(url)` | Open an `http`, `https`, or `mailto` link in the default app |
| `slate.on(event, fn)` | Listen for an event (see below) |
| `slate.version`, `slate.api_version` | Slate version string and plugin API number |
| `slate.plugin` | `{ id, name, version, dir }` of the running plugin |

### Events

| Event | Arguments | Notes |
| --- | --- | --- |
| `ready` | | After the plugin loads (also after each hot reload) |
| `workspace_open` | `root` | A workspace was opened |
| `document_open` | `path` | A note was opened |
| `document_change` | `path` | After typing pauses (debounced about 0.3 s) |
| `document_before_save` | `path, text` | Return a string to replace what gets written |
| `document_save` | `path` | After a note was written |
| `unload` | | Before the plugin is disabled or reloaded |
| `quit` | | Slate is closing |

### Commands

```lua
local id = slate.command.register {
  id = "hello",                 -- final id is "<plugin-id>.hello"
  label = "Say hello",          -- shown in the command palette
  description = "Greets you",
  aliases = { "hello", "hi" },  -- typed after ':'; taken aliases are skipped with a log line
  args = true,                  -- accepts trailing text (default true)
  run = function(args)          -- return a string to show it, or false to report failure
    return "hello " .. args
  end,
}

slate.command.run("today")                 -- any command line: built-in or plugin
slate.command.run("slate.search.show", "renderer")
for _, c in ipairs(slate.command.list()) do slate.log(c.id, c.label) end
```

Commands run on the next frame through the same dispatcher the `:` prompt uses. A plugin can drive every built-in action this way, such as `slate.document.save`, `slate.journal.openToday`, `slate.browser.files`, and `slate.settings.show`.

### Slash commands, home entries, status line, keys

```lua
slate.slash.register { id = "date", description = "Insert today's date",
  run = function() return os.date("%Y-%m-%d") end }      -- or: insert = "fixed text"

slate.home.add { id = "inbox", section = "write", label = "inbox", key = "i",
  command = "open Inbox.md" }                            -- or: run = function() ... end

slate.status.set("words", "120 words")                   -- right side of the status line
slate.status.clear("words")

slate.keymap.set("ctrl+shift+d", "today")                -- a command line...
slate.keymap.set("alt+t", function() slate.notify("hi") end)  -- ...or a function
```

Key chords also work while you type in the editor, as long as they use `ctrl`, `alt`, or a function key.

### Editor and document

| Function | Description |
| --- | --- |
| `slate.editor.available()` | `true` when a note is open |
| `slate.editor.text()` / `set_text(s)` | Read or replace the whole note (one undo step) |
| `slate.editor.insert(s)` / `replace_selection(s)` | Insert at the caret, replacing the selection |
| `slate.editor.selection()` | Selected text, or `""` |
| `slate.editor.line()` / `set_line(s)` | Read or replace the caret line |
| `slate.editor.caret()` | `line, column`, both 1-based |
| `slate.editor.goto(line)` | Move the caret to a line |
| `slate.document.path()` / `name()` / `dirty()` | Information about the open note |
| `slate.document.save()` / `open(path)` | Save, or open another note |

### Workspace

| Function | Description |
| --- | --- |
| `slate.workspace.root()` / `name()` | Workspace folder and title |
| `slate.workspace.files()` | Every Markdown path |
| `slate.workspace.read(path)` / `write(path, text)` / `exists(path)` | File access inside the workspace |
| `slate.workspace.journal_path()` | Today's journal path |
| `slate.workspace.rescan()` | Refresh the file tree after you write files |

To change the open note, use `slate.editor.set_text`. If you write the open note with `slate.workspace.write`, Slate reports an external change.

### Settings

```lua
slate.settings.set("greeting", "hi")         -- per plugin, persisted in config/plugins/<id>.tsv
slate.settings.get("greeting", "default")    -- strings, numbers, and booleans keep their type

slate.config.set("font_size", 16)            -- app settings, same as the config screen
slate.config.get("live_preview")
for _, k in ipairs(slate.config.keys()) do slate.log(k.key, k.type, k.description) end
slate.theme.list()                           -- { shell = {...}, markdown = {...} }
```

### Prompts

```lua
slate.ui.prompt("rename to", "old name", function(value) slate.notify(value) end)
slate.ui.confirm("delete it?", function(ok) if ok then ... end end)
```

## Managing plugins

| Command | Action |
| --- | --- |
| `:plugins` | Open the plugin list in config (`enter` toggles a plugin) |
| `:reload [id]` | Rediscover folders and reload all plugins, or one plugin |
| `:plugin new <name>` | Make a starter plugin |
| `:plugin folder` | Open your plugin folder |
| `:plugin enable <id>` / `:plugin disable <id>` | Toggle a plugin from the prompt |
| `:init` | Create and open `init.lua` |

Slate saves your enable and disable choices in `%APPDATA%/Slate/config/plugins/enabled.tsv`.

## Bundled examples

- **word-count**: status line word count and reading time. Adds `:wc`.
- **daily-tools**: `/date`, `/time`, and `/stamp` slash commands, plus `:yesterday` and a home entry.
- **editing-tools**: `ctrl+enter` toggles tasks, `:sort` sorts selected lines, and `:trim-on-save` uses the save hook.
- **phosphor-theme**: a mod that only ships a green terminal palette.
