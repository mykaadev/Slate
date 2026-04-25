# Shortcuts

Slate shortcuts are owned by `ShortcutService`.

## Default bindings

Default bindings are declared in:

```text
src/App/Slate/Input/ShortcutService.cpp
```

Look for `ShortcutService::RegisterDefaults()` and edit the `add(...)` calls.

Example:

```cpp
add(ShortcutAction::HomeFiles, "home.files", "home", "Files", Bind(ImGuiKey_F));
```

## User overrides

Runtime edits are made from the settings/config screen: select a shortcut row, press Enter, then press the new key chord in the capture popup. Edits are saved to:

```text
%APPDATA%/Slate/config/input/shortcuts.tsv
```

If `%APPDATA%` is unavailable, Slate falls back to `%USERPROFILE%/Slate/config/input/shortcuts.tsv`, then the current working directory.

## UI rule

Any helper text or visible shortcut label should be generated from `ShortcutService::Label()`, `ShortcutService::Helper()`, or `ShortcutService::HelperLine()` so the UI always matches the active binding.


All app-owned saved configuration lives under `%APPDATA%/Slate/config` on Windows, falling back to the user profile/current directory when needed. Config is separated by responsibility:

```text
%APPDATA%/Slate/config/workspace/settings.tsv
%APPDATA%/Slate/config/input/shortcuts.tsv
%APPDATA%/Slate/config/appearance/theme.tsv
%APPDATA%/Slate/config/editor/editor.tsv
```

Older flat files such as `%APPDATA%/Slate/shortcuts.tsv` are still read as migration fallbacks, but new saves go into the categorized config folders. Default shortcut declarations live in `src/App/Slate/Input/ShortcutService.cpp` inside `ShortcutService::RegisterDefaults()`.
