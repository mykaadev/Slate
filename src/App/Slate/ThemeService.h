#pragma once

#include "App/Slate/SlateTypes.h"

#include "imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    struct ShellThemePreset
    {
        std::string id;
        std::string label;
        ImVec4 background;
        ImVec4 primary;
        ImVec4 muted;
        ImVec4 cyan;
        ImVec4 amber;
        ImVec4 green;
        ImVec4 red;
        ImVec4 panel;
        ImVec4 code;
    };

    struct MarkdownThemePreset
    {
        std::string id;
        std::string label;
        ImVec4 heading1;
        ImVec4 heading2;
        ImVec4 heading3;
        ImVec4 bullet;
        ImVec4 checkbox;
        ImVec4 checkboxDone;
        ImVec4 quote;
        ImVec4 table;
        ImVec4 link;
        ImVec4 image;
        ImVec4 bold;
        ImVec4 italic;
        ImVec4 code;
    };

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
        std::vector<std::string> ShellPresetIds() const;
        std::vector<std::string> MarkdownPresetIds() const;
        std::string ShellPresetLabel(std::string_view id) const;
        std::string MarkdownPresetLabel(std::string_view id) const;

    private:
        void EnsurePresetsLoaded() const;
        const ShellThemePreset& FindShellPreset(std::string_view id) const;
        const MarkdownThemePreset& FindMarkdownPreset(std::string_view id) const;

        fs::path m_workspaceRoot;
        mutable bool m_presetsLoaded = false;
        mutable std::vector<ShellThemePreset> m_shellPresets;
        mutable std::vector<MarkdownThemePreset> m_markdownPresets;
    };
}
