#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include "imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    // Stores one shell color preset
    struct ShellThemePreset
    {
        // Stores the preset id
        std::string id;
        // Stores the visible preset label
        std::string label;
        // Stores the app background color
        ImVec4 background;
        // Stores the primary text color
        ImVec4 primary;
        // Stores the muted text color
        ImVec4 muted;
        // Stores the cyan accent color
        ImVec4 cyan;
        // Stores the amber accent color
        ImVec4 amber;
        // Stores the green accent color
        ImVec4 green;
        // Stores the red accent color
        ImVec4 red;
        // Stores the panel background color
        ImVec4 panel;
        // Stores the code accent color
        ImVec4 code;
    };

    // Stores one markdown color preset
    struct MarkdownThemePreset
    {
        // Stores the preset id
        std::string id;
        // Stores the visible preset label
        std::string label;
        // Stores the first heading color
        ImVec4 heading1;
        // Stores the second heading color
        ImVec4 heading2;
        // Stores the fallback heading color
        ImVec4 heading3;
        // Stores the bullet color
        ImVec4 bullet;
        // Stores the unchecked checkbox color
        ImVec4 checkbox;
        // Stores the checked checkbox color
        ImVec4 checkboxDone;
        // Stores the block quote color
        ImVec4 quote;
        // Stores the table text color
        ImVec4 table;
        // Stores the link color
        ImVec4 link;
        // Stores the image marker color
        ImVec4 image;
        // Stores the bold text color
        ImVec4 bold;
        // Stores the italic text color
        ImVec4 italic;
        // Stores the code text color
        ImVec4 code;
    };

    // Stores the chosen preset ids
    struct ThemeSettings
    {
        // Stores the shell preset id
        std::string shellPreset;
        // Stores the markdown preset id
        std::string markdownPreset;
    };

    // Loads saves and applies theme presets
    class ThemeService
    {
    public:
        // Builds the service for one workspace root
        explicit ThemeService(fs::path workspaceRoot = {});

        // Updates the workspace root
        void SetWorkspaceRoot(fs::path workspaceRoot);
        // Returns the theme settings path
        fs::path SettingsPath() const;

        // Loads theme settings from disk
        bool Load(ThemeSettings* out, std::string* error = nullptr) const;
        // Saves theme settings to disk
        bool Save(const ThemeSettings& settings, std::string* error = nullptr) const;
        // Applies theme colors to the live UI
        void Apply(const ThemeSettings& settings) const;

        // Returns the default theme settings
        static ThemeSettings DefaultSettings();
        // Returns all shell preset ids
        std::vector<std::string> ShellPresetIds() const;
        // Returns all markdown preset ids
        std::vector<std::string> MarkdownPresetIds() const;
        // Returns the shell preset label for one id
        std::string ShellPresetLabel(std::string_view id) const;
        // Returns the markdown preset label for one id
        std::string MarkdownPresetLabel(std::string_view id) const;

    private:
        // Loads preset files on first use
        void EnsurePresetsLoaded() const;
        // Finds one shell preset by id
        const ShellThemePreset& FindShellPreset(std::string_view id) const;
        // Finds one markdown preset by id
        const MarkdownThemePreset& FindMarkdownPreset(std::string_view id) const;

        // Stores the workspace root
        fs::path m_workspaceRoot;
        // Marks whether presets are already loaded
        mutable bool m_presetsLoaded = false;
        // Stores loaded shell presets
        mutable std::vector<ShellThemePreset> m_shellPresets;
        // Stores loaded markdown presets
        mutable std::vector<MarkdownThemePreset> m_markdownPresets;
    };
}
