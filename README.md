<!-- GH_ONLY_START -->
<h1 align="center">
  <br>
  <a href="https://github.com/mykaadev/Slate">
    <img src="https://github.com/mykaadev/Slate/blob/main/resources/Icon.png" alt="Slate" width="160">
  </a>
</h1>

<h4 align="center">A quiet local-first text editor for Markdown, todo lists, and daily journaling</h4>

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
  <a href="#-getting-started">🚀 Getting Started</a> •
  <a href="#-configuration">🧩 Configuration</a> •
  <a href="#-how-it-works">🧠 How it works</a> •
  <a href="#-best-practices">📚 Best practices</a> •
  <a href="#-requirements">⚙️ Requirements</a> •
  <a href="#-installation">🛠️ Installation</a>
</p>

<p align="center">
  <a href="https://buymeacoffee.com/mykaadev"><img src="https://www.svgrepo.com/show/476855/coffee-to-go.svg" alt="Coffee" width="50px"></a>
</p>
<p align="center"><b>Buy me a coffee!</b></p>
<!-- GH_ONLY_END -->

---

## 🌳 Summary

**Slate** is a lightweight desktop text editor built for people who want calm, local, portable writing.  
It uses normal Markdown files inside a workspace folder you already own, with a distraction-free flow for notes, daily journal entries, and todo tracking.

<table>
  <tr>
    <td><b>local-first</b> instead of account-first</td>
    <td><b>plain files</b> instead of a locked database</td>
    <td><b>focused UI</b> instead of a giant workspace operating system</td>
  </tr>
</table>

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/LandingPage.png" width="700" alt="Landing Page" />
</div>

<br>

<table>
  <tr>
    <th align="left">Default idea</th>
    <th align="left">File ownership</th>
  </tr>
  <tr>
    <td>
      write notes<br>
      open today’s journal fast<br>
      keep todos close to the notes they belong to<br>
      stay out of the way
    </td>
    <td>
      <b>Your files stay yours:</b> Slate stores notes as regular <code>.md</code> files in a normal workspace folder.<br><br>
      No account is required, and sync can be handled however the user wants.
    </td>
  </tr>
</table>

---

## 🎯 Why Slate

A lot of writing tools split in two directions:

| Too small | Too much |
| --- | --- |
| too bare and disposable | too heavy and overbuilt |

Slate aims for the middle.

It is meant to feel like:

| Slate should feel like... |
| --- |
| a quiet writing space |
| a dependable Markdown notebook |
| a light todo layer |
| a journaling tool that does not turn into a productivity circus |

The goal is not to compete with giant note platforms.  
The goal is to make opening a workspace and writing feel immediate, calm, and durable.

---

## ✨ Features

| Feature | What it does |
| --- | --- |
| **Local-first Markdown workspace** | Notes live as regular `.md` files in folders you control.<br>No server, no account, no lock-in. |
| **Distraction-free home flow** | Fast keyboard-first navigation for writing, browsing, search, and settings.<br>Built around a quiet full-screen desktop workflow. |
| **Daily journal mode** | Jump straight into today’s entry.<br>Daily notes are created automatically under:<br><br>`Journal/YYYY/MM/YYYY-MM-DD.md` |
| **Markdown editing + preview** | Write in the editor, open a rendered preview, and inspect a heading outline.<br>Supports headings, code blocks, links, images, tags, and inline styling. |
| **Todo blocks inside Markdown** | Todos stay in your notes instead of a separate app.<br>Built-in todo states: `Open`, `Research`, `Doing`, `Done`.<br>Workspace todo overlay lets you filter and search across notes. |
| **Fast workspace search** | Search by file names, full note text, or recent files.<br>Quick-open style workflow for moving around large note folders. |
| **Multiple workspaces** | Create, register, and switch between local workspaces.<br>Each workspace can have its own title, emoji, theme, and editor settings. |
| **File tree + library views** | Browse the full workspace tree.<br>Keep journal content separate from your broader note library when needed. |
| **Wiki links and Markdown links** | Works with both `[[Wiki Links]]` and standard Markdown links.<br>Renaming or moving notes can update internal links across the workspace. |
| **Recovery, history, and external-change awareness** | Detects newer recovery data.<br>Warns when a note changed on disk outside the editor.<br>Writes local history snapshots for safer file operations. |
| **Theme presets** | Separate shell and Markdown appearance presets.<br>Workspace-level settings make it easy to keep a preferred writing environment. |
| **Image-friendly note workflow** | Markdown image rendering in preview.<br>Supports inserting dropped image files.<br>Clipboard image paste is available on Windows. |
| **Portable packaging** | Includes a terminal target for building a portable Windows zip. |

---

## 🚀 Getting Started

| Step | Action |
| ---: | --- |
| 1 | Launch Slate. |
| 2 | Create a workspace or open an existing one. |
| 3 | Start writing plain Markdown files in a normal folder. |
| 4 | Use journal mode for daily notes. |
| 5 | Use the todo overlay when you need to collect open work across the workspace. |

### Home actions

| Key | Action |
| --- | --- |
| `j` | open today’s journal |
| `n` | create a new note |
| `s` or `/` | search |
| `f` | browse files |
| `t` | open todos |
| `c` | open config |
| `w` | switch workspaces |
| `?` | help |
| `q` | quit |

### Command mode

Slate also supports a small command flow.

