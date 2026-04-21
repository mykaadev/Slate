#include "App/Slate/ThemeService.h"

#include "App/Slate/PathUtils.h"
#include "App/Slate/UI/SlateUi.h"

#include <array>
#include <fstream>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        struct ShellThemePreset
        {
            const char* id = "";
            const char* label = "";
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
            const char* id = "";
            const char* label = "";
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

        std::string Escape(std::string_view value)
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

        std::string Unescape(std::string_view value)
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

        const std::array<ShellThemePreset, 3>& ShellPresets()
        {
            static const std::array<ShellThemePreset, 3> kPresets{{
                {
                    "slate-night",
                    "Slate Night",
                    ImVec4{0.055f, 0.055f, 0.052f, 1.0f},
                    ImVec4{0.86f, 0.84f, 0.77f, 1.0f},
                    ImVec4{0.48f, 0.48f, 0.43f, 1.0f},
                    ImVec4{0.50f, 0.78f, 0.86f, 1.0f},
                    ImVec4{0.93f, 0.70f, 0.35f, 1.0f},
                    ImVec4{0.58f, 0.82f, 0.56f, 1.0f},
                    ImVec4{0.92f, 0.38f, 0.36f, 1.0f},
                    ImVec4{0.085f, 0.085f, 0.08f, 1.0f},
                    ImVec4{0.70f, 0.75f, 0.78f, 1.0f},
                },
                {
                    "pine-night",
                    "Pine Night",
                    ImVec4{0.050f, 0.060f, 0.056f, 1.0f},
                    ImVec4{0.86f, 0.87f, 0.81f, 1.0f},
                    ImVec4{0.45f, 0.50f, 0.47f, 1.0f},
                    ImVec4{0.46f, 0.76f, 0.71f, 1.0f},
                    ImVec4{0.90f, 0.69f, 0.37f, 1.0f},
                    ImVec4{0.62f, 0.84f, 0.58f, 1.0f},
                    ImVec4{0.88f, 0.41f, 0.36f, 1.0f},
                    ImVec4{0.072f, 0.083f, 0.078f, 1.0f},
                    ImVec4{0.71f, 0.80f, 0.78f, 1.0f},
                },
                {
                    "ember-night",
                    "Ember Night",
                    ImVec4{0.061f, 0.052f, 0.051f, 1.0f},
                    ImVec4{0.88f, 0.83f, 0.78f, 1.0f},
                    ImVec4{0.52f, 0.45f, 0.40f, 1.0f},
                    ImVec4{0.47f, 0.74f, 0.82f, 1.0f},
                    ImVec4{0.95f, 0.62f, 0.28f, 1.0f},
                    ImVec4{0.62f, 0.80f, 0.52f, 1.0f},
                    ImVec4{0.94f, 0.39f, 0.34f, 1.0f},
                    ImVec4{0.092f, 0.077f, 0.074f, 1.0f},
                    ImVec4{0.77f, 0.76f, 0.81f, 1.0f},
                },
            }};
            return kPresets;
        }

        const std::array<MarkdownThemePreset, 3>& MarkdownPresets()
        {
            static const std::array<MarkdownThemePreset, 3> kPresets{{
                {
                    "vivid-notes",
                    "Vivid Notes",
                    ImVec4{0.56f, 0.84f, 0.90f, 1.0f},
                    ImVec4{0.70f, 0.88f, 0.68f, 1.0f},
                    ImVec4{0.94f, 0.73f, 0.42f, 1.0f},
                    ImVec4{0.84f, 0.69f, 0.42f, 1.0f},
                    ImVec4{0.88f, 0.74f, 0.45f, 1.0f},
                    ImVec4{0.57f, 0.86f, 0.60f, 1.0f},
                    ImVec4{0.63f, 0.72f, 0.78f, 1.0f},
                    ImVec4{0.78f, 0.74f, 0.56f, 1.0f},
                    ImVec4{0.50f, 0.80f, 0.96f, 1.0f},
                    ImVec4{0.92f, 0.62f, 0.51f, 1.0f},
                    ImVec4{0.96f, 0.92f, 0.84f, 1.0f},
                    ImVec4{0.76f, 0.80f, 0.67f, 1.0f},
                    ImVec4{0.78f, 0.83f, 0.86f, 1.0f},
                },
                {
                    "quiet-notes",
                    "Quiet Notes",
                    ImVec4{0.62f, 0.78f, 0.82f, 1.0f},
                    ImVec4{0.74f, 0.82f, 0.70f, 1.0f},
                    ImVec4{0.84f, 0.72f, 0.52f, 1.0f},
                    ImVec4{0.70f, 0.69f, 0.60f, 1.0f},
                    ImVec4{0.78f, 0.69f, 0.48f, 1.0f},
                    ImVec4{0.60f, 0.77f, 0.58f, 1.0f},
                    ImVec4{0.58f, 0.63f, 0.66f, 1.0f},
                    ImVec4{0.70f, 0.69f, 0.60f, 1.0f},
                    ImVec4{0.62f, 0.78f, 0.90f, 1.0f},
                    ImVec4{0.82f, 0.62f, 0.56f, 1.0f},
                    ImVec4{0.90f, 0.88f, 0.82f, 1.0f},
                    ImVec4{0.72f, 0.74f, 0.69f, 1.0f},
                    ImVec4{0.72f, 0.76f, 0.78f, 1.0f},
                },
                {
                    "sunrise-notes",
                    "Sunrise Notes",
                    ImVec4{0.55f, 0.82f, 0.90f, 1.0f},
                    ImVec4{0.94f, 0.68f, 0.43f, 1.0f},
                    ImVec4{0.88f, 0.79f, 0.47f, 1.0f},
                    ImVec4{0.74f, 0.80f, 0.65f, 1.0f},
                    ImVec4{0.93f, 0.68f, 0.39f, 1.0f},
                    ImVec4{0.57f, 0.84f, 0.58f, 1.0f},
                    ImVec4{0.66f, 0.72f, 0.82f, 1.0f},
                    ImVec4{0.84f, 0.76f, 0.58f, 1.0f},
                    ImVec4{0.48f, 0.83f, 0.95f, 1.0f},
                    ImVec4{0.93f, 0.58f, 0.48f, 1.0f},
                    ImVec4{0.98f, 0.91f, 0.83f, 1.0f},
                    ImVec4{0.77f, 0.82f, 0.67f, 1.0f},
                    ImVec4{0.77f, 0.81f, 0.88f, 1.0f},
                },
            }};
            return kPresets;
        }

        const ShellThemePreset& FindShellPreset(std::string_view id)
        {
            for (const auto& preset : ShellPresets())
            {
                if (id == preset.id)
                {
                    return preset;
                }
            }
            return ShellPresets().front();
        }

        const MarkdownThemePreset& FindMarkdownPreset(std::string_view id)
        {
            for (const auto& preset : MarkdownPresets())
            {
                if (id == preset.id)
                {
                    return preset;
                }
            }
            return MarkdownPresets().front();
        }
    }

    ThemeService::ThemeService(fs::path workspaceRoot) : m_workspaceRoot(std::move(workspaceRoot))
    {
    }

    void ThemeService::SetWorkspaceRoot(fs::path workspaceRoot)
    {
        m_workspaceRoot = std::move(workspaceRoot);
    }

    fs::path ThemeService::SettingsPath() const
    {
        return m_workspaceRoot.empty() ? fs::path() : (m_workspaceRoot / ".slate" / "theme.tsv");
    }

    bool ThemeService::Load(ThemeSettings* out, std::string* error) const
    {
        if (out)
        {
            *out = DefaultSettings();
        }

        const fs::path settingsPath = SettingsPath();
        if (settingsPath.empty())
        {
            return true;
        }

        std::ifstream file(settingsPath, std::ios::binary);
        if (!file)
        {
            return true;
        }

        ThemeSettings loaded = DefaultSettings();
        std::string line;
        while (std::getline(file, line))
        {
            const auto parts = SplitTabs(line);
            if (parts.size() < 2)
            {
                continue;
            }

            const std::string key = Unescape(parts[0]);
            const std::string value = Unescape(parts[1]);
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

        const auto shellId = std::string(FindShellPreset(settings.shellPreset).id);
        const auto markdownId = std::string(FindMarkdownPreset(settings.markdownPreset).id);
        file << "shell\t" << Escape(shellId) << '\n';
        file << "markdown\t" << Escape(markdownId) << '\n';
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

    const std::vector<std::string>& ThemeService::ShellPresetIds()
    {
        static const std::vector<std::string> ids = [] {
            std::vector<std::string> values;
            for (const auto& preset : ShellPresets())
            {
                values.emplace_back(preset.id);
            }
            return values;
        }();
        return ids;
    }

    const std::vector<std::string>& ThemeService::MarkdownPresetIds()
    {
        static const std::vector<std::string> ids = [] {
            std::vector<std::string> values;
            for (const auto& preset : MarkdownPresets())
            {
                values.emplace_back(preset.id);
            }
            return values;
        }();
        return ids;
    }

    std::string ThemeService::ShellPresetLabel(std::string_view id)
    {
        return FindShellPreset(id).label;
    }

    std::string ThemeService::MarkdownPresetLabel(std::string_view id)
    {
        return FindMarkdownPreset(id).label;
    }
}
