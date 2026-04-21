#pragma once

#include "App/Slate/SlateTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    struct ThemeSettings
    {
        std::string shellPreset;
        std::string markdownPreset;
    };

    class ThemeService
    {
    public:
        explicit ThemeService(fs::path workspaceRoot = {});

        void SetWorkspaceRoot(fs::path workspaceRoot);
        fs::path SettingsPath() const;

        bool Load(ThemeSettings* out, std::string* error = nullptr) const;
        bool Save(const ThemeSettings& settings, std::string* error = nullptr) const;
        void Apply(const ThemeSettings& settings) const;

        static ThemeSettings DefaultSettings();
        static const std::vector<std::string>& ShellPresetIds();
        static const std::vector<std::string>& MarkdownPresetIds();
        static std::string ShellPresetLabel(std::string_view id);
        static std::string MarkdownPresetLabel(std::string_view id);

    private:
        fs::path m_workspaceRoot;
    };
}
