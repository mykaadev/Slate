#include "App/Slate/Workspace/WorkspaceRegistryService.h"

#include "App/Slate/Core/PathUtils.h"

#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        std::vector<std::string> SplitTabs(const std::string& line)
        {
            std::vector<std::string> parts;
            std::size_t start = 0;
            while (start <= line.size())
            {
                const std::size_t end = line.find('\t', start);
                if (end == std::string::npos)
                {
                    parts.push_back(line.substr(start));
                    break;
                }
                parts.push_back(line.substr(start, end - start));
                start = end + 1;
            }
            return parts;
        }

        fs::path EnvPath(const char* name)
        {
            const char* value = std::getenv(name);
            return value ? fs::path(value) : fs::path();
        }
    }

    WorkspaceRegistryService::WorkspaceRegistryService(fs::path settingsPath)
        : m_settingsPath(settingsPath.empty() ? DefaultSettingsPath() : std::move(settingsPath))
    {
    }

    bool WorkspaceRegistryService::Load(std::string* error)
    {
        m_vaults.clear();
        m_activeVaultId.clear();

        std::ifstream file(m_settingsPath, std::ios::binary);
        if (!file)
        {
            return true;
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.rfind("active\t", 0) == 0)
            {
                m_activeVaultId = Unescape(line.substr(7));
                continue;
            }

            if (line.rfind("vault\t", 0) != 0)
            {
                continue;
            }

            const auto parts = SplitTabs(line);
            if (parts.size() < 5)
            {
                continue;
            }

            WorkspaceVault vault;
            vault.id = Unescape(parts[1]);
            vault.emoji = Unescape(parts[2]);
            vault.title = Unescape(parts[3]);
            vault.path = fs::path(Unescape(parts[4]));
            if (!vault.id.empty() && !vault.title.empty() && !vault.path.empty())
            {
                m_vaults.push_back(std::move(vault));
            }
        }

        if (file.bad())
        {
            if (error)
            {
                *error = "Could not read app settings.";
            }
            return false;
        }

        if (m_activeVaultId.empty() && !m_vaults.empty())
        {
            m_activeVaultId = m_vaults.front().id;
        }
        return true;
    }

    bool WorkspaceRegistryService::Save(std::string* error) const
    {
        std::error_code ec;
        fs::create_directories(m_settingsPath.parent_path(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        std::ofstream file(m_settingsPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            if (error)
            {
                *error = "Could not write app settings.";
            }
            return false;
        }

        file << "active\t" << Escape(m_activeVaultId) << '\n';
        for (const auto& vault : m_vaults)
        {
            file << "vault\t" << Escape(vault.id) << '\t' << Escape(vault.emoji) << '\t'
                 << Escape(vault.title) << '\t' << Escape(vault.path.string()) << '\n';
        }
        return static_cast<bool>(file);
    }

    const std::vector<WorkspaceVault>& WorkspaceRegistryService::Vaults() const
    {
        return m_vaults;
    }

    bool WorkspaceRegistryService::HasVaults() const
    {
        return !m_vaults.empty();
    }

    const WorkspaceVault* WorkspaceRegistryService::ActiveVault() const
    {
        return FindVault(m_activeVaultId);
    }

    const WorkspaceVault* WorkspaceRegistryService::FindVault(const std::string& id) const
    {
        for (const auto& vault : m_vaults)
        {
            if (vault.id == id)
            {
                return &vault;
            }
        }
        return nullptr;
    }

    WorkspaceVault WorkspaceRegistryService::AddVault(std::string emoji, std::string title, fs::path path)
    {
        WorkspaceVault vault;
        vault.emoji = PathUtils::Trim(emoji).empty() ? DefaultEmoji() : PathUtils::Trim(emoji);
        vault.title = PathUtils::Trim(title).empty() ? "My Workspace" : PathUtils::Trim(title);
        vault.path = std::move(path);
        vault.id = MakeUniqueId(vault.title, vault.path);

        m_vaults.push_back(vault);
        m_activeVaultId = vault.id;
        return vault;
    }

    bool WorkspaceRegistryService::RemoveVault(const std::string& id)
    {
        const auto oldSize = m_vaults.size();
        m_vaults.erase(std::remove_if(m_vaults.begin(), m_vaults.end(), [&id](const WorkspaceVault& vault) {
                           return vault.id == id;
                       }),
                       m_vaults.end());

        if (oldSize == m_vaults.size())
        {
            return false;
        }

        if (m_activeVaultId == id)
        {
            m_activeVaultId = m_vaults.empty() ? std::string() : m_vaults.front().id;
        }
        return true;
    }

    bool WorkspaceRegistryService::SetActiveVault(const std::string& id)
    {
        if (!FindVault(id))
        {
            return false;
        }
        m_activeVaultId = id;
        return true;
    }

    fs::path WorkspaceRegistryService::SettingsPath() const
    {
        return m_settingsPath;
    }

    fs::path WorkspaceRegistryService::DefaultSettingsPath()
    {
        fs::path base = EnvPath("APPDATA");
        if (base.empty())
        {
            base = EnvPath("USERPROFILE");
        }
        if (base.empty())
        {
            base = fs::current_path();
        }
        return base / "Slate" / "settings.tsv";
    }

    fs::path WorkspaceRegistryService::DefaultWorkspaceRoot()
    {
        fs::path user = EnvPath("USERPROFILE");
        if (user.empty())
        {
            return fs::current_path() / "Documents" / "Slate Workspaces";
        }
        return user / "Documents" / "Slate Workspaces";
    }

    fs::path WorkspaceRegistryService::DefaultWorkspacePathForTitle(const std::string& title)
    {
        return DefaultWorkspaceRoot() / PathUtils::SanitizeFileName(PathUtils::Trim(title).empty() ? "My Workspace" : title);
    }

    std::string WorkspaceRegistryService::DefaultEmoji()
    {
        return "\xF0\x9F\x93\x93";
    }

    std::string WorkspaceRegistryService::Escape(std::string_view value)
    {
        std::string out;
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                out += "\\\\";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out.push_back(ch);
                break;
            }
        }
        return out;
    }

    std::string WorkspaceRegistryService::Unescape(std::string_view value)
    {
        std::string out;
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '\\' || i + 1 >= value.size())
            {
                out.push_back(value[i]);
                continue;
            }

            const char next = value[++i];
            switch (next)
            {
            case 't':
                out.push_back('\t');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            default:
                out.push_back(next);
                break;
            }
        }
        return out;
    }

    std::string WorkspaceRegistryService::MakeUniqueId(const std::string& title, const fs::path& path) const
    {
        const std::string base = PathUtils::SanitizeFileName(title) + "-" + PathUtils::StablePathId(path);
        std::string candidate = base;
        for (int suffix = 2; FindVault(candidate) && suffix < 10000; ++suffix)
        {
            candidate = base + "-" + std::to_string(suffix);
        }
        return candidate;
    }
}
