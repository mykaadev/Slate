#pragma once

#include "App/Slate/DocumentService.h"
#include "App/Slate/EditorSettingsService.h"

#include <cstdint>
#include <string>

namespace Software::Slate
{
    enum class NativeEditorCommand : std::uint32_t
    {
        Save = 1u << 0,
        Find = 1u << 1,
        Escape = 1u << 2,
        PasteClipboardImage = 1u << 3,
    };

    class ScintillaEditorHost
    {
    public:
        ScintillaEditorHost();
        ~ScintillaEditorHost();

        void AttachToParentWindow(void* nativeHandle);

        bool Available() const;
        bool Visible() const;
        void SetVisible(bool visible);

        void Clear();
        void LoadDocument(const DocumentService::Document* document, bool forceReload = false);
        void Render(const DocumentService::Document& document, DocumentService& documents, double elapsedSeconds,
                    float screenX, float screenY, float width, float height);
        bool SyncDocument(DocumentService& documents, double elapsedSeconds);
        void MarkSaved();

        bool IsFocused() const;
        void Focus();
        void ReleaseFocus();
        void SetEditorSettings(const EditorSettings& settings);

        void JumpToLine(std::size_t line);
        bool InsertTextAtCursor(const std::string& text);

        bool ConsumeCommand(NativeEditorCommand command);

    private:
        bool EnsureWindow();
        void DestroyWindowResources();
        void ConfigureEditor();
        void ApplyThemeIfNeeded();
        void UpdateGeometry(float screenX, float screenY, float width, float height);
        std::string GetText() const;
        void SetText(const std::string& text);
        bool HandleSmartEnter();
        static std::uint32_t CommandMask(NativeEditorCommand command);

#if defined(_WIN32)
        static intptr_t __stdcall EditorWindowProc(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);
        intptr_t HandleWindowMessage(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);

        void* m_parentWindow = nullptr;
        void* m_editorWindow = nullptr;
        void* m_editorProc = nullptr;
#endif

        fs::path m_loadedPath;
        std::string m_lastSyncedText;
        std::string m_lineEnding = "\n";
        EditorSettings m_editorSettings = EditorSettingsService::DefaultSettings();
        bool m_visible = false;
        std::uint32_t m_pendingCommands = 0;
        int m_lastX = 0;
        int m_lastY = 0;
        int m_lastWidth = 0;
        int m_lastHeight = 0;
    };
}
