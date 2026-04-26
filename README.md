<!-- GH_ONLY_START -->
<h1 align="center">
  <br>
  <a href="https://github.com/mykaadev/Slate">
    <img src="https://github.com/mykaadev/Slate/blob/main/resources/Icon.png" alt="Slate" width="160">
  </a>
</h1>

<h4 align="center">A quiet local-first writing space for Markdown, todos, and daily journaling</h4>

<p align="center">
  <a href="https://github.com/mykaadev/Slate/commits/main"><img src="https://img.shields.io/github/last-commit/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Last Commit"></a>
  <a href="https://github.com/mykaadev/Slate/issues"><img src="https://img.shields.io/github/issues-raw/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Issues"></a>
  <a href="https://github.com/mykaadev/Slate/pulls"><img src="https://img.shields.io/github/issues-pr-raw/mykaadev/Slate?style=plastic&logo=github" alt="GitHub Pull Requests"></a>
  <a href="https://github.com/mykaadev/Slate"><img src="https://img.shields.io/github/stars/mykaadev/Slate?style=plastic&logo=github" alt="GitHub Stars"></a>
  <a href="https://twitter.com/mykaadev/"><img src="https://img.shields.io/twitter/follow/mykaadev?style=plastic&logo=x" alt="Twitter Follow"></a>
</p>

<p align="center">
  <a href="#-summary">🌳 Summary</a> •
  <a href="#-why-slate">🎯 Why</a> •
  <a href="#-features">✨ Features</a> •
  <a href="#-quick-start">🚀 Quick Start</a> •
  <a href="#-workflows">🪴 Workflows</a> •
  <a href="#-configuration">🧩 Configuration</a> •
  <a href="#-how-it-works">🧠 How It Works</a> •
  <a href="#-building">🛠️ Building</a>
</p>

<p align="center">
  <a href="https://buymeacoffee.com/mykaadev"><img src="https://www.svgrepo.com/show/476855/coffee-to-go.svg" alt="Coffee" width="50px"></a>
</p>
<p align="center"><b>Buy me a coffee!</b></p>
<!-- GH_ONLY_END -->

---

## 🌳 Summary

**Slate** is a small desktop writing space built around folders and Markdown files.

It is meant to feel quiet: open a workspace, write a note, jump to today’s journal, track a todo, and leave with files that are still yours.

<div align="center">

<table>
  <tr>
    <td align="center"><b>local-first</b><br>your workspace is a folder</td>
    <td align="center"><b>plain Markdown</b><br>readable and portable</td>
    <td align="center"><b>slow-paced</b><br>useful before it is clever</td>
  </tr>
</table>

</div>

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/LandingPage.png" width="700" alt="Slate landing page" />
</div>

<br>

<div align="center">

<table>
  <tr>
    <th align="center">Slate is for</th>
    <th align="center">Slate is not trying to be</th>
  </tr>
  <tr>
    <td>
      calm Markdown writing<br>
      daily notes and reflection<br>
      editable todo documents<br>
      local workspace navigation<br>
      files you can back up yourself
    </td>
    <td>
      a cloud platform<br>
      a locked note database<br>
      a social productivity app<br>
      a plugin marketplace<br>
      an everything workspace
    </td>
  </tr>
</table>

</div>

---

## 🎯 Why Slate

Slate exists because writing tools can become noisy very quickly.

Some are too small to hold context. Others try to become an operating system for your life. Slate tries to stay in the middle: a humble local notebook that grows slowly and keeps its shape.

> Open a workspace. Write. Search. Journal. Track todos. Keep your files.

The goal is not to be an Obsidian clone, a cloud workspace, or a giant knowledge platform. The goal is to make local Markdown feel calm, direct, and durable.

---

## ✨ Features

### Writing and navigation

<div align="center">

| Feature | What it does |
| --- | --- |
| **Local-first Markdown workspace** | Notes live as regular `.md` files in folders you control. |
| **Markdown editing + preview** | Edit Markdown, preview rendered content, and inspect a heading outline. |
| **File tree + library views** | Move through nested folders with a quiet, rail-based tree view. |
| **Workspace search** | Search file names, note text, and recent files. |
| **Inline slash commands** | Type `/todo`, `/table`, `/callout`, `/image`, and more while writing. |
| **Bindable shortcuts** | Helper text follows the active shortcut bindings. |

</div>

### Journaling and todos

<div align="center">

| Feature | What it does |
| --- | --- |
| **Daily journal mode** | Opens today’s entry under `Journal/YYYY/MM/YYYY-MM-DD.md`. |
| **Standalone todo files** | Todos are Markdown documents under `Todos/`. |
| **Todo overlay** | Create, open, edit, and delete todos from a focused list. |
| **Todo metadata** | Todo files store state and priority in frontmatter. |

</div>

### Workspace safety and media

<div align="center">

