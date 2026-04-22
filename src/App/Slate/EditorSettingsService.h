#pragma once

#include "App/Slate/SlateTypes.h"

#include <string>

namespace Software::Slate
{
    struct EditorSettings
    {
        int fontSize = 15;
        int lineSpacing = 3;
        int pageWidth = 980;
        bool wordWrap = true;
        bool highlightCurrentLine = true;
        int tabWidth = 4;
        bool indentWithTabs = false;
        bool autoListContinuation = true;
        bool pasteClipboardImages = true;
    };

    class EditorSettingsService
    {
    public:
        explicit EditorSettingsService(fs::path workspaceRoot = {});

        void SetWorkspaceRoot(fs::path workspaceRoot);
        fs::path SettingsPath() const;

        bool Load(EditorSettings* out, std::string* error = nullptr) const;
        bool Save(const EditorSettings& settings, std::string* error = nullptr) const;

        static EditorSettings DefaultSettings();
        static EditorSettings Sanitize(const EditorSettings& settings);

    private:
        fs::path m_workspaceRoot;
    };
}
