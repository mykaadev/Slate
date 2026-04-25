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
