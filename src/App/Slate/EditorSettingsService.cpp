#include "App/Slate/EditorSettingsService.h"

#include "App/Slate/SettingsFile.h"

#include <algorithm>
#include <fstream>
#include <utility>

namespace Software::Slate
{
    EditorSettingsService::EditorSettingsService(fs::path workspaceRoot) : m_workspaceRoot(std::move(workspaceRoot))
    {
    }

    void EditorSettingsService::SetWorkspaceRoot(fs::path workspaceRoot)
    {
        m_workspaceRoot = std::move(workspaceRoot);
    }

    fs::path EditorSettingsService::SettingsPath() const
    {
        return m_workspaceRoot.empty() ? fs::path() : (m_workspaceRoot / ".slate" / "editor.tsv");
    }

    bool EditorSettingsService::Load(EditorSettings* out, std::string* error) const
    {
        EditorSettings settings = DefaultSettings();
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

                    if (key == "font_size")
                    {
                        SettingsFile::ParseInt(value, &settings.fontSize);
                    }
                    else if (key == "line_spacing")
                    {
                        SettingsFile::ParseInt(value, &settings.lineSpacing);
                    }
                    else if (key == "page_width")
                    {
                        SettingsFile::ParseInt(value, &settings.pageWidth);
                    }
                    else if (key == "word_wrap")
                    {
                        SettingsFile::ParseBool(value, &settings.wordWrap);
                    }
                    else if (key == "highlight_current_line")
                    {
                        SettingsFile::ParseBool(value, &settings.highlightCurrentLine);
                    }
                    else if (key == "tab_width")
                    {
                        SettingsFile::ParseInt(value, &settings.tabWidth);
                    }
                    else if (key == "indent_with_tabs")
                    {
                        SettingsFile::ParseBool(value, &settings.indentWithTabs);
                    }
                    else if (key == "auto_list_continuation")
                    {
                        SettingsFile::ParseBool(value, &settings.autoListContinuation);
                    }
                    else if (key == "paste_clipboard_images")
                    {
                        SettingsFile::ParseBool(value, &settings.pasteClipboardImages);
                    }
                }

                if (file.bad())
                {
                    if (error)
                    {
                        *error = "Could not read editor settings.";
                    }
                    return false;
                }
            }
        }

        if (out)
        {
            *out = Sanitize(settings);
        }
        return true;
    }

    bool EditorSettingsService::Save(const EditorSettings& settings, std::string* error) const
    {
        const fs::path settingsPath = SettingsPath();
        if (settingsPath.empty())
        {
            if (error)
            {
                *error = "No workspace available for editor settings.";
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
                *error = "Could not write editor settings.";
            }
            return false;
        }

        const EditorSettings sanitized = Sanitize(settings);
        file << "font_size\t" << SettingsFile::SerializeInt(sanitized.fontSize) << '\n';
        file << "line_spacing\t" << SettingsFile::SerializeInt(sanitized.lineSpacing) << '\n';
        file << "page_width\t" << SettingsFile::SerializeInt(sanitized.pageWidth) << '\n';
        file << "word_wrap\t" << SettingsFile::SerializeBool(sanitized.wordWrap) << '\n';
        file << "highlight_current_line\t" << SettingsFile::SerializeBool(sanitized.highlightCurrentLine) << '\n';
        file << "tab_width\t" << SettingsFile::SerializeInt(sanitized.tabWidth) << '\n';
        file << "indent_with_tabs\t" << SettingsFile::SerializeBool(sanitized.indentWithTabs) << '\n';
        file << "auto_list_continuation\t" << SettingsFile::SerializeBool(sanitized.autoListContinuation) << '\n';
        file << "paste_clipboard_images\t" << SettingsFile::SerializeBool(sanitized.pasteClipboardImages) << '\n';
        return static_cast<bool>(file);
    }

    EditorSettings EditorSettingsService::DefaultSettings()
    {
        return {};
    }

    EditorSettings EditorSettingsService::Sanitize(const EditorSettings& settings)
    {
        EditorSettings sanitized = settings;
        sanitized.fontSize = std::clamp(sanitized.fontSize, 12, 24);
        sanitized.lineSpacing = std::clamp(sanitized.lineSpacing, 0, 8);
        sanitized.pageWidth = sanitized.pageWidth == 0 ? 0 : std::clamp(sanitized.pageWidth, 560, 1400);
        sanitized.tabWidth = std::clamp(sanitized.tabWidth, 2, 8);
        return sanitized;
    }
}