| Feature | What it does |
| --- | --- |
| **Multiple workspaces** | Register and switch between local folders. |
| **Links** | Supports standard Markdown links and `[[Wiki Links]]`. |
| **Recovery and history** | Keeps local recovery/history data for safer edits. |
| **Image storage policies** | Choose note-local attachments, same-folder files, workspace media, linked originals, or embedded images. |
| **Theme and editor presets** | Separate shell and Markdown appearance presets. |
| **Portable packaging** | Includes a CMake portable Windows build target. |

</div>

---

## 🚀 Quick Start

<div align="center">

| Step | Action |
| ---: | --- |
| 1 | Launch Slate. |
| 2 | Create a workspace or open an existing folder. |
| 3 | Start writing Markdown files. |
| 4 | Open today’s journal when you need a daily note. |
| 5 | Use search, the file tree, slash commands, and todos when they help. |

</div>

### Default home shortcuts

Shortcuts are bindable from the settings/config screen. These are the default intents:

<div align="center">

| Default | Action |
| --- | --- |
| `j` | open today’s journal |
| `n` | create a new note |
| `s` or `/` | search |
| `f` | browse files |
| `t` | open todos |
| `c` | open config |
| `w` | switch workspaces |
| `q` | quit |

</div>

### Command mode

Slate also has a small command flow for direct actions.

<div align="center">

| Command | Example |
| --- | --- |
| Today | `:today` |
| Open note | `:open Notes/Ideas.md` |
| Todos | `:todos` |
| Search | `:search renderer` |
| Workspaces | `:workspaces` |

</div>

### Inline slash commands

Slash commands are editor-first. They appear near the caret while typing a slash token, and insert only when you press `Tab`.

<div align="center">

| Slash command | Inserts |
| --- | --- |
| `/todo` | opens the todo creation form |
| `/table` | a simple Markdown table |
| `/callout` | a quoted callout block |
| `/image` | image insertion / placeholder flow |
| `/code` | a fenced code block |
| `/checklist` | a Markdown checklist item |
| `/quote` | a block quote |
| `/divider` | a horizontal divider |
| `/link` | a Markdown link placeholder |

</div>

---

## 🪴 Workflows

### Daily journal

Press the journal shortcut to open or create today’s note.

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/journal.png" width="700" alt="Slate journal view" />
</div>

Daily notes are stored predictably:

```text
Journal/YYYY/MM/YYYY-MM-DD.md
```

New entries start simple:

```md
# YYYY-MM-DD

---
```

When a new journal note is created, the caret is placed after the divider so you can start writing immediately.

### Todo documents

Todos are regular Markdown files. They are not hidden records, and new todos are no longer written into the current note.

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/todos.png" width="700" alt="Slate todo overlay" />
</div>

You can create a todo from the editor with `/todo`, or from the pinned `+ new todo` row in the todo overlay.

```text
Todos/
  finish-readme-polish.md
```

A todo file looks like this:

```md
---
slate: todo
status: Open
priority: Normal
---

# Finish README polish

Tighten the summary section.
Add updated screenshots.
```

<div align="center">

| State flow | Priorities |
| --- | --- |
| `Open -> Research -> Doing -> Done` | `Low -> Normal -> High -> Urgent` |

</div>

The todo overlay stays intentionally small. It shows titles grouped by state and ordered by priority. Open or edit a todo when you want to change details.

### Todo form shortcuts

<div align="center">

| Shortcut | Action |
| --- | --- |
| `Ctrl+S` | save todo |
| `Enter` | insert a new line in the description |
| `Shift+Enter` | insert a new line in the description |
| `Ctrl+Tab` | cycle todo state |
| `Ctrl+P` | cycle todo priority |
| `Esc` | cancel |

</div>

### File tree and search

Slate is built around folder-based workspaces.

<div align="center">

| Flow | Use it for |
| --- | --- |
| **File tree** | moving through nested folders and opening notes |
| **Library view** | browsing non-journal notes |
| **Search overlay** | jumping to files, matching note text, and reopening recent documents |

</div>

### Images and attachments

Slate supports image insertion without forcing every image into one global `assets/` folder.

<div align="center">

| Image storage policy | Behavior |
| --- | --- |
| **Subfolder under note folder** | default; stores images in `attachments/` near the current note |
| **Same folder as note** | stores images beside the Markdown file |
| **Workspace media folder** | legacy centralized media behavior |
| **Link original** | inserts a link without copying the image |
| **Embed in Markdown** | stores image data inside the `.md` file |

</div>

Example default layout:

```text
Notes/
  Combat.md
  attachments/
    screenshot.png
```

If a filename already exists, Slate keeps the name readable and appends a suffix like `screenshot_2.png`.

---

## 🧩 Configuration

Slate keeps configuration plain, small, and separated by responsibility.

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/settings.png" width="700" alt="Slate settings view" />
</div>

### App config

User-level config is stored under:

