#pragma once

#include "App/Slate/AssetService.h"
#include "App/Slate/DocumentService.h"
#include "App/Slate/EditorSettingsService.h"
#include "App/Slate/LinkService.h"
#include "App/Slate/SearchService.h"
#include "App/Slate/ThemeService.h"
#include "App/Slate/WorkspaceRegistryService.h"
#include "App/Slate/WorkspaceService.h"

#include <string>

namespace Software::Slate
{
    class SlateWorkspaceContext
    {
    public:
        void Initialize();
        void Update(double elapsedSeconds);
        bool ConsumeBackgroundError(std::string* error);

        bool HasWorkspaceLoaded() const;

        WorkspaceService& Workspace();
        const WorkspaceService& Workspace() const;

        DocumentService& Documents();
        const DocumentService& Documents() const;

        SearchService& Search();
        const SearchService& Search() const;

        LinkService& Links();
        const LinkService& Links() const;

        AssetService& Assets();
        const AssetService& Assets() const;

        ThemeService& Theme();
        const ThemeService& Theme() const;

        WorkspaceRegistryService& WorkspaceRegistry();
        const WorkspaceRegistryService& WorkspaceRegistry() const;

        const ThemeSettings& CurrentThemeSettings() const;
        void SetThemeSettings(const ThemeSettings& settings);
        bool SaveThemeSettings(std::string* error = nullptr);
        const EditorSettings& CurrentEditorSettings() const;
        void SetEditorSettings(const EditorSettings& settings);
        bool SaveEditorSettings(std::string* error = nullptr);

        bool OpenWorkspace(const fs::path& root, std::string* error = nullptr);
        bool OpenVault(const WorkspaceVault& vault, std::string* error = nullptr);
        bool CreateWorkspaceVault(const std::string& title, const std::string& emoji, const fs::path& path,
                                  WorkspaceVault* createdVault, std::string* error = nullptr);

        bool OpenDocument(const fs::path& relativePath, std::string* error = nullptr);
        bool SaveDocument(std::string* error = nullptr);
        void CloseDocument();

        bool OpenTodayJournal(fs::path* dailyPath, std::string* error = nullptr);
        bool CreateNewNote(const fs::path& folder, const std::string& value, fs::path* createdPath,
                           std::string* error = nullptr);
        bool CreateNewFolder(const fs::path& folder, const std::string& value, fs::path* createdPath,
                             std::string* error = nullptr);
        RewritePlan BuildRenamePlan(const fs::path& oldRelativePath, const fs::path& newRelativePath) const;
        bool ApplyRewritePlan(const RewritePlan& plan, std::string* error = nullptr) const;
        bool RenameOrMove(const fs::path& oldRelativePath, const fs::path& newRelativePath, std::string* error = nullptr);
        bool DeletePath(const fs::path& relativePath, std::string* error = nullptr);

    private:
        void SetBackgroundError(std::string error);

        WorkspaceService m_workspace;
        DocumentService m_documents;
        SearchService m_search;
        LinkService m_links;
        AssetService m_assets;
        ThemeService m_theme;
        EditorSettingsService m_editorSettingsService;
        WorkspaceRegistryService m_workspaceRegistry;
        ThemeSettings m_themeSettings = ThemeService::DefaultSettings();
        EditorSettings m_editorSettings = EditorSettingsService::DefaultSettings();
        std::string m_backgroundError;
        double m_lastScanSeconds = 0.0;
        double m_lastIndexSeconds = 0.0;
        bool m_workspaceLoaded = false;
        bool m_initialized = false;
    };
}
