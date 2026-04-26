#include "App/Slate/Documents/AssetService.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Editor/EditorDocumentViewModel.h"
#include "App/Slate/Markdown/JournalService.h"
#include "App/Slate/Documents/LinkService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Workspace/SearchService.h"
#include "App/Slate/Todos/TodoService.h"
#include "App/Slate/Workspace/ThemeService.h"
#include "App/Slate/UI/SlateUi.h"
#include "App/Slate/Workspace/WorkspaceService.h"
#include "App/Slate/Workspace/WorkspaceRegistryService.h"
#include "App/Slate/Workspace/WorkspaceTree.h"

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{
#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)

    void Check(bool value, const char* expression, const char* file, int line)
    {
        if (!value)
        {
            std::cerr << file << ":" << line << " check failed: " << expression << "\n";
            std::abort();
        }
    }

    fs::path MakeTempWorkspace()
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        fs::path root = fs::temp_directory_path() / ("slate_core_tests_" + std::to_string(stamp));
        fs::create_directories(root);
        return root;
    }

    void WriteFile(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    void TestWorkspaceScan()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Design.md", "# Design\n");
        WriteFile(root / ".slate" / "ignored.md", "# Ignore\n");
        WriteFile(root / "build" / "ignored.md", "# Ignore\n");

        Software::Slate::WorkspaceService workspace(root);
        CHECK(workspace.MarkdownFiles().size() == 1);
        CHECK(workspace.MarkdownFiles().front() == fs::path("Design.md"));
        fs::remove_all(root);
    }

    void TestDocumentSaveAndRecovery()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Note.md", "# Note\r\nbody\r\n");

        Software::Slate::DocumentService docs;
        docs.SetWorkspaceRoot(root);
        CHECK(docs.Open("Note.md"));
        CHECK(docs.Active());
        CHECK(docs.Active()->lineEnding == "\r\n");
        docs.Active()->text += "more\r\n";
        docs.MarkEdited(1.0);
        CHECK(docs.WriteRecovery());
        CHECK(fs::exists(docs.RecoveryPathFor("Note.md")));
        CHECK(docs.Save());
        CHECK(!fs::exists(docs.RecoveryPathFor("Note.md")));
        CHECK(ReadFile(root / "Note.md").find("more") != std::string::npos);
        CHECK(fs::exists(root / ".slate" / "history"));
        fs::remove_all(root);
    }

    void TestSearch()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "GDD.md", "# Combat\nThe player has a shield.\n");
        WriteFile(root / "Journal" / "2026" / "04" / "2026-04-20.md", "# Day\nquiet writing\n");

        Software::Slate::WorkspaceService workspace(root);
        Software::Slate::SearchService search;
        search.Rebuild(workspace);
        const auto filenameResults = search.Query("shield");
        CHECK(filenameResults.empty());
        const auto combatResults = search.Query("combat");
        CHECK(!combatResults.empty());
        CHECK(combatResults.front().relativePath == fs::path("GDD.md"));
        const auto bodyResults = search.Query("shield", Software::Slate::SearchMode::FullText);
        CHECK(!bodyResults.empty());
        CHECK(bodyResults.front().relativePath == fs::path("GDD.md"));
        CHECK(bodyResults.front().line == 2);

        const auto docResults = Software::Slate::SearchService::QueryDocument("# Combat\nshield and shield\n", "shield");
        CHECK(docResults.size() == 2);
        CHECK(docResults.front().line == 2);
        CHECK(docResults.front().column == 1);
        fs::remove_all(root);
    }

    void TestLinksRewrite()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Design.md", "# Design\r\nSee [Combat](Combat.md) and [[Combat]].\r\n");
        WriteFile(root / "Combat.md", "# Combat\n");

        Software::Slate::WorkspaceService workspace(root);
        Software::Slate::LinkService links;
        auto plan = links.BuildRenamePlan(workspace, "Combat.md", "Docs/Combat.md");
        CHECK(plan.TotalReplacements() == 2);
        CHECK(links.ApplyRewritePlan(workspace, plan));
        CHECK(workspace.RenameOrMove("Combat.md", "Docs/Combat.md"));
        const std::string design = ReadFile(root / "Design.md");
        CHECK(design.find("(Docs/Combat.md)") != std::string::npos);
        CHECK(design.find("[[Docs/Combat]]") != std::string::npos);
        fs::remove_all(root);
    }

    void TestDailyAndAssets()
    {
        const fs::path root = MakeTempWorkspace();
        Software::Slate::WorkspaceService workspace(root);
        fs::path daily;
        CHECK(workspace.EnsureDailyNote(&daily));
        CHECK(daily.generic_string().find("Journal/") == 0);
        CHECK(fs::exists(root / daily));

        WriteFile(root / "source.png", "not really an image, but extension copy is enough for path handling");
        Software::Slate::AssetService assets(root);
        fs::path asset;
        CHECK(assets.CopyImageAsset(daily, root / "source.png", &asset));
        CHECK(asset.generic_string().find("assets/") == 0);
        CHECK(Software::Slate::AssetService::MarkdownImageLink(daily, asset).find("![](") == 0);
        fs::remove_all(root);
    }

    void TestCollisionSafeNotePaths()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Docs" / "Pitch.md", "# Pitch\n");
        WriteFile(root / "Docs" / "Pitch-2.md", "# Pitch 2\n");

        Software::Slate::WorkspaceService workspace(root);
        CHECK(workspace.MakeCollisionSafeMarkdownPath("Docs", "Pitch.md") == fs::path("Docs/Pitch-3.md"));
        CHECK(workspace.MakeCollisionSafeMarkdownPath("Docs", "Combat") == fs::path("Docs/Combat.md"));

        fs::path created;
        CHECK(workspace.CreateMarkdownFile(workspace.MakeCollisionSafeMarkdownPath("Docs", "Pitch.md"), "# New\n",
                                           &created));
        CHECK(created == fs::path("Docs/Pitch-3.md"));
        fs::remove_all(root);
    }

    void TestWorkspaceFolderOperations()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Docs" / "Systems" / "Combat.md", "# Combat\n");

        Software::Slate::WorkspaceService workspace(root);
        fs::path createdFolder;
        CHECK(workspace.CreateFolder("Docs", "Systems", &createdFolder));
        CHECK(createdFolder == fs::path("Docs/Systems-2"));
        CHECK(fs::exists(root / createdFolder));
        CHECK(workspace.MakeCollisionSafeFolderPath("Docs", "Systems") == fs::path("Docs/Systems-3"));

        CHECK(workspace.CreateMarkdownFile(workspace.MakeCollisionSafeMarkdownPath(createdFolder, "AI.md"), "# AI\n", nullptr));
        CHECK(workspace.RenameOrMove(createdFolder, "Docs/AI Systems"));
        CHECK(fs::exists(root / "Docs" / "AI Systems" / "AI.md"));
        CHECK(!fs::exists(root / createdFolder));

        CHECK(workspace.DeletePath("Docs/AI Systems/AI.md"));
        CHECK(!fs::exists(root / "Docs" / "AI Systems" / "AI.md"));
        CHECK(workspace.DeletePath("Docs/AI Systems"));
        CHECK(!fs::exists(root / "Docs" / "AI Systems"));
        fs::remove_all(root);
    }

    void TestTreeFilteringPreservesParents()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Docs" / "Systems" / "Combat.md", "# Combat\n");
        WriteFile(root / "Docs" / "Story.md", "# Story\n");
        WriteFile(root / "Journal" / "2026.md", "# Day\n");

        Software::Slate::WorkspaceService workspace(root);
        Software::Slate::CollapsedFolderSet collapsed;
        const auto rows =
            Software::Slate::BuildTreeRows(workspace.Entries(), collapsed, "Combat", Software::Slate::TreeViewMode::Files);

        bool sawDocs = false;
        bool sawSystems = false;
        bool sawCombat = false;
        for (const auto& row : rows)
        {
            sawDocs = sawDocs || row.relativePath == fs::path("Docs");
            sawSystems = sawSystems || row.relativePath == fs::path("Docs/Systems");
            sawCombat = sawCombat || row.relativePath == fs::path("Docs/Systems/Combat.md");
        }
        CHECK(sawDocs);
        CHECK(sawSystems);
        CHECK(sawCombat);
        fs::remove_all(root);
    }

    void TestCollapsedTreeHidesDescendants()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Root.md", "# Root\n");
        WriteFile(root / "Docs" / "Systems" / "Combat.md", "# Combat\n");
        WriteFile(root / "Journal" / "2026.md", "# Day\n");

        Software::Slate::WorkspaceService workspace(root);
        Software::Slate::CollapsedFolderSet collapsed;
        for (const auto& entry : workspace.Entries())
        {
            if (entry.isDirectory)
            {
                collapsed.insert(Software::Slate::TreePathKey(entry.relativePath));
            }
        }

        const auto rows =
            Software::Slate::BuildTreeRows(workspace.Entries(), collapsed, "", Software::Slate::TreeViewMode::Files);

        bool sawRoot = false;
        bool sawDocs = false;
        bool sawJournal = false;
        bool sawSystems = false;
        bool sawCombat = false;
        for (const auto& row : rows)
        {
            sawRoot = sawRoot || row.relativePath == fs::path("Root.md");
            sawDocs = sawDocs || row.relativePath == fs::path("Docs");
            sawJournal = sawJournal || row.relativePath == fs::path("Journal");
            sawSystems = sawSystems || row.relativePath == fs::path("Docs/Systems");
            sawCombat = sawCombat || row.relativePath == fs::path("Docs/Systems/Combat.md");
        }

        CHECK(sawRoot);
        CHECK(sawDocs);
        CHECK(sawJournal);
        CHECK(!sawSystems);
        CHECK(!sawCombat);
        fs::remove_all(root);
    }

    void TestLibraryTreeExcludesJournalAndPreservesParents()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Root.md", "# Root\n");
        WriteFile(root / "Docs" / "Systems" / "Combat.md", "# Combat\n");
        WriteFile(root / "Docs" / "Pitch.txt", "not markdown\n");
        WriteFile(root / "Journal" / "2026" / "04" / "2026-04-21.md", "# Day\n");

        Software::Slate::WorkspaceService workspace(root);
        Software::Slate::CollapsedFolderSet collapsed;

        const auto rows =
            Software::Slate::BuildTreeRows(workspace.Entries(), collapsed, "combat", Software::Slate::TreeViewMode::Library);

        bool sawRoot = false;
        bool sawDocs = false;
        bool sawSystems = false;
        bool sawCombat = false;
        bool sawJournal = false;
        bool sawPitch = false;
        for (const auto& row : rows)
        {
            sawRoot = sawRoot || row.relativePath == fs::path("Root.md");
            sawDocs = sawDocs || row.relativePath == fs::path("Docs");
            sawSystems = sawSystems || row.relativePath == fs::path("Docs/Systems");
            sawCombat = sawCombat || row.relativePath == fs::path("Docs/Systems/Combat.md");
            sawJournal = sawJournal || row.relativePath.generic_string().rfind("Journal", 0) == 0;
            sawPitch = sawPitch || row.relativePath == fs::path("Docs/Pitch.txt");
        }

        CHECK(!sawRoot);
        CHECK(sawDocs);
        CHECK(sawSystems);
        CHECK(sawCombat);
        CHECK(!sawJournal);
        CHECK(!sawPitch);

        const auto unfiltered =
            Software::Slate::BuildTreeRows(workspace.Entries(), collapsed, "", Software::Slate::TreeViewMode::Library);
        sawRoot = false;
        sawJournal = false;
        for (const auto& row : unfiltered)
        {
            sawRoot = sawRoot || row.relativePath == fs::path("Root.md");
            sawJournal = sawJournal || row.relativePath.generic_string().rfind("Journal", 0) == 0;
        }
        CHECK(sawRoot);
        CHECK(!sawJournal);
        fs::remove_all(root);
    }

    void TestJournalMonthSummary()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / "Journal" / "2026" / "04" / "2026-04-01.md", "# 2026-04-01\n\n- ");
        WriteFile(root / "Journal" / "2026" / "04" / "2026-04-02.md", "# 2026-04-02\n\n- Walked outside\n");
        WriteFile(root / "Journal" / "2026" / "04" / "2026-04-03.md", "# 2026-04-03\n\n- Read a chapter\n- Felt better\n");
        WriteFile(root / "Journal" / "2026" / "03" / "2026-03-31.md", "# 2026-03-31\n\n- Previous month\n");
        WriteFile(root / "Docs" / "Plan.md", "# Plan\n");

        Software::Slate::WorkspaceService workspace(root);
        Software::Slate::JournalService journal;

        CHECK(journal.IsJournalPath("Journal/2026/04/2026-04-02.md"));
        CHECK(!journal.IsJournalPath("Docs/Plan.md"));
        CHECK(journal.HasMeaningfulContent("# 2026-04-02\n\n- Walked outside\n"));
        CHECK(!journal.HasMeaningfulContent("# 2026-04-01\n\n- "));

        const std::string activeText = "# 2026-04-01\n\n- Draft note\n";
        const auto summary =
            journal.BuildMonthSummary(workspace, 2026, 4, "Journal/2026/04/2026-04-01.md", &activeText);
        CHECK(summary.year == 2026);
        CHECK(summary.month == 4);
        CHECK(summary.daysInMonth == 30);
        CHECK(summary.firstWeekdayMondayBased == 2);
        CHECK(summary.writtenDays == 3);
        CHECK(summary.activeDay == 1);
        CHECK(summary.activeWordCount >= 2);
        CHECK(summary.days[0].hasContent);
        CHECK(summary.days[1].hasContent);
        CHECK(summary.days[2].hasContent);
        CHECK(summary.days[0].wordCount >= 2);
        CHECK(summary.days[1].wordCount >= 2);
        CHECK(summary.days[2].wordCount >= 5);
        fs::remove_all(root);
    }

    void TestArrowNavigationController()
    {
        Software::Slate::NavigationController nav;
        nav.SetCount(3);
        nav.MoveNext();
        CHECK(nav.Selected() == 1);
        nav.MovePrevious();
        CHECK(nav.Selected() == 0);
        nav.MovePrevious();
        CHECK(nav.Selected() == 2);
        nav.SetSelected(99);
        CHECK(nav.Selected() == 2);
    }

    void TestEditorDocumentViewModel()
    {
        Software::Slate::EditorDocumentViewModel editor;
        editor.Load("# Title\nbody\nlast", "\n");
        CHECK(editor.Lines().size() == 3);
        CHECK(editor.ActiveLine() == 0);
        editor.SetActiveLine(1, 4);
        CHECK(editor.ActiveLineText() == "body");
        editor.ReplaceActiveLine("body text");
        editor.SetCaretColumn(4);
        CHECK(editor.SplitActiveLine());
        CHECK(editor.ActiveLine() == 2);
        CHECK(editor.ActiveLineText() == " text");
        CHECK(editor.ToText() == "# Title\nbody\n text\nlast");
        CHECK(editor.MergeActiveLineWithPrevious());
        CHECK(editor.ActiveLineText() == "body text");
        CHECK(editor.ToText() == "# Title\nbody text\nlast");
        editor.SetCaretColumn(static_cast<int>(editor.ActiveLineText().size()));
        CHECK(editor.MergeActiveLineWithNext());
        CHECK(editor.ActiveLineText() == "body textlast");
        editor.SetCaretColumn(4);
        CHECK(editor.InsertTextAtCursor("\nnew"));
        CHECK(editor.ActiveLineText() == "new textlast");
        CHECK(editor.ToText() == "# Title\nbody\nnew textlast");
        editor.SetActiveLine(0, 20);
        editor.MoveActiveLine(1);
        CHECK(editor.CaretColumn() == 4);
    }

    void TestEditorSingleCharacterDeletionDoesNotMergeLines()
    {
        Software::Slate::EditorDocumentViewModel editor;
        editor.Load("previous\na", "\n");
        editor.SetActiveLine(1, 1);
        editor.ReplaceActiveLine("");
        CHECK(editor.ActiveLine() == 1);
        CHECK(editor.ToText() == "previous\n");
        CHECK(editor.MergeActiveLineWithPrevious());
        CHECK(editor.ToText() == "previous");
    }

    void TestMarkdownInlineSpans()
    {
        const auto spans =
            Software::Slate::MarkdownService::ParseInlineSpans("plain **bold** *italic* `code` [link](x) [[Wiki|label]] <u>under</u>");
        bool sawBold = false;
        bool sawItalic = false;
        bool sawCode = false;
        bool sawLink = false;
        bool sawUnderlineSource = false;
        for (const auto& span : spans)
        {
            sawBold = sawBold || (span.text == "bold" && span.bold);
            sawItalic = sawItalic || (span.text == "italic" && span.italic);
            sawCode = sawCode || (span.text == "code" && span.code);
            sawLink = sawLink || ((span.text == "link" || span.text == "label") && span.link);
            sawUnderlineSource = sawUnderlineSource || span.text.find("<u>under</u>") != std::string::npos;
        }
        CHECK(sawBold);
        CHECK(sawItalic);
        CHECK(sawCode);
        CHECK(sawLink);
        CHECK(sawUnderlineSource);
    }

    void TestMarkdownTagsAndTodos()
    {
        Software::Slate::MarkdownService markdown;

        const auto tagsOnly = markdown.Parse("# Heading #ignored\nplain #alpha\n```md\n#beta\n#todo - [Open] - hidden\n```\n");
        CHECK(tagsOnly.tags.size() == 1);
        CHECK(tagsOnly.tags.front().name == "alpha");

        const std::string todoText =
            "before\n"
            "#todo - [Research] - Ship compact todos #ux\n"
            "  Keep title and description tidy #compact\n"
            "after\n";
        const auto summary = markdown.Parse(todoText);
        CHECK(summary.todos.size() == 1);
        const auto& todo = summary.todos.front();
        CHECK(todo.line == 2);
        CHECK(todo.endLine == 3);
        CHECK(todo.state == Software::Slate::TodoState::Research);
        CHECK(!todo.done);
        CHECK(todo.title == "Ship compact todos #ux");
        CHECK(todo.description == "Keep title and description tidy #compact");
        CHECK(std::find(todo.tags.begin(), todo.tags.end(), "todo") != todo.tags.end());
        CHECK(std::find(todo.tags.begin(), todo.tags.end(), "ux") != todo.tags.end());
        CHECK(std::find(todo.tags.begin(), todo.tags.end(), "compact") != todo.tags.end());

        const auto legacy = markdown.Parse("- [x] #todo [Done] Legacy ticket\n  Wrapped\n");
        CHECK(legacy.todos.size() == 1);
        CHECK(legacy.todos.front().state == Software::Slate::TodoState::Done);
        CHECK(legacy.todos.front().done);
    }

    void TestTodoDocumentFormattingAndParsing()
    {
        const std::string text = Software::Slate::TodoService::FormatTodoDocument(
            Software::Slate::TodoState::Doing,
            "Ship todo files",
            "Keep todo details editable\nAdd useful notes",
            "\n");

        Software::Slate::TodoTicket ticket;
        CHECK(Software::Slate::TodoService::ParseTodoDocument(text, "Todos/ship-todo-files.md", &ticket));
        CHECK(ticket.relativePath == fs::path("Todos/ship-todo-files.md"));
        CHECK(ticket.state == Software::Slate::TodoState::Doing);
        CHECK(ticket.priority == Software::Slate::TodoPriority::Normal);
        CHECK(ticket.title == "Ship todo files");
        CHECK(ticket.description == "Keep todo details editable\nAdd useful notes");
        CHECK(ticket.line == 7);
        CHECK(!Software::Slate::TodoService::IsTodoDocument("# Normal note\n", "Notes/Normal.md"));
    }

    void TestEditorDocumentReplaceActiveLineWithText()
    {
        Software::Slate::EditorDocumentViewModel editor;
        editor.Load("first\n/template\nlast", "\n");
        editor.SetActiveLine(1, 9);
        CHECK(editor.ReplaceActiveLineWithText("inserted\ncontent"));
        CHECK(editor.ActiveLine() == 2);
        CHECK(editor.ActiveLineText() == "content");
        CHECK(editor.ToText() == "first\ninserted\ncontent\nlast");
    }

    void TestWorkspaceRegistry()
    {
        const fs::path root = MakeTempWorkspace();
        const fs::path settingsPath = root / "settings.tsv";

        Software::Slate::WorkspaceRegistryService registry(settingsPath);
        auto first = registry.AddVault("*", "Design", root / "Design");
        auto second = registry.AddVault("#", "Journal", root / "Journal");
        CHECK(registry.HasVaults());
        CHECK(registry.ActiveVault());
        CHECK(registry.ActiveVault()->id == second.id);
        CHECK(registry.SetActiveVault(first.id));
        CHECK(registry.Save());

        Software::Slate::WorkspaceRegistryService loaded(settingsPath);
        CHECK(loaded.Load());
        CHECK(loaded.Vaults().size() == 2);
        CHECK(loaded.ActiveVault());
        CHECK(loaded.ActiveVault()->title == "Design");
        CHECK(loaded.RemoveVault(first.id));
        CHECK(loaded.ActiveVault());
        CHECK(loaded.ActiveVault()->title == "Journal");
        fs::remove_all(root);
    }

    void TestThemeServiceRoundTrip()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / ".slate" / "presets" / "shell" / "custom-shell.tsv",
                  "id\tcustom-shell\nlabel\tCustom Shell\nbackground\t0.100,0.100,0.100,1.000\n"
                  "primary\t0.900,0.900,0.900,1.000\nmuted\t0.500,0.500,0.500,1.000\n"
                  "cyan\t0.200,0.800,0.900,1.000\namber\t0.900,0.700,0.200,1.000\n"
                  "green\t0.500,0.800,0.500,1.000\nred\t0.900,0.300,0.300,1.000\n"
                  "panel\t0.120,0.120,0.120,1.000\ncode\t0.700,0.800,0.900,1.000\n");
        WriteFile(root / ".slate" / "presets" / "markdown" / "custom-markdown.tsv",
                  "id\tcustom-markdown\nlabel\tCustom Markdown\nheading1\t0.300,0.700,0.900,1.000\n"
                  "heading2\t0.500,0.800,0.600,1.000\nheading3\t0.900,0.700,0.300,1.000\n"
                  "bullet\t0.800,0.700,0.300,1.000\ncheckbox\t0.850,0.700,0.350,1.000\n"
                  "checkbox_done\t0.500,0.850,0.500,1.000\nquote\t0.600,0.700,0.800,1.000\n"
                  "table\t0.750,0.730,0.500,1.000\nlink\t0.400,0.800,0.950,1.000\n"
                  "image\t0.900,0.550,0.450,1.000\nbold\t0.960,0.920,0.860,1.000\n"
                  "italic\t0.760,0.820,0.700,1.000\ncode\t0.780,0.820,0.860,1.000\n");
        Software::Slate::ThemeService theme(root);

        Software::Slate::ThemeSettings settings;
        settings.shellPreset = "custom-shell";
        settings.markdownPreset = "custom-markdown";
        CHECK(theme.Save(settings));

        Software::Slate::ThemeSettings loaded;
        CHECK(theme.Load(&loaded));
        CHECK(loaded.shellPreset == "custom-shell");
        CHECK(loaded.markdownPreset == "custom-markdown");
        CHECK(fs::exists(root / ".slate" / "theme.tsv"));
        fs::remove_all(root);
    }

    void TestThemeServiceApply()
    {
        const fs::path root = MakeTempWorkspace();
        WriteFile(root / ".slate" / "presets" / "shell" / "apply-shell.tsv",
                  "id\tapply-shell\nlabel\tApply Shell\nbackground\t0.200,0.200,0.200,1.000\n"
                  "primary\t0.880,0.870,0.810,1.000\nmuted\t0.450,0.500,0.470,1.000\n"
                  "cyan\t0.460,0.760,0.710,1.000\namber\t0.900,0.690,0.370,1.000\n"
                  "green\t0.620,0.840,0.580,1.000\nred\t0.880,0.410,0.360,1.000\n"
                  "panel\t0.072,0.083,0.078,1.000\ncode\t0.710,0.800,0.780,1.000\n");
        WriteFile(root / ".slate" / "presets" / "markdown" / "apply-markdown.tsv",
                  "id\tapply-markdown\nlabel\tApply Markdown\nheading1\t0.620,0.780,0.820,1.000\n"
                  "heading2\t0.740,0.820,0.700,1.000\nheading3\t0.840,0.720,0.520,1.000\n"
                  "bullet\t0.700,0.690,0.600,1.000\ncheckbox\t0.780,0.690,0.480,1.000\n"
                  "checkbox_done\t0.600,0.770,0.580,1.000\nquote\t0.580,0.630,0.660,1.000\n"
                  "table\t0.700,0.690,0.600,1.000\nlink\t0.620,0.780,0.900,1.000\n"
                  "image\t0.820,0.620,0.560,1.000\nbold\t0.900,0.880,0.820,1.000\n"
                  "italic\t0.720,0.740,0.690,1.000\ncode\t0.720,0.760,0.780,1.000\n");

        Software::Slate::ThemeService theme(root);
        theme.Apply({"apply-shell", "apply-markdown"});
        CHECK(Software::Slate::UI::Background.x == 0.200f);
        CHECK(Software::Slate::UI::Panel.y == 0.083f);
        CHECK(Software::Slate::UI::MarkdownHeading1.x == 0.62f);
        CHECK(Software::Slate::UI::MarkdownImage.x == 0.82f);

        theme.Apply(Software::Slate::ThemeService::DefaultSettings());
        CHECK(Software::Slate::UI::Background.x >= 0.0f);
        CHECK(Software::Slate::UI::MarkdownHeading1.x >= 0.0f);
        fs::remove_all(root);
    }

    void TestTodoServiceMatching()
    {
        Software::Slate::TodoTicket todo;
        todo.title = "Refactor command routing";
        todo.description = "Move command handling out of the base mode";
        todo.relativePath = fs::path("Docs/Architecture.md");
        todo.state = Software::Slate::TodoState::Doing;
        todo.tags = {"architecture", "commands"};

        CHECK(Software::Slate::TodoService::StateSortKey(Software::Slate::TodoState::Open) <
              Software::Slate::TodoService::StateSortKey(Software::Slate::TodoState::Done));
        CHECK(Software::Slate::TodoService::MatchesQuery(todo, "command"));
        CHECK(Software::Slate::TodoService::MatchesQuery(todo, "#architecture"));
        CHECK(Software::Slate::TodoService::MatchesQuery(todo, "doing"));
        CHECK(!Software::Slate::TodoService::MatchesQuery(todo, "unrelated"));
    }

    void TestEditorSettingsRoundTrip()
    {
        const fs::path root = MakeTempWorkspace();
        Software::Slate::EditorSettingsService settingsService(root);

        Software::Slate::EditorSettings settings = Software::Slate::EditorSettingsService::DefaultSettings();
        settings.fontSize = 17;
        settings.lineSpacing = 5;
        settings.pageWidth = 880;
        settings.wordWrap = false;
        settings.showWhitespace = true;
        settings.highlightCurrentLine = false;
        settings.tabWidth = 2;
        settings.panelLayout = 1;
        settings.previewRenderMode = 1;
        settings.previewFollowMode = 2;
        settings.panelMotion = 3;
        settings.scrollMotion = 2;
        settings.scrollbarStyle = 2;
        settings.caretMotion = 2;
        settings.linkUnderline = false;
        settings.indentWithTabs = true;
        settings.autoListContinuation = false;
        settings.pasteClipboardImages = false;
        CHECK(settingsService.Save(settings));

        Software::Slate::EditorSettings loaded;
        CHECK(settingsService.Load(&loaded));
        CHECK(loaded.fontSize == 17);
        CHECK(loaded.lineSpacing == 5);
        CHECK(loaded.pageWidth == 880);
        CHECK(!loaded.wordWrap);
        CHECK(loaded.showWhitespace);
        CHECK(!loaded.highlightCurrentLine);
        CHECK(loaded.tabWidth == 2);
        CHECK(loaded.panelLayout == 1);
        CHECK(loaded.previewRenderMode == 0);
        CHECK(loaded.previewFollowMode == 2);
        CHECK(loaded.panelMotion == 3);
        CHECK(loaded.scrollMotion == 2);
        CHECK(loaded.scrollbarStyle == 2);
        CHECK(loaded.caretMotion == 2);
        CHECK(!loaded.linkUnderline);
        CHECK(loaded.indentWithTabs);
        CHECK(!loaded.autoListContinuation);
        CHECK(!loaded.pasteClipboardImages);
        CHECK(fs::exists(root / ".slate" / "editor.tsv"));
        fs::remove_all(root);
    }
}

int main()
{
    TestWorkspaceScan();
    TestDocumentSaveAndRecovery();
    TestSearch();
    TestLinksRewrite();
    TestDailyAndAssets();
    TestCollisionSafeNotePaths();
    TestWorkspaceFolderOperations();
    TestTreeFilteringPreservesParents();
    TestCollapsedTreeHidesDescendants();
    TestLibraryTreeExcludesJournalAndPreservesParents();
    TestJournalMonthSummary();
    TestArrowNavigationController();
    TestEditorDocumentViewModel();
    TestEditorSingleCharacterDeletionDoesNotMergeLines();
    TestMarkdownInlineSpans();
    TestMarkdownTagsAndTodos();
    TestTodoDocumentFormattingAndParsing();
    TestEditorDocumentReplaceActiveLineWithText();
    TestWorkspaceRegistry();
    TestThemeServiceRoundTrip();
    TestThemeServiceApply();
    TestTodoServiceMatching();
    TestEditorSettingsRoundTrip();
    std::cout << "SlateCoreTests passed\n";
    return 0;
}


