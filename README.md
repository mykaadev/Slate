<!-- GH_ONLY_START -->
<h1 align="center">
  <br>
  Slate
</h1>

<h4 align="center">A quiet local-first text editor for Markdown, todo lists, and daily journaling</h4>

<div align="center">
    <a href="https://github.com/mykaadev/Slate/commits/main"><img src="https://img.shields.io/github/last-commit/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Last Commit"></a>
    <a href="https://github.com/mykaadev/Slate/issues"><img src="https://img.shields.io/github/issues-raw/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Issues"></a>
    <a href="https://github.com/mykaadev/Slate/pulls"><img src="https://img.shields.io/github/issues-pr-raw/mykaadev/Slate?style=plastic&logo=github&logoColor=white" alt="GitHub Pull Requests"></a>
    <a href="https://github.com/mykaadev/Slate"><img src="https://img.shields.io/github/stars/mykaadev/Slate?style=plastic&logo=github" alt="GitHub Stars"></a>
    <a href="https://twitter.com/mykaadev/"><img src="https://img.shields.io/twitter/follow/mykaadev?style=plastic&logo=x" alt="Twitter Follow"></a>

<p style="display:none;">
  <a href="#-summary">🌳 Summary</a> •
  <a href="#-why-slate">🎯 Why</a> •
  <a href="#-features">✨ Features</a> •
  <a href="#-getting-started">🚀 Getting Started</a> •
  <a href="#-configuration">🧩 Configuration</a> •
  <a href="#-how-it-works">🧠 How it works</a> •
  <a href="#-best-practices">📚 Best practices</a> •
  <a href="#-requirements">⚙️ Requirements</a> •
  <a href="#-installation">🛠️ Installation</a> •
  <a href="#-credits">❤️ Credits</a> •
  <a href="#-support">📞 Support</a> •
  <a href="#-license">📃 License</a>
</p>

<a href="https://buymeacoffee.com/mykaadev"><img src="https://www.svgrepo.com/show/476855/coffee-to-go.svg" alt="Coffee" width="50px"></a>
<p><b>Buy me a coffee!</b></p>
</div>
<!-- GH_ONLY_END -->

## 🌳 Summary
**Slate** is a lightweight desktop text editor built for people who want calm, local, portable writing.
It uses normal Markdown files inside a workspace folder you already own, with a distraction-free flow for notes, daily journal entries, and todo tracking.

Slate is intentionally small:
- **local-first** instead of account-first,
- **plain files** instead of a locked database,
- **focused UI** instead of a giant workspace operating system.

Default idea:
- write notes,
- open today’s journal fast,
- keep todos close to the notes they belong to,
- stay out of the way.

> **Your files stay yours:** Slate stores notes as regular `.md` files in a normal workspace folder.
> No account is required, and sync can be handled however the user wants.

## 🎯 Why Slate
A lot of writing tools split in two directions:
- too bare and disposable,
- or too heavy and overbuilt.

Slate aims for the middle.

It is meant to feel like:
- a quiet writing space,
- a dependable Markdown notebook,
- a light todo layer,
- and a journaling tool that does not turn into a productivity circus.

The goal is not to compete with giant note platforms.
The goal is to make opening a workspace and writing feel immediate, calm, and durable.

## ✨ Features
- **Local-first Markdown workspace**
  - Notes live as regular `.md` files in folders you control.
  - No server, no account, no lock-in.

- **Distraction-free home flow**
  - Fast keyboard-first navigation for writing, browsing, search, and settings.
  - Built around a quiet full-screen desktop workflow.

- **Daily journal mode**
  - Jump straight into today’s entry.
  - Daily notes are created automatically under:

  `Journal/YYYY/MM/YYYY-MM-DD.md`

- **Markdown editing + preview**
  - Write in the editor, open a rendered preview, and inspect a heading outline.
  - Supports headings, code blocks, links, images, tags, and inline styling.

- **Todo blocks inside Markdown**
  - Todos stay in your notes instead of a separate app.
  - Built-in todo states:
    - `Open`
    - `Research`
    - `Doing`
    - `Done`
  - Workspace todo overlay lets you filter and search across notes.

- **Fast workspace search**
  - Search by file names, full note text, or recent files.
  - Quick-open style workflow for moving around large note folders.

- **Multiple workspaces**
  - Create, register, and switch between local workspaces.
  - Each workspace can have its own title, emoji, theme, and editor settings.

- **File tree + library views**
  - Browse the full workspace tree.
  - Keep journal content separate from your broader note library when needed.

- **Wiki links and Markdown links**
  - Works with both `[[Wiki Links]]` and standard Markdown links.
  - Renaming or moving notes can update internal links across the workspace.

- **Recovery, history, and external-change awareness**
  - Detects newer recovery data.
  - Warns when a note changed on disk outside the editor.
  - Writes local history snapshots for safer file operations.

- **Theme presets**
  - Separate shell and Markdown appearance presets.
  - Workspace-level settings make it easy to keep a preferred writing environment.

- **Image-friendly note workflow**
  - Markdown image rendering in preview.
  - Supports inserting dropped image files.
  - Clipboard image paste is available on Windows.

