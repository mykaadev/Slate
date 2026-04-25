<!-- GH_ONLY_START -->
<h1 align="center">
  <br>
  <a href="https://github.com/mykaadev/Slate">
    <img src="https://github.com/mykaadev/Slate/blob/main/resources/Icon.png" alt="Slate" width="160">
  </a>
</h1>

<h4 align="center">A quiet local-first text editor for Markdown, todos, and daily journaling</h4>

<p align="center">
  <a href="https://github.com/mykaadev/Slate/commits/main"><img src="https://img.shields.io/github/last-commit/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Last Commit"></a>
  <a href="https://github.com/mykaadev/Slate/issues"><img src="https://img.shields.io/github/issues-raw/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Issues"></a>
  <a href="https://github.com/mykaadev/Slate/pulls"><img src="https://img.shields.io/github/issues-pr-raw/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Pull Requests"></a>
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

**Slate** is a lightweight desktop writing space built around local Markdown files.
It is designed for notes, daily journal entries, todo tracking, and calm workspace navigation without accounts, servers, or lock-in.

<table>
  <tr>
    <td><b>local-first</b><br>your workspace is a folder</td>
    <td><b>plain Markdown</b><br>portable, readable, syncable</td>
    <td><b>quiet UI</b><br>fast, focused, keyboard-friendly</td>
  </tr>
</table>

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/LandingPage.png" width="700" alt="Slate landing page" />
</div>

<br>

<table>
  <tr>
    <th align="left">Slate is for</th>
    <th align="left">Slate is not trying to be</th>
  </tr>
  <tr>
    <td>
      calm Markdown writing<br>
      daily notes and reflection<br>
      todos that stay close to context<br>
      fast local workspace navigation<br>
      durable files you can back up yourself
    </td>
    <td>
      a cloud platform<br>
      a locked note database<br>
      a social productivity app<br>
      a giant workspace operating system<br>
      an account-first writing tool
    </td>
  </tr>
</table>

---

## 🎯 Why Slate

Most writing tools drift toward one of two extremes:

| Too small | Too much |
| --- | --- |
| disposable scratchpad | heavy workspace platform |
| little structure | too many systems |
| easy to lose context | hard to simply start writing |

Slate aims for the middle: a quiet Markdown notebook with just enough structure for daily work.

> Open a workspace. Write. Search. Journal. Track todos. Keep your files.

The goal is not to compete with giant note platforms. The goal is to make writing feel immediate, calm, and durable.

---

## ✨ Features

### Writing and navigation

| Feature | What it does |
| --- | --- |
| **Local-first Markdown workspace** | Notes live as regular `.md` files in folders you control. No account, server, or lock-in. |
| **Distraction-free home flow** | Keyboard-first navigation for writing, browsing, search, todos, workspaces, and settings. |
| **Markdown editing + preview** | Edit Markdown, open rendered preview, and inspect a heading outline. Supports headings, code blocks, links, images, tags, and inline styling. |
| **File tree + library views** | Browse a full workspace tree, move through nested folders, and keep journal content separate from broader notes when useful. |
| **Fast workspace search** | Search file names, note text, and recent files with quick-open style navigation. |

### Journaling and todos

| Feature | What it does |
| --- | --- |
| **Daily journal mode** | Jump straight into today’s entry. Daily notes are created under `Journal/YYYY/MM/YYYY-MM-DD.md`. |
| **Todo blocks inside Markdown** | Todos stay inside notes instead of a separate database. States: `Open`, `Research`, `Doing`, `Done`. |
| **Workspace todo overlay** | Collect, filter, edit, cycle, and delete todos across the workspace without leaving the writing flow. |

### Workspace safety

| Feature | What it does |
| --- | --- |
| **Multiple workspaces** | Create, register, and switch between local workspaces. |
| **Wiki links and Markdown links** | Supports `[[Wiki Links]]` and standard Markdown links. Rename/move operations can update internal links. |
| **Recovery and history** | Detects newer recovery data, warns about external file changes, and writes local history snapshots for safer file operations. |
| **Image-friendly notes** | Renders Markdown images, supports dropped image files, and supports clipboard image paste on Windows. |
| **Theme and editor presets** | Separate shell and Markdown appearance presets, with focused settings for writing ergonomics. |
| **Portable packaging** | Includes a CMake target for producing a portable Windows zip. |

---

## 🚀 Quick Start

| Step | Action |
| ---: | --- |
| 1 | Launch Slate. |
| 2 | Create a workspace or open an existing folder. |
| 3 | Start writing normal Markdown files. |
| 4 | Press the journal shortcut to open today’s note. |
| 5 | Use search, the file tree, and the todo overlay to move around your workspace. |

### Default home shortcuts

> Shortcuts are bindable from the settings/config screen. The table below shows the default intent.

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

### Command mode

Slate also supports a small command flow:

