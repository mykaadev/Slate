#pragma once

#include "App/Slate/Editor/Live/CodeHighlighter.h"

#include "imgui.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate::Live
{
    // What an atom draws; text atoms draw their bytes, the rest draw shapes
    enum class AtomKind : std::uint8_t
    {
        Text,
        Bullet,
        Checkbox,
        QuoteBar,
        Rule
    };

    // Code tokens share the style space with Markdown styles, offset by this value
    constexpr int CodeStyleBase = 100;

    // One drawable piece of a line: a code point or a decoration
    struct Atom
    {
        std::uint32_t start = 0;
        std::uint32_t end = 0;
        float x = 0.0f;
        float width = 0.0f;
        std::uint32_t row = 0;
        std::uint8_t style = 0;
        AtomKind kind = AtomKind::Text;
        bool checked = false;
    };

    // One visual row after wrapping
    struct Row
    {
        float y = 0.0f;
        float height = 0.0f;
        float textHeight = 0.0f;
        std::uint32_t firstAtom = 0;
        std::uint32_t atomCount = 0;
        std::uint32_t startOffset = 0;
        std::uint32_t endOffset = 0;
    };

    struct InlineOptions
    {
        // Show syntax characters instead of rendering them
        bool reveal = false;
        // Treat the text as source code: no Markdown, wrap anywhere
        bool code = false;
        std::string language;
        int codeState = 0;
        float fontSize = 16.0f;
        float wrapWidth = 600.0f;
        float lineGap = 4.0f;
        // Draw headings larger than body text
        bool headingScale = true;
        // Added to every atom offset, so callout bodies keep source offsets
        std::uint32_t offsetBase = 0;
    };

    // A laid out line, in local coordinates with (0, 0) at its top-left
    struct InlineLayout
    {
        std::vector<Atom> atoms;
        std::vector<Row> rows;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> strikes;
        float height = 0.0f;
        float width = 0.0f;
        int headingLevel = 0;
        int quoteDepth = 0;
        bool isRule = false;
        int codeStateOut = 0;
        // Offset range of the source this layout covers
        std::uint32_t sourceStart = 0;
        std::uint32_t sourceEnd = 0;
    };

    // Returns the font size a heading level renders at
    float HeadingFontSize(int level, float baseSize, bool headingScale);

    // Lays out one line of Markdown or code
    InlineLayout LayoutInline(std::string_view text, const InlineOptions& options);

    // Draws a layout at origin; text is the same text that was laid out
    void DrawInline(ImDrawList* drawList, const InlineLayout& layout, std::string_view text, ImVec2 origin,
                    const InlineOptions& options, float alpha = 1.0f);

    // Returns the x position of a source offset inside a row, for carets and selections
    float OffsetX(const InlineLayout& layout, const Row& row, std::uint32_t offset);
    // Returns the source offset nearest to a local point
    std::uint32_t OffsetAtPoint(const InlineLayout& layout, ImVec2 local);
    // Returns the row index holding an offset (the last row that can hold it)
    std::size_t RowOfOffset(const InlineLayout& layout, std::uint32_t offset);
    // Returns the atom under a local point, or nullptr
    const Atom* AtomAtPoint(const InlineLayout& layout, ImVec2 local);

    // Theme lookups shared by the editor and plugin views
    ImU32 StyleColor(int style, float alpha = 1.0f);
    // Draws text in the editor font with optional faux bold and italic
    void DrawStyledText(ImDrawList* drawList, float size, ImVec2 position, ImU32 color, const char* begin,
                        const char* end, bool bold, bool italic);
}
