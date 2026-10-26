# Slate Runtime Architecture

Slate is moving from a feature/mode monolith into a small modular runtime. Current behavior is preserved, but new work should register capabilities through modules, services, and commands instead of wiring systems together directly.

## Runtime backbone

The core runtime owns these registries:

- `ServiceLocator` — shared service instances.
- `EventBus` — decoupled notifications.
- `CommandRegistry` — stable command ids, aliases, and optional executors.
- `ModuleRegistry` — loads, starts, ticks, and stops app modules.
- `ToolRegistry` — current route/mode adapter.

## Module contract

A module implements `IModule`:

```cpp
class IModule
{
public:
    virtual ModuleDescriptor Describe() const = 0;
    virtual void Register(ModuleContext& context) = 0;
    virtual void Start(ModuleContext& context) {}
    virtual void Update(ModuleContext& context) {}
    virtual void Stop(ModuleContext& context) {}
};
```

`Register` should add services, commands, panels, routes, event handlers, or jobs. `Start` should activate runtime behavior. `Update` should advance module-owned background work. `Stop` should clean up transient state.

## Current built-in modules

`DefaultModules.cpp` now composes Slate from dedicated modules:

- `slate.workspace` — owns `SlateWorkspaceContext`, workspace loading, and workspace background updates.
- `slate.editor` — owns `SlateEditorContext` and native editor attachment.
- `slate.ui` — owns shared Slate UI state plus the module-owned overlay controllers:
  - `SlateSearchOverlayController`
  - `SlateWorkspaceSwitcherOverlay`
- `slate.todos` — owns the todo-domain service used to collect/query markdown todos.
- `slate.commands` — registers the built-in command surface.
- `slate.routes` — registers existing Slate modes as route adapters.
- `slate.startup` — chooses the initial route after services are live.
- `slate.plugins` — owns `PluginManager` (Lua plugins) and the `:plugins`, `:reload`, `:plugin new`, and `:init` commands.

The old feature compatibility layer has been removed from the app runtime. New behavior should enter through modules, commands, services, overlays, or routes.

## Command contract

Commands use stable ids such as:

- `slate.document.save`
- `slate.document.open`
- `slate.journal.openToday`
- `slate.workspace.open`
- `slate.search.show`
- `slate.todos.show`

Aliases like `w`, `today`, `search`, or `workspace <path>` are registered in `CommandRegistry`. The command prompt resolves text through `CommandRegistry`, then delegates through `SlateCommandDispatcher` so the giant command switch does not live inside `SlateModeBase`.

## Dependency rule

Use this direction:

```text
Core Runtime
  -> Slate contracts/services
  -> Slate modules
  -> route/mode adapters
```

Avoid direct module-to-module calls. Prefer:

1. Commands for user actions.
2. Services for domain behavior.
3. Events for cross-module reactions.
4. Panels/overlays for UI behavior.

## Direction for future work

New functionality should follow this path:

1. Add a module.
2. Register service interfaces instead of concrete app contexts.
3. Register commands for user actions.
4. Render UI through panels/overlays that call commands.
5. Use events for cross-module reactions.

Avoid adding more behavior to `SlateModeBase` or `SlateWorkspaceContext`. Those classes should keep shrinking as commands, overlays, and services move into dedicated modules.

## Overlay ownership

Shared overlays are now module-owned services instead of private `SlateModeBase` state.

- `SlateSearchOverlayController` owns search query text, search scope, search mode, result navigation, result querying, helper text, and search overlay rendering.
- `SlateWorkspaceSwitcherOverlay` owns workspace switcher visibility, selected workspace navigation, keyboard handling, and switcher rendering.

`SlateModeBase` is now only a compatibility adapter for these overlays. It opens them, forwards selected actions back into existing document/workspace flows, and mirrors visibility for legacy mode helpers. New overlays should follow this pattern:

1. Store overlay state inside the overlay controller.
2. Register the controller through a module.
3. Expose callbacks or commands for side effects.
4. Keep modes as thin route adapters.

## Segmentation pass: todo overlay extraction

The legacy feature runtime has been removed. Slate services are now registered through modules instead of through feature wrappers.

`SlateModeBase` should remain a shell adapter only. The todo list, todo form state, todo keyboard navigation, todo filtering, and slash-command replacement live in `SlateTodoOverlayController`, registered by `SlateTodoModule`. Todo collection, update, and deletion live in `TodoService`, so markdown todo mutations stay in the todo module domain layer rather than in the overlay.

Current ownership split:

