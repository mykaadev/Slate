#pragma once

#include "App/Slate/Editor/Live/InlineLayout.h"
#include "App/Slate/Editor/Live/MarkdownBlocks.h"
#include "App/Slate/Editor/Live/TextBuffer.h"

#include "imgui.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Software::Slate::Live
{
    // Per-frame editor configuration, derived from editor settings
    struct LiveEditorSettings
    {
        float fontSize = 16.0f;
        float lineGap = 4.0f;
        // Hide syntax and render blocks; off shows plain styled source everywhere
        bool livePreview = true;
        bool headingScale = true;
        int tabWidth = 4;
        bool indentWithTabs = false;
        bool smartLists = true;
        bool highlightLine = true;
        // Reading view: everything rendered, no caret
        bool readOnly = false;
        // Another widget (the slash palette) owns up, down, tab, enter, and escape this frame
        bool captureNavigation = false;
        // False while an overlay owns the keyboard; the editor keeps focus but ignores keys
        bool acceptInput = true;
        // Max width of the writing column; 0 fills the area
        float pageWidth = 0.0f;
    };

    // What the editor asks the app to do
    struct LiveEditorHooks
    {
        std::function<void(const std::string& target, bool isWikiLink)> openLink;
        std::function<void(const std::string& tag)> openTag;
        // Resolves an image target (relative to the note) to an absolute file path
        std::function<std::filesystem::path(const std::string& target)> resolveImage;
    };

    // A Markdown editor drawn with ImGui: blocks render as widgets and turn into
    // editable source when the caret or selection enters them.
    class LiveEditor
    {
    public:
        // Loads text and resets caret, scroll, and undo; key identifies the document
        void Load(std::string_view text, std::string lineEnding, std::string key);
        void Clear();
        const std::string& Key() const { return m_key; }
        bool HasDocument() const { return !m_key.empty(); }
        // Returns the text using the document's line endings
        std::string Text() const;
        std::uint64_t Revision() const { return m_buffer.Revision(); }

        // Draws the editor filling size and handles input when focused
        void Draw(const char* id, ImVec2 size, const LiveEditorSettings& settings, const LiveEditorHooks& hooks, double now);

        bool Focused() const { return m_focused; }
        void SetFocused(bool focused);

        // Editing API used by commands, slash commands, and plugins; all edits are undoable
        void InsertText(std::string_view text);
        bool SelectedText(std::string* text) const;
        bool HasSelection() const { return !m_selection.Empty(); }
        std::string CurrentLine() const;
        std::size_t CaretColumn() const;
        std::size_t CaretLine() const;
        void ReplaceCurrentLine(std::string_view text);
        void DeleteCurrentLine();
        void ReplaceAll(std::string_view text);
        // Moves the caret to a 1-based line and scrolls it into view
        void JumpToLine(std::size_t line);
        bool Undo();
        bool Redo();

        // Screen position just below the caret, for popups like the slash palette
        ImVec2 CaretScreenPos() const { return m_caretScreen; }
        float CaretHeight() const { return m_caretHeight; }
        std::size_t LineCount() const { return m_buffer.LineCount(); }
        std::size_t FirstVisibleLine() const { return m_firstVisibleLine; }

    private:
        enum class VisualKind
        {
            Rows,
            CodePanel,
            Table,
            Callout,
            Image,
            Properties,
            Math
        };

        // One positioned piece of the document for this frame
        struct Visual
        {
            VisualKind kind = VisualKind::Rows;
            std::size_t line = 0;
            std::size_t lineStart = 0;
            std::size_t block = 0;
            // Index into m_tables for tables
            std::size_t extra = 0;
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            const InlineLayout* layout = nullptr;
            InlineOptions options;
            bool revealed = false;
            float alpha = 1.0f;
            // Image texture data when kind is Image
            ImTextureID texture = 0;
            ImVec2 imageSize{};
            std::string target;
        };

        // A click target inside a rendered widget
        struct HitTarget
        {
            ImVec2 min;
            ImVec2 max;
            std::size_t offset = 0;
            int action = 0;
            std::size_t block = 0;
        };

        // A row the caret can stop on, in document order, for up and down movement
        struct CaretStop
        {
            float y = 0.0f;
            float height = 0.0f;
            std::size_t visual = 0;
            std::size_t row = 0;
            bool widget = false;
            std::size_t firstOffset = 0;
            std::size_t lastOffset = 0;
        };

        struct TableGeometry
        {
            std::vector<float> columnWidths;
            std::vector<float> rowHeights;
            std::vector<std::vector<const InlineLayout*>> cells;
            std::vector<std::vector<std::size_t>> cellOffsets;
            std::vector<std::vector<std::string_view>> cellTexts;
            std::vector<TableAlign> aligns;
            std::vector<std::size_t> rowLines;
        };

        const InlineLayout& CachedLayout(std::string_view text, const InlineOptions& options);
        void RebuildStructure();
        void BuildVisuals(float contentWidth, const LiveEditorSettings& settings, const LiveEditorHooks& hooks);
        void AddLineRows(std::size_t line, std::size_t textOffset, float x, float width, const InlineOptions& options,
                         float* y, float alpha = 1.0f);
        void DrawVisuals(ImDrawList* drawList, ImVec2 origin, float scrollY, float viewHeight, double now,
                         const LiveEditorSettings& settings);
        void DrawSelectionAndCaret(ImDrawList* drawList, ImVec2 origin, double now, const LiveEditorSettings& settings);

        void HandleKeyboard(const LiveEditorSettings& settings, double now);
        void HandleMouse(ImVec2 origin, const ImVec2& viewMin, const ImVec2& viewMax, const LiveEditorSettings& settings,
                         const LiveEditorHooks& hooks, double now);
        bool OffsetAtScreen(ImVec2 origin, ImVec2 point, std::size_t* offset) const;
        bool LocateCaret(std::size_t offset, std::size_t* visual, std::size_t* row) const;
        void MoveVertical(int delta, bool extend);

        void Edit(std::size_t start, std::size_t end, std::string_view text, EditKind kind, double now,
                  std::size_t caretAfter);
        void ReplaceSelection(std::string_view text, EditKind kind, double now);
        void SetCaret(std::size_t offset, bool extend);
        void InsertNewline(bool smart, double now);
        void Backspace(bool word, double now);
        void DeleteForward(bool word, double now);
        void IndentLines(bool outdent, double now);
        void ToggleWrap(std::string_view marker, double now);
        void MoveLines(int delta, double now);
        void ToggleTaskAt(std::size_t markOffset, double now);
        std::size_t LineHomeOffset(std::size_t line) const;

        TextBuffer m_buffer;
        Selection m_selection;
        std::string m_key;
        std::string m_lineEnding = "\n";
        bool m_focused = false;

        // Structure cache
        std::uint64_t m_structureRevision = 0;
        std::vector<std::string_view> m_lines;
        std::vector<Block> m_blocks;
        std::vector<std::size_t> m_lineBlock;

        // Layout cache and per-frame geometry
        std::unordered_map<std::uint64_t, InlineLayout> m_layoutCache;
        std::vector<Visual> m_visuals;
        std::vector<HitTarget> m_hits;
        std::vector<CaretStop> m_stops;
        std::vector<TableGeometry> m_tables;
        std::set<std::size_t> m_unfoldedCallouts;
        std::set<std::size_t> m_foldedCallouts;
        float m_contentHeight = 0.0f;
        float m_viewHeight = 0.0f;
        float m_scrollY = 0.0f;
        float m_lineHeight = 20.0f;
        float m_columnX = 0.0f;
        float m_desiredX = -1.0f;
        std::size_t m_firstVisibleLine = 0;

        // Caret and interaction state
        ImVec2 m_caretScreen{};
        float m_caretHeight = 0.0f;
        double m_lastInputTime = 0.0;
        bool m_scrollToCaret = false;
        bool m_centerCaret = false;
        bool m_dragging = false;
        int m_clickCount = 0;
        std::size_t m_dragOrigin = 0;
        std::size_t m_dragOriginEnd = 0;
        std::size_t m_revealFirst = 0;
        std::size_t m_revealLast = 0;
        bool m_hasReveal = false;
        std::uint64_t m_visualRevision = 0;
        bool m_resetScroll = false;
        double m_now = 0.0;
        float m_contentWidth = 600.0f;
        LiveEditorSettings m_settings;
    };
}
