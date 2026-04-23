#pragma once

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/EditorSettingsService.h"

#include <cstdint>
#include <string>

namespace Software::Slate
{
    // Lists commands emitted by the native editor
    enum class NativeEditorCommand : std::uint32_t
    {
        Save = 1u << 0,
        Find = 1u << 1,
        Escape = 1u << 2,
        PasteClipboardImage = 1u << 3,
        Outline = 1u << 4,
        Preview = 1u << 5,
        NavigateBack = 1u << 6,
        NavigateForward = 1u << 7,
        Todo = 1u << 8,
    };

    // Wraps the native Scintilla editor window
    class ScintillaEditorHost
    {
    public:
        // Stores viewport details pulled from Scintilla
        struct ScrollState
        {
            // Stores the first visible line
            int firstVisibleLine = 0;
            // Stores how many lines fit in view
            int visibleLineCount = 0;
            // Stores the total number of lines
            int totalLineCount = 0;
            // Stores the caret line
            int caretLine = 0;
        };

        // Builds the host
        ScintillaEditorHost();
        // Destroys the native editor window
        ~ScintillaEditorHost();

        // Attaches the editor to the app window
        void AttachToParentWindow(void* nativeHandle);

        // Reports whether the host can render
        bool Available() const;
        // Reports whether the editor window is visible
        bool Visible() const;
        // Shows or hides the editor window
        void SetVisible(bool visible);

        // Clears editor content and state
        void Clear();
        // Loads a document into the editor
        void LoadDocument(const DocumentService::Document* document, bool forceReload = false);
        // Renders the editor inside a screen rectangle
        void Render(const DocumentService::Document& document, DocumentService& documents, double elapsedSeconds,
                    float screenX, float screenY, float width, float height);
        // Syncs editor text back into the document service
        bool SyncDocument(DocumentService& documents, double elapsedSeconds);
        // Marks the current buffer as saved
        void MarkSaved();

        // Reports whether the native editor has focus
        bool IsFocused() const;
        // Gives keyboard focus to the native editor
        void Focus();
        // Returns keyboard focus to the host window
        void ReleaseFocus();
        // Applies editor settings to the native editor
        void SetEditorSettings(const EditorSettings& settings);

        // Moves the caret to a source line
        void JumpToLine(std::size_t line);
        // Inserts text at the current caret
        bool InsertTextAtCursor(const std::string& text);
        // Reads the current line text
        bool CurrentLineText(std::string* text) const;
        // Replaces the current line text
        bool ReplaceCurrentLine(const std::string& text);
        // Returns current scroll state
        ScrollState GetScrollState() const;
        // Sets the first visible line
        void SetFirstVisibleLine(int line);

        // Consumes one pending command bit
        bool ConsumeCommand(NativeEditorCommand command);

    private:
        // Creates the editor window on demand
        bool EnsureWindow();
        // Tears down native window resources
        void DestroyWindowResources();
        // Applies one time editor configuration
        void ConfigureEditor();
        // Reapplies theme colors when needed
        void ApplyThemeIfNeeded();
        // Moves the native window to match the UI layout
        void UpdateGeometry(float screenX, float screenY, float width, float height);
        // Pulls full text from Scintilla
        std::string GetText() const;
        // Replaces the full buffer text
        void SetText(const std::string& text);
        // Handles smart enter continuation rules
        bool HandleSmartEnter();
        // Converts one command into a bit mask
        static std::uint32_t CommandMask(NativeEditorCommand command);

#if defined(_WIN32)
        // Routes editor window messages into the host
        static intptr_t __stdcall EditorWindowProc(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);
        // Handles one window message
        intptr_t HandleWindowMessage(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);

        // Stores the parent window handle
        void* m_parentWindow = nullptr;
        // Stores the Scintilla window handle
        void* m_editorWindow = nullptr;
        // Stores the previous window procedure
        void* m_editorProc = nullptr;
#endif

        // Stores the path currently loaded into Scintilla
        fs::path m_loadedPath;
        // Stores the last text synced into the service layer
        std::string m_lastSyncedText;
        // Stores the line ending for smart enter behavior
        std::string m_lineEnding = "\n";
        // Stores the active editor settings
        EditorSettings m_editorSettings = EditorSettingsService::DefaultSettings();
        // Marks whether the native window is visible
        bool m_visible = false;
        // Stores pending command bits
        std::uint32_t m_pendingCommands = 0;
        // Stores the last x position
        int m_lastX = 0;
        // Stores the last y position
        int m_lastY = 0;
        // Stores the last width
        int m_lastWidth = 0;
        // Stores the last height
        int m_lastHeight = 0;
    };
}
