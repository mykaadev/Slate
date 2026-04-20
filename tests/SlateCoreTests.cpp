#include "App/Slate/AssetService.h"
#include "App/Slate/DocumentService.h"
#include "App/Slate/LinkService.h"
#include "App/Slate/NavigationController.h"
#include "App/Slate/SearchService.h"
#include "App/Slate/WorkspaceService.h"
#include "App/Slate/WorkspaceRegistryService.h"
#include "App/Slate/WorkspaceTree.h"

#include <chrono>
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
        const auto results = search.Query("shield");
        CHECK(!results.empty());
        CHECK(results.front().relativePath == fs::path("GDD.md"));
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
}

int main()
{
    TestWorkspaceScan();
    TestDocumentSaveAndRecovery();
    TestSearch();
    TestLinksRewrite();
    TestDailyAndAssets();
    TestCollisionSafeNotePaths();
    TestTreeFilteringPreservesParents();
    TestArrowNavigationController();
    TestWorkspaceRegistry();
    std::cout << "SlateCoreTests passed\n";
    return 0;
}