```text
%APPDATA%/Slate/config/
```

```text
%APPDATA%/Slate/config/workspace/settings.tsv
%APPDATA%/Slate/config/input/shortcuts.tsv
%APPDATA%/Slate/config/appearance/theme.tsv
%APPDATA%/Slate/config/editor/editor.tsv
```

Older flat files, such as `%APPDATA%/Slate/shortcuts.tsv`, may still be read as migration fallbacks. New saves go into the categorized config folders.

### Workspace model

A workspace is just a folder.

```text
MyWorkspace/
  Journal/
  Notes/
  Projects/
  Todos/
  .slate/
```

Slate may use `.slate/` for local history, recovery, and workspace-owned presets.

### Shortcut bindings

Visible helper text should come from the active bindings, not hardcoded key strings.

<div align="center">

| Action | Behavior |
| --- | --- |
| Open settings | go to the shortcuts section |
| Select shortcut row | press `Enter` |
| Bind shortcut | press the new key chord in the capture popup |
| Cancel capture | press `Esc` |

</div>

Default shortcut declarations live in:

```text
src/App/Slate/Input/ShortcutService.cpp
```

Look for:

```cpp
ShortcutService::RegisterDefaults()
```

### Theme presets

Slate separates the app shell theme from Markdown rendering colors.

<div align="center">

| Preset type | Built-in presets | Workspace-specific overrides |
| --- | --- | --- |
| Shell | `assets/slate/presets/shell/` | `.slate/presets/shell/` |
| Markdown | `assets/slate/presets/markdown/` | `.slate/presets/markdown/` |

</div>

---

## 🧠 How It Works

Slate is intentionally simple at the file level.

<div align="center">

| Area | Details |
| --- | --- |
| **Workspace scanning** | Opens a folder, indexes Markdown files, and builds navigation data. |
| **Search modes** | Targets document names, full note text, and recent files. |
| **Journal structure** | Resolves today’s note from the local date and writes it to `Journal/`. |
| **Todo model** | Collects todo Markdown files from `Todos/` using frontmatter metadata. |
| **Markdown model** | Parses headings, links, tags, todo metadata, and image targets. |
| **Image handling** | Routes image insertion through a user-selected storage policy. |
| **Rename and move safety** | Can update matching internal Markdown/wiki links. |
| **Recovery and history** | Keeps local recovery files and history snapshots under `.slate/`. |
| **External file awareness** | Warns when a file changes outside Slate. |
| **Modules and services** | Features are being separated into owned modules, services, commands, and overlays. |

</div>

---

## 📚 Best Practices

<div align="center">

| Practice | Why |
| --- | --- |
| Keep one workspace per real context. | Personal notes, studio notes, or project notes stay separate. |
| Let journals stay in `Journal/`. | Predictable daily notes are easier to sync and back up. |
| Let todos stay in `Todos/`. | Todo documents stay editable and searchable. |
| Use links when they help. | `[[Wiki Links]]` and Markdown links connect related notes. |
| Use note-local attachments for most images. | Keeps notes portable without huge Markdown files. |
| Treat themes as writing ergonomics. | Calm colors change the whole feel of the app. |
| Sync the workspace folder, not the app. | Use Git, Syncthing, cloud folders, or manual backups. |

</div>

---

## ⚙️ Requirements

<div align="center">

| Requirement | Notes |
| --- | --- |
| **CMake 3.15+** | First configure/build fetches third-party dependencies. |
| **C++17 compiler** | Required for the core application code. |
| **OpenGL-capable desktop environment** | Needed by the current desktop renderer. |
| **Windows primary target** | Windows builds also pull Scintilla and Lexilla during configure. |

</div>

---

## 🛠️ Building

### Build from source

```bash
cmake -S . -B build
cmake --build build -j
```

On single-config generators, the executable is produced under `build/bin/`. On Visual Studio-style generators, use your selected configuration.

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

### Portable Windows build

```bash
cmake -S . -B build
cmake --build build --config Release --target portable
```

Portable packages are written to:

```text
build/portable/
```

Typical output name:

```text
Slate-0.1.0-windows-portable.zip
```

### Repository layout

```text
assets/
  app/
  slate/
    presets/
resources/
src/
  App/
    Slate/
  Framework/
  Modes/
tests/
cmake/
```

---

<!-- GH_ONLY_START -->
## ❤️ Credits

<a href="https://github.com/mykaadev/Slate/graphs/contributors"><img src="https://contrib.rocks/image?repo=mykaadev/Slate"/></a>
<br>

| GLFW | Dear ImGui | nlohmann/json | Scintilla | Lexilla |
| --- | --- | --- | --- | --- |

## 📞 Support

Reach out via profile page: https://github.com/mykaadev

## 📃 License

[![License](https://img.shields.io/badge/license-MIT-green)](https://www.tldrlegal.com/license/mit-license)
<!-- GH_ONLY_END -->
