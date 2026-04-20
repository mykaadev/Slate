#pragma once

#include "App/Slate/SlateTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace Software::Slate
{
    class WorkspaceRegistryService
    {
    public:
        explicit WorkspaceRegistryService(fs::path settingsPath = {});

        bool Load(std::string* error = nullptr);
        bool Save(std::string* error = nullptr) const;

        const std::vector<WorkspaceVault>& Vaults() const;
        bool HasVaults() const;
        const WorkspaceVault* ActiveVault() const;
        const WorkspaceVault* FindVault(const std::string& id) const;

        WorkspaceVault AddVault(std::string emoji, std::string title, fs::path path);
        bool RemoveVault(const std::string& id);
        bool SetActiveVault(const std::string& id);

        fs::path SettingsPath() const;

        static fs::path DefaultSettingsPath();
        static fs::path DefaultWorkspaceRoot();
        static fs::path DefaultWorkspacePathForTitle(const std::string& title);
        static std::string DefaultEmoji();

    private:
        static std::string Escape(std::string_view value);
        static std::string Unescape(std::string_view value);
        std::string MakeUniqueId(const std::string& title, const fs::path& path) const;

        fs::path m_settingsPath;
        std::vector<WorkspaceVault> m_vaults;
        std::string m_activeVaultId;
    };
}
