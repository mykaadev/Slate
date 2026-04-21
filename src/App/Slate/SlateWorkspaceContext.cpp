#include "App/Slate/SlateWorkspaceContext.h"

namespace Software::Slate
{
    void SlateWorkspaceContext::Initialize()
    {
        if (m_initialized)
        {
            return;
        }

        m_themeSettings = ThemeService::DefaultSettings();
        m_theme.Apply(m_themeSettings);

        std::string error;
        if (!m_workspaceRegistry.Load(&error))
        {
            SetBackgroundError(error);
        }

        if (const auto* vault = m_workspaceRegistry.ActiveVault())
        {
            if (!OpenVault(*vault, &error))
            {
                SetBackgroundError(error);
            }
        }

        m_initialized = true;
    }

    void SlateWorkspaceContext::Update(double elapsedSeconds)
    {
        if (!m_workspaceLoaded)
        {
            return;
        }

        if (elapsedSeconds - m_lastScanSeconds > 2.0)
        {
            m_documents.CheckExternalChange();
            m_lastScanSeconds = elapsedSeconds;
        }

        std::string error;
        if (!m_documents.SaveIfNeeded(elapsedSeconds, &error))
        {
            SetBackgroundError(error);
        }

        if (elapsedSeconds - m_lastIndexSeconds > 5.0)
        {
            m_search.Rebuild(m_workspace);
            m_lastIndexSeconds = elapsedSeconds;
        }
    }

    bool SlateWorkspaceContext::ConsumeBackgroundError(std::string* error)
    {
        if (m_backgroundError.empty())
        {
            return false;
        }

        if (error)
        {
            *error = m_backgroundError;
        }
        m_backgroundError.clear();
        return true;
    }

    bool SlateWorkspaceContext::HasWorkspaceLoaded() const
    {
        return m_workspaceLoaded;
    }

    WorkspaceService& SlateWorkspaceContext::Workspace()
    {
        return m_workspace;
    }

    const WorkspaceService& SlateWorkspaceContext::Workspace() const
    {
        return m_workspace;
    }

    DocumentService& SlateWorkspaceContext::Documents()
    {
        return m_documents;
    }

    const DocumentService& SlateWorkspaceContext::Documents() const
    {
        return m_documents;
    }

    SearchService& SlateWorkspaceContext::Search()
    {
        return m_search;
    }

    const SearchService& SlateWorkspaceContext::Search() const
    {
        return m_search;
    }

    LinkService& SlateWorkspaceContext::Links()
    {
        return m_links;
    }

    const LinkService& SlateWorkspaceContext::Links() const
    {
        return m_links;
    }

    AssetService& SlateWorkspaceContext::Assets()
    {
        return m_assets;
    }

    const AssetService& SlateWorkspaceContext::Assets() const
    {
        return m_assets;
    }

    ThemeService& SlateWorkspaceContext::Theme()
    {
        return m_theme;
    }

    const ThemeService& SlateWorkspaceContext::Theme() const
    {
        return m_theme;
    }

    WorkspaceRegistryService& SlateWorkspaceContext::WorkspaceRegistry()
    {
        return m_workspaceRegistry;
    }

    const WorkspaceRegistryService& SlateWorkspaceContext::WorkspaceRegistry() const
    {
        return m_workspaceRegistry;
    }

    const ThemeSettings& SlateWorkspaceContext::CurrentThemeSettings() const
    {
        return m_themeSettings;
    }

    void SlateWorkspaceContext::SetThemeSettings(const ThemeSettings& settings)
    {
        m_themeSettings = settings;
    }

    bool SlateWorkspaceContext::SaveThemeSettings(std::string* error)
    {
        m_theme.Apply(m_themeSettings);
        return m_theme.Save(m_themeSettings, error);
    }

    bool SlateWorkspaceContext::OpenWorkspace(const fs::path& root, std::string* error)
    {
        m_documents.Close();

        if (!m_workspace.Open(root, error))
        {
            return false;
        }

        m_documents.SetWorkspaceRoot(m_workspace.Root());
        m_assets.SetWorkspaceRoot(m_workspace.Root());
        m_theme.SetWorkspaceRoot(m_workspace.Root());

        m_search.Rebuild(m_workspace);
        m_lastScanSeconds = 0.0;
        m_lastIndexSeconds = 0.0;
        m_workspaceLoaded = true;

        m_themeSettings = ThemeService::DefaultSettings();
        std::string themeError;
        if (!m_theme.Load(&m_themeSettings, &themeError))
        {
            SetBackgroundError(themeError);
        }
        m_theme.Apply(m_themeSettings);

        return true;
    }