| Command | Example |
| --- | --- |
| Today | `:today` |
| Open | `:open Notes/Ideas.md` |
| New note | `:new` |
| New folder | `:mkdir` |
| Todos | `:todos` |
| Search | `:search renderer` |
| Workspaces | `:workspaces` |
| Write | `:w` |
| Write and quit | `:wq` |

### Journal flow

Press `j` to open or create today’s daily note.

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/journal.png" width="700" alt="Landing Page" />
</div>

New daily entries start with:

```md
# YYYY-MM-DD

- 
```

Slate also builds a month summary for journaling and includes a rotating reflection prompt for the active day.

### Todo flow

Slate treats todos as part of your Markdown workflow.  
A todo block looks like this:

Easily created by using `/todo`

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/todos.png" width="700" alt="Landing Page" />
</div>

```md
#todo [Open] Finish README polish
  tighten summary section
  add screenshot
```

| Todo states |
| --- |
| `Open -> Research -> Doing -> Done` |

You can collect, filter, and edit todo blocks from across the workspace without leaving the writing flow.

---

## 🧩 Configuration

Slate keeps configuration intentionally small and workspace-friendly.

### 1) Workspace model

A workspace is just a normal folder containing your Markdown files plus a `.slate/` directory for local app data.

| Typical contents |
| --- |
| `Journal/` |
| `Notes/` |
| `Projects/` |
| `.slate/` |

### 2) Workspace settings

Slate stores per-workspace settings in plain files:

```text
.slate/editor.tsv
.slate/theme.tsv
```

<div align="center">
  <img src="https://github.com/mykaadev/Slate/blob/main/resources/settings.png" width="700" alt="Landing Page" />
</div>

These cover things like:

| Editor | Layout | Writing flow |
| --- | --- | --- |
| font size | page width | preview sync behavior |
| line spacing | panel layout | caret feel |
| word wrap | panel and scroll motion | link underline |
| whitespace visibility | tab width | smart list continuation |
| current-line highlight |  | clipboard image paste |

### 3) Theme presets

Slate separates the app shell theme from Markdown rendering colors.

| Preset type | Built-in presets | Workspace-specific preset overrides |
| --- | --- | --- |
| Shell | `assets/slate/presets/shell/` | `.slate/presets/shell/` |
| Markdown | `assets/slate/presets/markdown/` | `.slate/presets/markdown/` |

### 4) Workspace registry

Your saved workspace list is stored separately from note content, so you can keep multiple local workspaces and switch between them easily.

### 5) File ownership

Slate is built around normal files and folders.  
That means your backup and sync strategy stays your choice:

| Backup and sync options |
| --- |
| Syncthing |
| cloud-synced folders |
| Git |
| manual backup |
| whatever works for you |

---

## 🧠 How it works

| Area | Details |
| --- | --- |
| **Workspace scanning** | Slate opens a folder, indexes Markdown files, and keeps a workspace tree for navigation, search, and recent-file history. |
| **Search modes** | Search can target document names, full note text, and recent files.<br>That keeps quick lookup lightweight without turning the app into a database-first system. |
| **Journal structure** | Today’s journal note is resolved automatically from the current local date and placed into a predictable folder layout. |
| **Markdown model** | Slate parses headings, Markdown links, wiki links, tags, and todo tickets.<br>That powers the preview, outline, search snippets, link rewriting, and todo overlays. |
| **Rename and move safety** | When a note is renamed or moved, Slate can build a rewrite plan and update matching internal Markdown and wiki links across the workspace. |
| **Recovery and history** | Slate keeps local recovery files and history snapshots under `.slate/`.<br>If a newer recovery copy exists, the app can offer to restore it. |
| **External file awareness** | If a file changes outside Slate, the editor can detect that state and warn instead of silently pretending everything is fine. |

---

## 📚 Best practices

| Practice | Why |
| --- | --- |
| Keep one workspace per real context. | Example: personal notes, studio notes, project notes. |
| Let journals stay in `Journal/`. | Keeping daily notes predictable makes backup, browsing, and future sync much easier. |
| Use links aggressively. | `[[Wiki Links]]` and Markdown links both help turn plain files into a usable knowledge graph without extra ceremony. |
| Keep todos close to the note they belong to. | Slate works best when tasks stay attached to context instead of drifting into a disconnected task silo. |
| Treat themes as writing ergonomics, not decoration. | A calm shell preset plus a readable Markdown preset changes the whole feel of the app. |
| Sync the workspace folder, not the app. | Slate is happiest when sync stays file-based. |

---

## ⚙️ Requirements

| Requirement | Notes |
| --- | --- |
| **CMake 3.15+** | First configure/build will fetch third-party dependencies such as GLFW, ImGui, and nlohmann/json. |
| **C++17 compiler** |  |
| **OpenGL-capable desktop environment** |  |
| **Windows is the primary target right now** | Windows builds also pull Scintilla and Lexilla sources during configure. |

---

## 🛠️ Installation

### Build from source

```bash
cmake -S . -B build
cmake --build build -j
```

On single-config generators, the executable is produced under `build/bin/`.  
On Visual Studio-style generators, use your selected configuration.

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
src/
  App/
  Features/
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

| GLFW |  Dear ImGui | nlohmann/json | Scintilla | Lexilla |
| --- | --- | --- | --- | --- |

## 📞 Support

Reach out via profile page: https://github.com/mykaadev

## 📃 License

[![License](https://img.shields.io/badge/license-MIT-green)](https://www.tldrlegal.com/license/mit-license)
<!-- GH_ONLY_END -->
