# Slate plugins and mods

Slate plugins are small Lua 5.4 scripts. A plugin can add commands, slash commands, home screen entries, status line text, key bindings, and themes. It can draw its own screens and sidebar panels, react to app events, and edit notes and todos.

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
  "api": 1,
  "settings": [
    { "key": "loud", "label": "loud mode", "type": "bool", "default": false,
      "description": "shout every greeting" },
    { "key": "speed", "label": "speed", "type": "number", "default": 220, "min": 100, "max": 400, "step": 20 },
    { "key": "style", "label": "style", "type": "choice", "options": ["calm", "wild"], "default": "calm" },
    { "key": "name", "label": "your name", "type": "text", "default": "" }
  ]
}
```

If there is no `plugin.json`, the folder name is the id and `main.lua` is the entry.

**Settings.** Each entry in `settings` becomes a row on the plugin's own page in settings (`settings > plugins > enter`). Types are `bool`, `number` (with `min`, `max`, `step`), `choice` (with `options`), and `text`. `slate.settings.get(key)` returns the user's value, or the `default` from `plugin.json`. When the user changes a value, the plugin gets a `settings_changed` event.

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
| `settings_changed` | `key, value` | The user changed one of this plugin's settings |
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
slate.settings.get("speed")                  -- falls back to the default declared in plugin.json
slate.on("settings_changed", function(key, value) redraw() end)

slate.config.set("font_size", 16)            -- app settings, same as the config screen
slate.config.get("live_preview")
for _, k in ipairs(slate.config.keys()) do slate.log(k.key, k.type, k.description) end
slate.theme.list()                           -- { shell = {...}, markdown = {...} }
```

### Views: screens and sidebar panels

A view is a function that draws UI every frame. A `page` view is a full screen (open it with `slate.view.open(id)` or `:view <id>`). A `sidebar` view is a panel on the right of the note screen (`ctrl+shift+b` toggles the sidebar; `:panel <id>` hides or shows one panel).

```lua
slate.view.register {
  id = "stats",
  title = "Stats",
  location = "page",               -- or "sidebar"
  draw = function(ui, width, height)
    ui.heading("Workspace stats")
    ui.text(#slate.workspace.files() .. " notes", "muted")
    if ui.button("open today") then slate.command.run("today") end
  end,
}
```

The `ui` table uses Slate's own fonts and theme colors. Color names: `primary`, `muted`, `cyan`, `amber`, `green`, `red`, `panel`, `background`, `link`, `heading1`, `heading2`, `heading3`, `code`, `bullet`, `image`.

| Function | Description |
| --- | --- |
| `ui.text(s, color \| {color, wrap})` / `ui.muted(s)` | Text, wrapped by default |
| `ui.heading(s, level)` | A heading at the editor's heading sizes |
| `ui.markdown(s)` | Markdown rendered by the same renderer as the editor |
| `ui.button(label, {color, width})` | Returns `true` when clicked |
| `ui.link(label, {color})` | Text link; returns `true` when clicked |
| `ui.card{id, title, body, accent, badge, badge_color, selected, drag_type, drag_payload, width}` | Returns `"click"`, `"double"`, `"right"`, or `nil` |
| `ui.drop_target(type)` | After a card or panel: returns the dropped payload, or `nil` |
| `ui.dragging(type)` | `true` while something of that type is being dragged |
| `ui.begin_panel(id, w, h, {background, border, highlight, padding})` / `ui.end_panel()` | A scrollable region; 0 means "fill" |
| `ui.input(id, text, hint)` | Returns `text, submitted` |
| `ui.checkbox(label, value)` | Returns `value, changed` |
| `ui.badge(text, color)`, `ui.progress(fraction, color)` | Small status pieces |
| `ui.same_line(gap)`, `ui.space(px)`, `ui.dummy(w, h)`, `ui.separator()` | Layout |
| `ui.width()`, `ui.height()` | Space left in the current region |
| `ui.tooltip(text)`, `ui.hovered()` | About the last item |
| `ui.key("n")` | `true` when the chord was pressed this frame |

If a view's `draw` raises an error, the view shows the error and stops running until the plugin reloads.

### Todos

Todos are Markdown files in `Todos/` with `status:` and `priority:` frontmatter.

```lua
for _, todo in ipairs(slate.todos.list()) do
  -- todo.path, todo.title, todo.description, todo.state, todo.priority, todo.tags
end
local path = slate.todos.create { title = "Write docs", state = "open", priority = "high" }
slate.todos.update(path, { state = "doing" })   -- also title, description, priority
slate.todos.delete(path)
slate.todos.states()                            -- { "open", "research", "doing", "done" }
```

Priorities: `low`, `normal`, `high`, `urgent`.

### Prompts

```lua
slate.ui.prompt("rename to", "old name", function(value) slate.notify(value) end)
slate.ui.confirm("delete it?", function(ok) if ok then ... end end)
```

## Managing plugins

| Command | Action |
| --- | --- |
| `:plugins` | Open the plugin list in settings: `space` turns a plugin on or off, `enter` opens its settings page |
| `:settings plugin <id>` | Open one plugin's settings page |
| `:reload [id]` | Rediscover folders and reload all plugins, or one plugin |
| `:plugin new <name>` | Make a starter plugin |
| `:plugin folder` | Open your plugin folder |
| `:plugin enable <id>` / `:plugin disable <id>` | Toggle a plugin from the prompt |
| `:init` | Create and open `init.lua` |

Slate saves your enable and disable choices in `%APPDATA%/Slate/config/plugins/enabled.tsv`.

## Bundled plugins

- **kanban**: a drag-and-drop board over your todo documents. Press `k` on home or run `:kanban`. Right-click a card to change its priority. Settings: sort order, done column, progress bar, card details.
- **backlinks**: sidebar panel listing notes that link to the open note. Settings: snippets per note, unlinked mentions.
- **calendar**: sidebar month calendar. Green days have journal entries; click a day to open or create it. Settings: week start, entry count.
- **word-count**: status line word count and reading time. Adds `:wc`. Settings: reading time, reading speed, characters.
- **daily-tools**: `/date`, `/time`, and `/stamp` slash commands, plus `:yesterday` and a home entry. Settings: date and time format.
- **editing-tools**: `ctrl+enter` toggles tasks, `:sort` sorts selected lines, and trim on save uses the save hook. Settings: trim on save, case-sensitive sort.
- **phosphor-theme**: a mod that only ships a green terminal palette.
