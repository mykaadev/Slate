#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    // Scintilla style ids produced by the live preview lexer. 32-39 are Scintilla's predefined styles.
    namespace LivePreviewStyle
    {
        enum : int
        {
            Default = 0,
            Hidden = 1,
            Markup = 2,
            Heading1 = 3,
            Heading2 = 4,
            Heading3 = 5,
            Heading4 = 6,
            Heading5 = 7,
            Heading6 = 8,
            Bold = 9,
            Italic = 10,
            BoldItalic = 11,
            Code = 12,
            CodeBlock = 13,
            Fence = 14,
            Link = 15,
            Url = 16,
            WikiLink = 17,
            Bullet = 18,
            Task = 19,
            TaskDone = 20,
            DoneText = 21,
            Quote = 22,
            QuoteMarker = 23,
            Rule = 24,
            Table = 25,
            Frontmatter = 26,
            Image = 27,
            Tag = 28,
            Strike = 29,
            Highlight = 30,
            Callout = 31,
            Comment = 40,
        };

        // Indicator used to draw strikethrough over completed tasks and ~~text~~
        constexpr int StrikeIndicator = 8;
    }

    // Styles one line; kept free of Scintilla so the rules are unit testable
    struct LivePreviewLine
    {
        // Stores one style per byte of the line text
        std::vector<char> styles;
        // Stores [start, end) byte ranges that get the strikethrough indicator
        std::vector<std::pair<std::size_t, std::size_t>> strikes;
        // Stores the block state carried into the next line
        int stateOut = 0;
    };

    // Styles one line of Markdown. stateIn carries fences and frontmatter between lines.
    LivePreviewLine StyleLivePreviewLine(std::string_view text, bool firstLine, int stateIn, bool reveal);

    // Returns the link target under a byte column: wiki targets as written, URLs and paths from [text](url).
    bool LinkTargetAt(std::string_view line, std::size_t column, std::string* target, bool* isWikiLink);

    // Returns the byte column of the checkbox mark ([ ] or [x]) under a column, if any.
    bool TaskMarkAt(std::string_view line, std::size_t column, std::size_t* markColumn);

    // Opaque handle to the Scintilla lexer object so headers stay free of Scintilla includes
    class LivePreviewLexerHandle
    {
    public:
        // Creates the lexer; the returned pointer is owned by Scintilla once passed to SCI_SETILEXER
        static void* Create(LivePreviewLexerHandle* handle);

        // Sets which lines show raw markup; others hide it
        void SetRevealRange(long long firstLine, long long lastLine);
        // Turns hiding on or off; when off every line shows its markup
        void SetHideMarkup(bool hide);
        // Reports whether a line currently shows markup
        bool Reveals(long long line) const;

    private:
        friend class LivePreviewLexer;
        long long m_revealFirst = -1;
        long long m_revealLast = -1;
        bool m_hideMarkup = true;
    };
}
