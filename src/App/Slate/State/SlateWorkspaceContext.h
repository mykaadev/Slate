#pragma once

#include "App/Slate/Documents/AssetService.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Documents/LinkService.h"
#include "App/Slate/Workspace/SearchService.h"
#include "App/Slate/Workspace/ThemeService.h"
#include "App/Slate/Workspace/WorkspaceRegistryService.h"
#include "App/Slate/Workspace/WorkspaceService.h"

#include <string>

namespace Software::Slate
{
    // Owns the shared workspace level services
    class SlateWorkspaceContext
    {
    public:
        // Builds long lived service state
        void Initialize();
        // Advances background workspace tasks
        void Update(double elapsedSeconds);
        // Pulls one background error for the UI
        bool ConsumeBackgroundError(std::string* error);

        // Reports whether a workspace is open
        bool HasWorkspaceLoaded() const;

        // Returns the workspace service
        WorkspaceService& Workspace();
        // Returns the workspace service as const
        const WorkspaceService& Workspace() const;

        // Returns the document service
        DocumentService& Documents();
        // Returns the document service as const
        const DocumentService& Documents() const;

        // Returns the search service
        SearchService& Search();
        // Returns the search service as const
        const SearchService& Search() const;

        // Returns the link service
        LinkService& Links();
        // Returns the link service as const
        const LinkService& Links() const;

        // Returns the asset service
        AssetService& Assets();
        // Returns the asset service as const
        const AssetService& Assets() const;

        // Returns the theme service
        ThemeService& Theme();
        // Returns the theme service as const
        const ThemeService& Theme() const;

        // Returns the workspace registry service
        WorkspaceRegistryService& WorkspaceRegistry();
        // Returns the workspace registry service as const
        const WorkspaceRegistryService& WorkspaceRegistry() const;

        // Returns the live theme settings
        const ThemeSettings& CurrentThemeSettings() const;
        // Replaces the live theme settings
        void SetThemeSettings(const ThemeSettings& settings);
        // Saves the live theme settings
        bool SaveThemeSettings(std::string* error = nullptr);
        // Returns the live editor settings
        const EditorSettings& CurrentEditorSettings() const;
        // Replaces the live editor settings
        void SetEditorSettings(const EditorSettings& settings);
        // Saves the live editor settings
        bool SaveEditorSettings(std::string* error = nullptr);

        // Opens a workspace by root path
        bool OpenWorkspace(const fs::path& root, std::string* error = nullptr);
        // Opens a registered workspace vault
        bool OpenVault(const WorkspaceVault& vault, std::string* error = nullptr);
        // Creates and registers a workspace vault
        bool CreateWorkspaceVault(const std::string& title, const std::string& emoji, const fs::path& path,
                                  WorkspaceVault* createdVault, std::string* error = nullptr);

        // Opens one workspace document
        bool OpenDocument(const fs::path& relativePath, std::string* error = nullptr);
        // Saves the active document
        bool SaveDocument(std::string* error = nullptr);
        // Closes the active document
        void CloseDocument();

        // Opens the current daily note
        bool OpenTodayJournal(fs::path* dailyPath, std::string* error = nullptr);
        // Creates a new note inside a folder
        bool CreateNewNote(const fs::path& folder, const std::string& value, fs::path* createdPath,
                           std::string* error = nullptr);
        // Creates a new folder inside a folder
        bool CreateNewFolder(const fs::path& folder, const std::string& value, fs::path* createdPath,
                             std::string* error = nullptr);
        // Builds a rewrite plan for rename aware links
        RewritePlan BuildRenamePlan(const fs::path& oldRelativePath, const fs::path& newRelativePath) const;
        // Applies a rename rewrite plan
        bool ApplyRewritePlan(const RewritePlan& plan, std::string* error = nullptr) const;
        // Renames or moves one path and related links
        bool RenameOrMove(const fs::path& oldRelativePath, const fs::path& newRelativePath, std::string* error = nullptr);
        // Deletes one workspace path
        bool DeletePath(const fs::path& relativePath, std::string* error = nullptr);

    private:
        // Stores one background error for later display
        void SetBackgroundError(std::string error);

        // Stores the workspace service
        WorkspaceService m_workspace;
        // Stores the document service
        DocumentService m_documents;
        // Stores the search service
        SearchService m_search;
        // Stores the link service
        LinkService m_links;
        // Stores the asset service
        AssetService m_assets;
        // Stores the theme service
        ThemeService m_theme;
        // Stores the editor settings loader
        EditorSettingsService m_editorSettingsService;
        // Stores the workspace registry
        WorkspaceRegistryService m_workspaceRegistry;
        // Stores the current theme settings
        ThemeSettings m_themeSettings = ThemeService::DefaultSettings();
        // Stores the current editor settings
        EditorSettings m_editorSettings = EditorSettingsService::DefaultSettings();
        // Stores the last background error
        std::string m_backgroundError;
        // Stores when the workspace was last scanned
        double m_lastScanSeconds = 0.0;
        // Stores when the search index was last rebuilt
        double m_lastIndexSeconds = 0.0;
        // Marks whether a workspace is loaded
        bool m_workspaceLoaded = false;
        // Marks whether services were initialized
        bool m_initialized = false;
    };
}
