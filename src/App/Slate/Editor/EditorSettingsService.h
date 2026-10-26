#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>

namespace Software::Slate
{
    struct EditorSettings
    {
        int fontSize = 16;
        int lineSpacing = 3;
        int pageWidth = 980;
        bool wordWrap = true;
        bool showWhitespace = false;
        bool highlightCurrentLine = true;
        int tabWidth = 4;
        int panelLayout = 0;
        int previewRenderMode = 0;
        int previewFollowMode = 3;
        int panelMotion = 2;
        int scrollMotion = 1;
        int scrollbarStyle = 1;
        int caretMotion = 1;
        bool linkUnderline = true;
        bool indentWithTabs = false;
        bool autoListContinuation = true;
        bool pasteClipboardImages = true;
        int imageStoragePolicy = static_cast<int>(ImageStoragePolicy::SubfolderUnderNoteFolder);
        // Renders Markdown in place and hides syntax away from the caret line
        bool livePreview = true;
        // Draws headings larger than body text; Scintilla grows every line to the tallest style
        bool headingScale = true;
        // Editor font family; must be installed on the system
        std::string fontFamily = "Cascadia Mono";
        // Scales every font in the app, in percent
        int interfaceScale = 100;
        // What opens on launch: 0 home, 1 last note, 2 today's journal
        int startScreen = 0;
        // Plays the short boot sequence on launch
        bool bootSequence = true;
        // Shows the clock in the status bar
        bool statusClock = true;
        // Blinks the text caret and the wordmark cursor
        bool cursorBlink = true;
        // Draws faint CRT scanlines over the app: 0 off, 1 subtle, 2 strong
        int scanlines = 0;
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