```text
slate.todos
  TodoService
  SlateTodoOverlayController

SlateModeBase
  root window shell
  prompt/confirm/status shell
  command dispatch adapter
  search/workspace/todo overlay callback wiring only
```

Do not add new todo UI state or markdown mutation behavior back to `SlateModeBase`. If another mode needs todo behavior, it should resolve `SlateTodoOverlayController` or execute a command registered by `slate.todos`.

## Shortcut ownership

Keyboard bindings are owned by `Slate.Input` / `ShortcutService`. New modules should register or consume stable shortcut actions instead of hardcoding display text like `(ctrl+s)` or direct key checks. Defaults live in `src/App/Slate/Input/ShortcutService.cpp`; user overrides are written to `config/input/shortcuts.tsv` by the settings/config screen. Shortcut editing uses an input-capture modal rather than cycling hardcoded choices.

## Todo document ownership

Todos are owned by `Slate.Todos` and stored as standalone Markdown documents under the workspace `Todos/` folder. The todo overlay creates, updates, deletes, opens, and filters todo documents through `TodoService`; it should not write `#todo` blocks into arbitrary notes. Legacy inline todo parsing remains only as Markdown compatibility for old files and rendering.

## Extension points

The shell never depends on the Lua runtime. Plugins (and any native module) contribute through two services that `slate.ui` registers:

- `SlateExtensionRegistry` stores home entries, status line items, slash commands, and command key bindings, keyed by owner. `RemoveOwner` removes everything an owner added.
- `SlateShellBridge` is a queue of shell requests: run a command line, set status, prompt, and confirm. `SlateModeBase` drains it at the start of each frame. Services and plugins never call into a mode directly.

App events live in `App/Slate/Core/SlateEvents.h` (`DocumentOpened`, `DocumentWillSave`, `DocumentSaved`, `WorkspaceOpened`). `SlateModeBase` publishes them through the `EventBus`. `DocumentWillSave` carries a pointer to the text, so listeners can rewrite it before the write.

`PluginManager` (`App/Slate/Plugins`) gives each plugin its own sandboxed `lua_State`. Lua is compiled as C++, so script errors unwind with exceptions. `PluginLuaApi.cpp` holds the `slate.*` bindings. The plugin API is documented in `docs/PLUGINS.md`.

## Live editor

The note editor is drawn by Slate with ImGui (`App/Slate/Editor/Live`). There is no native editor control, so every Markdown structure can render as a real widget.

| File | Role |
| --- | --- |
| `TextBuffer` | UTF-8 text with `
` endings, a line index, word and code point motion, and grouped undo/redo |
| `MarkdownBlocks` | Splits lines into blocks: code fences, tables, callouts, frontmatter, math, and single lines |
| `CodeHighlighter` | Tokenizes fenced code for about 20 languages, carrying block comments across lines |
| `InlineLayout` | Lays out one line into positioned atoms: wrapping with hanging indents, heading sizes, bullets, checkboxes, quote bars, and code/highlight/tag backgrounds. Plugins reuse it through `ui.markdown` |
| `LiveEditor` | Builds per-frame visuals, draws widgets, owns caret, selection, mouse, keyboard, clipboard, and IME |
| `LivePreviewLexer` | `StyleLivePreviewLine`: the per-byte styling rules shared by the layout |

Rendering follows Obsidian's live preview model:

- Lines and blocks touched by the selection show their Markdown source (dimmed syntax). Everything else renders.
- Multi-line blocks (tables, callouts, code, properties, math) render as one widget and switch to source as a whole when the caret enters them. Arrow keys step into a widget the same way as into a line.
- Rendered checkboxes toggle on click. Rendered links and tags act on a plain click; in source lines they need ctrl+click.
- Layouts are cached by line text, width, and options, and only visible blocks draw.
- `LiveEditorSettings::readOnly` gives the reading view (`ctrl+p`). The file browser uses the same editor read-only for its preview pane.

`SlateEditorContext` wraps the editor for the rest of the app. Commands, slash commands, and plugins edit through it, and it commits edits back to `DocumentService` once per frame.

## Plugin views

`SlateExtensionRegistry` also stores views: `Page` views open in `SlateViewMode` (`:view <id>`), and `Sidebar` views show in the right column of the note screen. Lua views draw through the `ui` table in `PluginViewApi.cpp`. That file also holds the `slate.todos` bindings over `TodoService`. A failing view keeps its error and stops redrawing until the plugin reloads; ImGui's error recovery unwinds any panels the script left open.
