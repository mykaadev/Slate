#pragma once

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Editor/SlashCommandService.h"

#include <cstdint>
#include <string>
#include <vector>

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
        SlashAccept = 1u << 9,
        SlashNext = 1u << 10,
        SlashPrevious = 1u << 11,
        SlashCancel = 1u << 12,
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
        // Reads the current line text and caret column inside that line.
        bool CurrentLineTextAndCaret(std::string* text, std::size_t* caretColumn) const;
        // Reports whether the native editor currently has an active text selection.
        bool HasSelection() const;
        // Replaces the current line text
        bool ReplaceCurrentLine(const std::string& text);
        // Deletes the current physical line
        bool DeleteCurrentLine();
        // Returns current scroll state
        ScrollState GetScrollState() const;
        // Sets the first visible line
        void SetFirstVisibleLine(int line);

        // Shows native slash-command suggestions without reopening the popup every frame.
        void ShowSlashCommandPalette(const std::vector<SlashCommandDefinition>& commands, int selectedIndex);
        // Hides native slash-command suggestions.
        void HideSlashCommandPalette();

        // Consumes one pending command bit
        bool ConsumeCommand(NativeEditorCommand command);
        // Consumes file paths dropped on the native editor
        bool ConsumeDroppedFiles(std::vector<std::string>* files);

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
        // Routes editor window messages into the host.
        static intptr_t __stdcall EditorWindowProc(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);
        // Routes the native slash-command palette paint messages.
        static intptr_t __stdcall SlashPaletteWindowProc(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);
        // Handles one editor window message.
        intptr_t HandleWindowMessage(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);
        // Creates the native slash-command palette popup when needed.
        bool EnsureSlashPaletteWindow();
        // Repositions and invalidates the native slash-command palette popup.
        void UpdateSlashPaletteWindow();
        // Paints the native slash-command palette with Slate tree-style rails and rows.
        void PaintSlashPalette(void* window);

        // Stores the parent window handle
        void* m_parentWindow = nullptr;
        // Stores the Scintilla window handle.
        void* m_editorWindow = nullptr;
        // Stores the previous window procedure.
        void* m_editorProc = nullptr;
        // Stores the native slash-command palette popup window.
        void* m_slashPaletteWindow = nullptr;
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
        // Stores file paths dropped on the native editor
        std::vector<std::string> m_pendingDroppedFiles;
        // Stores the last x position
        int m_lastX = 0;
        // Stores the last y position
        int m_lastY = 0;
        // Stores the last width
        int m_lastWidth = 0;
        // Stores the last height
        int m_lastHeight = 0;
        // Tracks whether the native slash-command popup is currently visible.
        bool m_slashPaletteVisible = false;
        // Stores the last native slash-command popup entries.
        std::vector<SlashCommandDefinition> m_slashPaletteCommands;
        // Stores the selected row in the native slash-command popup.
        int m_slashPaletteSelectedIndex = -1;
        // Stores the first visible row for the native slash-command popup.
        int m_slashPaletteScrollOffset = 0;
        // Suppresses slash-command key routing after Esc until the slash query changes.
        bool m_slashPaletteInputSuppressed = false;
        // Stores the slash query that was cancelled with Esc.
        std::string m_slashPaletteSuppressedQuery;
        // Suppresses the WM_CHAR generated by Tab after it is used to accept a slash command.
        bool m_suppressNextTabCharacter = false;
        // Stores the last caret position used for the native slash-command popup.
        std::intptr_t m_slashPalettePosition = -1;
    };
}