- **Portable packaging**
  - Includes a terminal target for building a portable Windows zip.

## 🚀 Getting Started
1. Launch Slate.
2. Create a workspace or open an existing one.
3. Start writing plain Markdown files in a normal folder.
4. Use journal mode for daily notes.
5. Use the todo overlay when you need to collect open work across the workspace.

### Home actions
- `j` — open today’s journal
- `n` — create a new note
- `s` or `/` — search
- `f` — browse files
- `t` — open todos
- `c` — open config
- `w` — switch workspaces
- `?` — help
- `q` — quit

### Command mode
Slate also supports a small command flow.
Examples:

```text
:today
:open Notes/Ideas.md
:new
:mkdir
:todos
:search renderer
:workspaces
:w
:wq
```

### Journal flow
Press `j` to open or create today’s daily note.

New daily entries start with:

```md
# YYYY-MM-DD

- 
```

Slate also builds a month summary for journaling and includes a rotating reflection prompt for the active day.

### Todo flow
Slate treats todos as part of your Markdown workflow.
A todo block looks like this:

```md
- [ ] #todo [Open] Finish README polish
  tighten summary section
  add screenshot
```

States can move through:

`Open -> Research -> Doing -> Done`

You can collect, filter, and edit todo blocks from across the workspace without leaving the writing flow.

## 🧩 Configuration
Slate keeps configuration intentionally small and workspace-friendly.

### 1) Workspace model
A workspace is just a normal folder containing your Markdown files plus a `.slate/` directory for local app data.

Typical contents:

```text
My Workspace/
  Journal/
  Notes/
  Projects/
  .slate/
```

### 2) Workspace settings
Slate stores per-workspace settings in plain files:

```text
.slate/editor.tsv
.slate/theme.tsv
```

These cover things like:
- font size
- line spacing
- page width
- word wrap
- whitespace visibility
- current-line highlight
- tab width
- panel layout
- preview sync behavior
- panel and scroll motion
- caret feel
- link underline
- smart list continuation
- clipboard image paste

### 3) Theme presets
Slate separates the app shell theme from Markdown rendering colors.

Built-in presets live under:

```text
assets/slate/presets/shell/
assets/slate/presets/markdown/
```

Workspace-specific preset overrides can also live under:

```text
.slate/presets/shell/
.slate/presets/markdown/
```

### 4) Workspace registry
Your saved workspace list is stored separately from note content, so you can keep multiple local workspaces and switch between them easily.

### 5) File ownership
Slate is built around normal files and folders.
That means your backup and sync strategy stays your choice:
- Syncthing
- cloud-synced folders
- Git
- manual backup
- whatever works for you

## 🧠 How it works
### Workspace scanning
Slate opens a folder, indexes Markdown files, and keeps a workspace tree for navigation, search, and recent-file history.

### Search modes
Search can target:
- document names,
- full note text,
- recent files.

That keeps quick lookup lightweight without turning the app into a database-first system.

### Journal structure
Today’s journal note is resolved automatically from the current local date and placed into a predictable folder layout.

### Markdown model
Slate parses:
- headings,
- Markdown links,
- wiki links,
- tags,
- todo tickets.

That powers the preview, outline, search snippets, link rewriting, and todo overlays.

### Rename and move safety
When a note is renamed or moved, Slate can build a rewrite plan and update matching internal Markdown and wiki links across the workspace.

### Recovery and history
Slate keeps local recovery files and history snapshots under `.slate/`.
If a newer recovery copy exists, the app can offer to restore it.

### External file awareness
If a file changes outside Slate, the editor can detect that state and warn instead of silently pretending everything is fine.

## 📚 Best practices
- Keep one workspace per real context.
  - Example: personal notes, studio notes, project notes.

- Let journals stay in `Journal/`.
  - Keeping daily notes predictable makes backup, browsing, and future sync much easier.

- Use links aggressively.
  - `[[Wiki Links]]` and Markdown links both help turn plain files into a usable knowledge graph without extra ceremony.

- Keep todos close to the note they belong to.
  - Slate works best when tasks stay attached to context instead of drifting into a disconnected task silo.

- Treat themes as writing ergonomics, not decoration.
  - A calm shell preset plus a readable Markdown preset changes the whole feel of the app.

- Sync the workspace folder, not the app.
  - Slate is happiest when sync stays file-based.

## ⚙️ Requirements
- **CMake 3.15+**
- **C++17 compiler**
- **OpenGL-capable desktop environment**
- **Windows is the primary target right now**

Practical note:
- First configure/build will fetch third-party dependencies such as GLFW, ImGui, and nlohmann/json.
- Windows builds also pull Scintilla and Lexilla sources during configure.

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

<!-- GH_ONLY_START -->
## ❤️ Credits
Built with:
- GLFW
- Dear ImGui
- nlohmann/json
- Scintilla
- Lexilla

## 📞 Support
Reach out via profile page: https://github.com/mykaadev

## 📃 License
[![License](https://img.shields.io/badge/license-MIT-green)](https://www.tldrlegal.com/license/mit-license)
<!-- GH_ONLY_END -->
