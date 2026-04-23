#include "App/Slate/Workspace/ThemeService.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Core/SettingsFile.h"
#include "App/Slate/UI/SlateUi.h"

#include <algorithm>
#include <fstream>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace Software::Slate
{
    namespace
    {
        fs::path ExecutableDirectory()
        {
#if defined(_WIN32)
            wchar_t buffer[MAX_PATH];
            const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            if (length > 0 && length < MAX_PATH)
            {
                return fs::path(buffer).parent_path();
            }
#endif
            return fs::current_path();
        }

        std::vector<fs::path> CandidatePresetRoots()
        {
            std::vector<fs::path> roots;
            roots.push_back(ExecutableDirectory() / "assets" / "slate" / "presets");
            roots.push_back(fs::current_path() / "assets" / "slate" / "presets");

            std::vector<fs::path> unique;
            for (const auto& path : roots)
            {
                const fs::path normalized = path.lexically_normal();
                if (std::find(unique.begin(), unique.end(), normalized) == unique.end())
                {
                    unique.push_back(normalized);
                }
            }
            return unique;
        }

        std::vector<fs::path> CollectPresetFiles(const fs::path& directory)
        {
            std::vector<fs::path> files;
            if (!fs::exists(directory))
            {
                return files;
            }

            for (const auto& entry : fs::directory_iterator(directory))
            {
                if (entry.is_regular_file())
                {
                    files.push_back(entry.path());
                }
            }

            std::sort(files.begin(), files.end());
            return files;
        }

        template <typename PresetType>
        void UpsertPreset(std::vector<PresetType>* presets, PresetType preset)
        {
            if (!presets || preset.id.empty())
            {
                return;
            }

            auto existing = std::find_if(presets->begin(), presets->end(), [&](const PresetType& candidate) {
                return candidate.id == preset.id;
            });
            if (existing == presets->end())
            {
                presets->push_back(std::move(preset));
            }
            else
            {
                *existing = std::move(preset);
            }
        }

        bool ParseShellPresetFile(const fs::path& path, ShellThemePreset* out)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }

            ShellThemePreset preset;
            std::string line;
            while (std::getline(file, line))
            {
                if (PathUtils::Trim(line).empty() || PathUtils::Trim(line).rfind("#", 0) == 0)
                {
                    continue;
                }

                const auto parts = SettingsFile::SplitTabs(line);
                if (parts.size() < 2)
                {
                    continue;
                }

                const std::string key = SettingsFile::Unescape(parts[0]);
                const std::string value = SettingsFile::Unescape(parts[1]);
                if (key == "id")
                {
                    preset.id = value;
                }
                else if (key == "label")
                {
                    preset.label = value;
                }
                else if (key == "background")
                {
                    SettingsFile::ParseColor(value, &preset.background);
                }
                else if (key == "primary")
                {
                    SettingsFile::ParseColor(value, &preset.primary);
                }
                else if (key == "muted")
                {
                    SettingsFile::ParseColor(value, &preset.muted);
                }
                else if (key == "cyan")
                {
                    SettingsFile::ParseColor(value, &preset.cyan);
                }
                else if (key == "amber")
                {
                    SettingsFile::ParseColor(value, &preset.amber);
                }
                else if (key == "green")
                {
                    SettingsFile::ParseColor(value, &preset.green);
                }
                else if (key == "red")
                {
                    SettingsFile::ParseColor(value, &preset.red);
                }
                else if (key == "panel")
                {
                    SettingsFile::ParseColor(value, &preset.panel);
                }
                else if (key == "code")
                {
                    SettingsFile::ParseColor(value, &preset.code);
                }
            }

            if (preset.id.empty())
            {
                preset.id = path.stem().string();
            }
            if (preset.label.empty())
            {
                preset.label = preset.id;
            }

            if (out)
            {
                *out = std::move(preset);
            }
            return static_cast<bool>(file) || file.eof();
        }

        bool ParseMarkdownPresetFile(const fs::path& path, MarkdownThemePreset* out)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }

            MarkdownThemePreset preset;
            std::string line;
            while (std::getline(file, line))
            {
                if (PathUtils::Trim(line).empty() || PathUtils::Trim(line).rfind("#", 0) == 0)
                {
                    continue;
                }

                const auto parts = SettingsFile::SplitTabs(line);
                if (parts.size() < 2)
                {
                    continue;
                }

                const std::string key = SettingsFile::Unescape(parts[0]);
                const std::string value = SettingsFile::Unescape(parts[1]);
                if (key == "id")
                {
                    preset.id = value;
                }
                else if (key == "label")
                {
                    preset.label = value;
                }
                else if (key == "heading1")
                {
                    SettingsFile::ParseColor(value, &preset.heading1);
                }
                else if (key == "heading2")
                {
                    SettingsFile::ParseColor(value, &preset.heading2);
                }
                else if (key == "heading3")
                {
                    SettingsFile::ParseColor(value, &preset.heading3);
                }
                else if (key == "bullet")
                {
                    SettingsFile::ParseColor(value, &preset.bullet);
                }
                else if (key == "checkbox")
                {
                    SettingsFile::ParseColor(value, &preset.checkbox);
                }
                else if (key == "checkbox_done")
                {
                    SettingsFile::ParseColor(value, &preset.checkboxDone);
                }
                else if (key == "quote")
                {
                    SettingsFile::ParseColor(value, &preset.quote);
                }
                else if (key == "table")
                {
                    SettingsFile::ParseColor(value, &preset.table);
                }
                else if (key == "link")
                {
                    SettingsFile::ParseColor(value, &preset.link);
                }
                else if (key == "image")
                {
                    SettingsFile::ParseColor(value, &preset.image);
                }
                else if (key == "bold")
                {
                    SettingsFile::ParseColor(value, &preset.bold);
                }
                else if (key == "italic")
                {
                    SettingsFile::ParseColor(value, &preset.italic);
                }
                else if (key == "code")
                {
                    SettingsFile::ParseColor(value, &preset.code);
                }
            }

            if (preset.id.empty())
            {
                preset.id = path.stem().string();
            }
            if (preset.label.empty())
            {
                preset.label = preset.id;
            }

            if (out)
            {
                *out = std::move(preset);
            }
            return static_cast<bool>(file) || file.eof();
        }

        void EnsureFallbackPresets(std::vector<ShellThemePreset>* shellPresets,
                                   std::vector<MarkdownThemePreset>* markdownPresets)
        {
            if (shellPresets && shellPresets->empty())
            {
                shellPresets->push_back(ShellThemePreset{
                    "slate-night",
                    "Slate Night",
                    UI::Background,
                    UI::Primary,
                    UI::Muted,
                    UI::Cyan,
                    UI::Amber,
                    UI::Green,
                    UI::Red,
                    UI::Panel,
                    UI::Code,
                });
            }

            if (markdownPresets && markdownPresets->empty())
            {
                markdownPresets->push_back(MarkdownThemePreset{
                    "vivid-notes",
                    "Vivid Notes",
                    UI::MarkdownHeading1,
                    UI::MarkdownHeading2,
                    UI::MarkdownHeading3,
                    UI::MarkdownBullet,
                    UI::MarkdownCheckbox,
                    UI::MarkdownCheckboxDone,
                    UI::MarkdownQuote,
                    UI::MarkdownTable,
                    UI::MarkdownLink,
                    UI::MarkdownImage,
                    UI::MarkdownBold,
                    UI::MarkdownItalic,
                    UI::MarkdownCode,
                });
            }
        }
    }

    ThemeService::ThemeService(fs::path workspaceRoot) : m_workspaceRoot(std::move(workspaceRoot))
    {
    }

    void ThemeService::SetWorkspaceRoot(fs::path workspaceRoot)
    {
        m_workspaceRoot = std::move(workspaceRoot);
        m_presetsLoaded = false;
        m_shellPresets.clear();
        m_markdownPresets.clear();
    }

    fs::path ThemeService::SettingsPath() const
    {
        return m_workspaceRoot.empty() ? fs::path() : (m_workspaceRoot / ".slate" / "theme.tsv");
    }

    void ThemeService::EnsurePresetsLoaded() const
    {
        if (m_presetsLoaded)
        {
            return;
        }

        m_shellPresets.clear();
        m_markdownPresets.clear();

        for (const auto& root : CandidatePresetRoots())
        {
            const fs::path shellDir = root / "shell";
            for (const auto& filePath : CollectPresetFiles(shellDir))
            {
                ShellThemePreset preset;
                if (ParseShellPresetFile(filePath, &preset))
                {
                    UpsertPreset(&m_shellPresets, std::move(preset));
                }
            }

            const fs::path markdownDir = root / "markdown";
            for (const auto& filePath : CollectPresetFiles(markdownDir))
            {
                MarkdownThemePreset preset;
                if (ParseMarkdownPresetFile(filePath, &preset))
                {
                    UpsertPreset(&m_markdownPresets, std::move(preset));
                }
            }
        }

        if (!m_workspaceRoot.empty())
        {
            const fs::path shellDir = m_workspaceRoot / ".slate" / "presets" / "shell";
            for (const auto& filePath : CollectPresetFiles(shellDir))
            {
                ShellThemePreset preset;
                if (ParseShellPresetFile(filePath, &preset))
                {
                    UpsertPreset(&m_shellPresets, std::move(preset));
                }
            }

            const fs::path markdownDir = m_workspaceRoot / ".slate" / "presets" / "markdown";
            for (const auto& filePath : CollectPresetFiles(markdownDir))
            {
                MarkdownThemePreset preset;
                if (ParseMarkdownPresetFile(filePath, &preset))
                {
                    UpsertPreset(&m_markdownPresets, std::move(preset));
                }
            }
        }

        EnsureFallbackPresets(&m_shellPresets, &m_markdownPresets);
        m_presetsLoaded = true;
    }

    const ShellThemePreset& ThemeService::FindShellPreset(std::string_view id) const
    {
        EnsurePresetsLoaded();
        auto it = std::find_if(m_shellPresets.begin(), m_shellPresets.end(), [&](const ShellThemePreset& preset) {
            return preset.id == id;
        });
        return it == m_shellPresets.end() ? m_shellPresets.front() : *it;
    }

    const MarkdownThemePreset& ThemeService::FindMarkdownPreset(std::string_view id) const
    {
        EnsurePresetsLoaded();
        auto it =
            std::find_if(m_markdownPresets.begin(), m_markdownPresets.end(), [&](const MarkdownThemePreset& preset) {
                return preset.id == id;
            });
        return it == m_markdownPresets.end() ? m_markdownPresets.front() : *it;
    }

    bool ThemeService::Load(ThemeSettings* out, std::string* error) const
    {
        EnsurePresetsLoaded();

        ThemeSettings loaded = DefaultSettings();
        const fs::path settingsPath = SettingsPath();
        if (!settingsPath.empty())
        {
            std::ifstream file(settingsPath, std::ios::binary);
            if (file)
            {
                std::string line;
                while (std::getline(file, line))
                {
                    const auto parts = SettingsFile::SplitTabs(line);
                    if (parts.size() < 2)
                    {
                        continue;
                    }

                    const std::string key = SettingsFile::Unescape(parts[0]);
                    const std::string value = SettingsFile::Unescape(parts[1]);
                    if (key == "shell")
                    {
                        loaded.shellPreset = value;
                    }
                    else if (key == "markdown")
                    {
                        loaded.markdownPreset = value;
                    }
                }

                if (file.bad())
                {
                    if (error)
                    {
                        *error = "Could not read theme settings.";
                    }
                    return false;
                }
            }
        }

        loaded.shellPreset = FindShellPreset(loaded.shellPreset).id;
        loaded.markdownPreset = FindMarkdownPreset(loaded.markdownPreset).id;
        if (out)
        {
            *out = std::move(loaded);
        }
        return true;
    }

    bool ThemeService::Save(const ThemeSettings& settings, std::string* error) const
    {
        EnsurePresetsLoaded();

        const fs::path settingsPath = SettingsPath();
        if (settingsPath.empty())
        {
            if (error)
            {
                *error = "No workspace available for theme settings.";
            }
            return false;
        }

        std::error_code ec;
        fs::create_directories(settingsPath.parent_path(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        std::ofstream file(settingsPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            if (error)
            {
                *error = "Could not write theme settings.";
            }
            return false;
        }

        file << "shell\t" << SettingsFile::Escape(FindShellPreset(settings.shellPreset).id) << '\n';
        file << "markdown\t" << SettingsFile::Escape(FindMarkdownPreset(settings.markdownPreset).id) << '\n';
        return static_cast<bool>(file);
    }

    void ThemeService::Apply(const ThemeSettings& settings) const
    {
        const auto& shell = FindShellPreset(settings.shellPreset);
        const auto& markdown = FindMarkdownPreset(settings.markdownPreset);

        UI::Background = shell.background;
        UI::Primary = shell.primary;
        UI::Muted = shell.muted;
        UI::Cyan = shell.cyan;
        UI::Amber = shell.amber;
        UI::Green = shell.green;
        UI::Red = shell.red;
        UI::Panel = shell.panel;
        UI::Code = shell.code;

        UI::MarkdownHeading1 = markdown.heading1;
        UI::MarkdownHeading2 = markdown.heading2;
        UI::MarkdownHeading3 = markdown.heading3;
        UI::MarkdownBullet = markdown.bullet;
        UI::MarkdownCheckbox = markdown.checkbox;
        UI::MarkdownCheckboxDone = markdown.checkboxDone;
        UI::MarkdownQuote = markdown.quote;
        UI::MarkdownTable = markdown.table;
        UI::MarkdownLink = markdown.link;
        UI::MarkdownImage = markdown.image;
        UI::MarkdownBold = markdown.bold;
        UI::MarkdownItalic = markdown.italic;
        UI::MarkdownCode = markdown.code;
    }

    ThemeSettings ThemeService::DefaultSettings()
    {
        return {"slate-night", "vivid-notes"};
    }

    std::vector<std::string> ThemeService::ShellPresetIds() const
    {
        EnsurePresetsLoaded();
        std::vector<std::string> ids;
        ids.reserve(m_shellPresets.size());
        for (const auto& preset : m_shellPresets)
        {
            ids.push_back(preset.id);
        }
        return ids;
    }

    std::vector<std::string> ThemeService::MarkdownPresetIds() const
    {
        EnsurePresetsLoaded();
        std::vector<std::string> ids;
        ids.reserve(m_markdownPresets.size());
        for (const auto& preset : m_markdownPresets)
        {
            ids.push_back(preset.id);
        }
        return ids;
    }

    std::string ThemeService::ShellPresetLabel(std::string_view id) const
    {
        return FindShellPreset(id).label;
    }

    std::string ThemeService::MarkdownPresetLabel(std::string_view id) const
    {
        return FindMarkdownPreset(id).label;
    }
}