| Command | Example |
| --- | --- |
| Today | `:today` |
| Open note | `:open Notes/Ideas.md` |
| Todos | `:todos` |
| Search | `:search renderer` |
| Workspaces | `:workspaces` |

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

- 
```

Slate also builds a month summary and includes a rotating reflection prompt for the active day.

### Todo blocks

Slate treats todos as part of your Markdown workflow. Create one from the editor with `/todo`.

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/todos.png" width="700" alt="Slate todo overlay" />
</div>

```md
#todo [Open] Finish README polish
  tighten summary section
  add screenshot
```

| Todo state flow |
| --- |
| `Open -> Research -> Doing -> Done` |

Use the todo overlay to collect, search, edit, cycle, or delete todo blocks from across the workspace.

### File tree and search

Slate is built around folder-based workspaces, so the tree and search flow are central:

| Flow | Use it for |
| --- | --- |
| **File tree** | moving through nested folders, opening notes, managing workspace structure |
| **Library view** | browsing non-journal notes without daily entries getting in the way |
| **Search overlay** | jumping to files, matching note text, and reopening recent documents |

---

## 🧩 Configuration

Slate keeps configuration plain, small, and separated by responsibility.

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/settings.png" width="700" alt="Slate settings view" />
</div>

### App config

User-level app config is stored under:

```text
%APPDATA%/Slate/config/
```

Current config layout:

```text
%APPDATA%/Slate/config/workspace/settings.tsv
%APPDATA%/Slate/config/input/shortcuts.tsv
%APPDATA%/Slate/config/appearance/theme.tsv
%APPDATA%/Slate/config/editor/editor.tsv
```

Older flat files, such as `%APPDATA%/Slate/shortcuts.tsv`, may still be read as fallback migration paths, but new saves go into the categorized config folders.

### Workspace model

A workspace is just a normal folder containing Markdown files. Slate may also create local workspace data for history, recovery, and workspace-owned presets.

```text
MyWorkspace/
  Journal/
  Notes/
  Projects/
  .slate/
```

### Shortcut bindings

All visible helper text should come from the active shortcut bindings, not hardcoded key strings.

| Action | Behavior |
| --- | --- |
| Open settings | go to the shortcuts section |
| Select shortcut row | press `Enter` |
| Bind shortcut | press the new key chord in the capture popup |
| Cancel capture | press `Esc` |

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

| Preset type | Built-in presets | Workspace-specific overrides |
| --- | --- | --- |
| Shell | `assets/slate/presets/shell/` | `.slate/presets/shell/` |
| Markdown | `assets/slate/presets/markdown/` | `.slate/presets/markdown/` |

---

## 🧠 How It Works

| Area | Details |
| --- | --- |
| **Workspace scanning** | Slate opens a folder, indexes Markdown files, and builds a tree for navigation, search, and recent-file history. |
| **Search modes** | Search can target document names, full note text, and recent files. |
| **Journal structure** | Today’s journal note is resolved from the current local date and written to a predictable folder layout. |
| **Markdown model** | Slate parses headings, Markdown links, wiki links, tags, and todo tickets. That powers preview, outline, search snippets, link rewriting, and todo overlays. |
| **Rename and move safety** | When a note is renamed or moved, Slate can build a rewrite plan and update matching internal Markdown/wiki links across the workspace. |
| **Recovery and history** | Slate keeps local recovery files and history snapshots under `.slate/`. If a newer recovery copy exists, the app can offer to restore it. |
| **External file awareness** | If a file changes outside Slate, the editor can detect that state and warn instead of silently overwriting work. |
| **Modular app structure** | Runtime behavior is moving toward modules, services, commands, and overlays so each feature owns its own responsibilities. |

---

## 📚 Best Practices

| Practice | Why |
| --- | --- |
| Keep one workspace per real context. | Example: personal notes, studio notes, or project notes. |
| Let journals stay in `Journal/`. | Predictable daily notes make search, sync, and backup easier. |
| Use links aggressively. | `[[Wiki Links]]` and Markdown links turn plain files into a useful knowledge graph. |
| Keep todos close to context. | Slate works best when tasks stay attached to the notes that explain them. |
| Treat themes as writing ergonomics. | A calm shell preset plus a readable Markdown preset changes the whole feel of the app. |
| Sync the workspace folder, not the app. | Use Syncthing, Git, cloud folders, or manual backups. Your files stay yours. |

---

## ⚙️ Requirements

| Requirement | Notes |
| --- | --- |
| **CMake 3.15+** | First configure/build fetches third-party dependencies such as GLFW, ImGui, and nlohmann/json. |
| **C++17 compiler** | Required for the core application code. |
| **OpenGL-capable desktop environment** | Needed by the current desktop renderer. |
| **Windows primary target** | Windows builds also pull Scintilla and Lexilla sources during configure. |

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
