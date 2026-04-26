# Todo documents

Slate todos are stored as standalone Markdown documents instead of inline blocks inside arbitrary notes.

## Storage

New todos are created under the workspace todo folder:

```text
Todos/
  ship-readme-polish.md
  fix-image-preview.md
```

Each todo is still a normal `.md` file, so it can be opened, edited, moved, renamed, linked, searched, and versioned like any other document.

## Format

```md
---
slate: todo
status: Open
priority: Normal
---

# Ship README polish

Tighten summary, add screenshots, and verify install notes.
```

The todo overlay reads:

- `slate: todo` to identify the file as a todo document
- `status` / `state` for the current todo state
- `priority` for the current todo priority
- first `# Heading` as the title
- the remaining body as the description/details

## States and priority

Supported states remain:

```text
Open -> Research -> Doing -> Done
```

Supported priorities are:

```text
Low -> Normal -> High -> Urgent
```

## Legacy inline todos

Old inline `#todo` and `- [ ] #todo` blocks are still parsed by the Markdown renderer for compatibility, but the todo module no longer creates, updates, or deletes todo blocks inside normal notes. Todo mutations now target todo documents only.

## Todo overlay

The todo overlay is a navigation list, not a metadata editor. It shows todo titles grouped by state and sorted by priority inside each state. Use the pinned `+ new todo` entry to create a todo, `Enter` to open the selected todo file, and `e` to open the metadata form. Status and priority changes are made from the edit form or by opening the todo file directly.