    bool SlateWorkspaceContext::OpenVault(const WorkspaceVault& vault, std::string* error)
    {
        std::error_code ec;
        fs::create_directories(vault.path, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        if (!OpenWorkspace(vault.path, error))
        {
            return false;
        }

        if (m_workspaceRegistry.SetActiveVault(vault.id))
        {
            std::string saveError;
            if (!m_workspaceRegistry.Save(&saveError))
            {
                SetBackgroundError(saveError);
            }
        }
        return true;
    }

    bool SlateWorkspaceContext::CreateWorkspaceVault(const std::string& title, const std::string& emoji, const fs::path& path,
                                                     WorkspaceVault* createdVault, std::string* error)
    {
        std::error_code ec;
        fs::create_directories(path, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        auto vault = m_workspaceRegistry.AddVault(emoji, title, path);
        if (!m_workspaceRegistry.Save(error))
        {
            return false;
        }

        if (createdVault)
        {
            *createdVault = std::move(vault);
        }
        return true;
    }

    bool SlateWorkspaceContext::OpenDocument(const fs::path& relativePath, std::string* error)
    {
        if (!m_documents.Open(relativePath, error))
        {
            return false;
        }

        m_workspace.TouchRecent(relativePath);
        return true;
    }

    bool SlateWorkspaceContext::SaveDocument(std::string* error)
    {
        if (!m_documents.Save(error))
        {
            return false;
        }

        m_workspace.Scan(nullptr);
        m_search.Rebuild(m_workspace);
        return true;
    }

    void SlateWorkspaceContext::CloseDocument()
    {
        m_documents.Close();
    }

    bool SlateWorkspaceContext::OpenTodayJournal(fs::path* dailyPath, std::string* error)
    {
        if (!m_workspace.EnsureDailyNote(dailyPath, error))
        {
            return false;
        }

        m_search.Rebuild(m_workspace);
        return true;
    }

    bool SlateWorkspaceContext::CreateNewNote(const fs::path& folder, const std::string& value, fs::path* createdPath,
                                              std::string* error)
    {
        const fs::path target = m_workspace.MakeCollisionSafeMarkdownPath(folder, value);
        if (!m_workspace.CreateMarkdownFile(target, "# " + target.stem().string() + "\n\n", createdPath, error))
        {
            return false;
        }

        m_search.Rebuild(m_workspace);
        return true;
    }

    bool SlateWorkspaceContext::CreateNewFolder(const fs::path& folder, const std::string& value, fs::path* createdPath,
                                                std::string* error)
    {
        if (!m_workspace.CreateFolder(folder, value, createdPath, error))
        {
            return false;
        }

        m_search.Rebuild(m_workspace);
        return true;
    }

    RewritePlan SlateWorkspaceContext::BuildRenamePlan(const fs::path& oldRelativePath, const fs::path& newRelativePath) const
    {
        return m_links.BuildRenamePlan(m_workspace, oldRelativePath, newRelativePath);
    }

    bool SlateWorkspaceContext::ApplyRewritePlan(const RewritePlan& plan, std::string* error) const
    {
        return m_links.ApplyRewritePlan(m_workspace, plan, error);
    }

    bool SlateWorkspaceContext::RenameOrMove(const fs::path& oldRelativePath, const fs::path& newRelativePath,
                                             std::string* error)
    {
        if (!m_workspace.RenameOrMove(oldRelativePath, newRelativePath, error))
        {
            return false;
        }

        m_search.Rebuild(m_workspace);
        return true;
    }

    bool SlateWorkspaceContext::DeletePath(const fs::path& relativePath, std::string* error)
    {
        if (!m_workspace.DeletePath(relativePath, error))
        {
            return false;
        }

        m_search.Rebuild(m_workspace);
        return true;
    }

    void SlateWorkspaceContext::SetBackgroundError(std::string error)
    {
        if (!error.empty())
        {
            m_backgroundError = std::move(error);
        }
    }
}
