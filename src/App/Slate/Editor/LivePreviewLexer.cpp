#include "App/Slate/Editor/LivePreviewLexer.h"

#if defined(_WIN32)
#include <cassert>
#include <string>
#include <string_view>

#include <ILexer.h>
#include <Scintilla.h>
#include <LexAccessor.h>
#include <DefaultLexer.h>
#endif

#include <algorithm>
#include <cctype>

namespace Software::Slate
{
    namespace
    {
        namespace S = LivePreviewStyle;

        // Line state layout: low byte fence length, bit 8 tilde fence, bit 9 frontmatter
        constexpr int FenceLengthMask = 0xFF;
        constexpr int FenceTildeBit = 1 << 8;
        constexpr int FrontmatterBit = 1 << 9;

        bool IsSpace(char ch)
        {
            return ch == ' ' || ch == '\t';
        }

        bool IsWordByte(char ch)
        {
            const auto byte = static_cast<unsigned char>(ch);
            return std::isalnum(byte) != 0 || byte >= 0x80;
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && IsSpace(text.front()))
            {
                text.remove_prefix(1);
            }
            while (!text.empty() && (IsSpace(text.back()) || text.back() == '\r' || text.back() == '\n'))
            {
                text.remove_suffix(1);
            }
            return text;
        }

        // Counts a fence opener like ``` or ~~~ after at most three spaces of indent
        bool FenceAt(std::string_view text, char* fenceChar, std::size_t* length)
        {
            std::size_t pos = 0;
            while (pos < text.size() && pos < 3 && text[pos] == ' ')
            {
                ++pos;
            }
            if (pos >= text.size() || (text[pos] != '`' && text[pos] != '~'))
            {
                return false;
            }
            const char ch = text[pos];
            std::size_t count = 0;
            while (pos + count < text.size() && text[pos + count] == ch)
            {
                ++count;
            }
            if (count < 3)
            {
                return false;
            }
            *fenceChar = ch;
            *length = count;
            return true;
        }

        bool IsRule(std::string_view text)
        {
            const std::string_view trimmed = Trim(text);
            if (trimmed.size() < 3 || (trimmed[0] != '-' && trimmed[0] != '*' && trimmed[0] != '_'))
            {
                return false;
            }
            std::size_t marks = 0;
            for (const char ch : trimmed)
            {
                if (ch == trimmed[0])
                {
                    ++marks;
                }
                else if (!IsSpace(ch))
                {
                    return false;
                }
            }
            return marks >= 3;
        }

        // Combines an emphasis level with the surrounding style
        int EmphasisStyle(int base, std::size_t markers)
        {
            if (base >= S::Heading1 && base <= S::Heading6)
            {
                return base;
            }
            if (markers >= 3)
            {
                return S::BoldItalic;
            }
            if (markers == 2)
            {
                return base == S::Italic ? S::BoldItalic : S::Bold;
            }
            return base == S::Bold ? S::BoldItalic : S::Italic;
        }

        class LineStyler
        {
        public:
            LineStyler(std::string_view text, bool reveal, LivePreviewLine* out)
                : m_text(text), m_reveal(reveal), m_out(out)
            {
                m_out->styles.assign(text.size(), static_cast<char>(S::Default));
            }

            void Fill(std::size_t start, std::size_t end, int style)
            {
                end = std::min(end, m_text.size());
                for (std::size_t i = start; i < end; ++i)
                {
                    m_out->styles[i] = static_cast<char>(style);
                }
            }

            // Syntax characters: dimmed on revealed lines, hidden elsewhere
            void Markup(std::size_t start, std::size_t end)
            {
                Fill(start, end, m_reveal ? S::Markup : S::Hidden);
            }

            void Strike(std::size_t start, std::size_t end)
            {
                if (end > start)
                {
                    m_out->strikes.emplace_back(start, end);
                }
            }

            std::size_t Find(std::string_view needle, std::size_t from, std::size_t to) const
            {
                if (from >= to)
                {
                    return std::string_view::npos;
                }
                const std::size_t found = m_text.substr(0, to).find(needle, from);
                return found;
            }

            void Inline(std::size_t from, std::size_t to, int base)
            {
                std::size_t i = from;
                while (i < to)
                {
                    const char ch = m_text[i];
                    const char next = i + 1 < to ? m_text[i + 1] : '\0';

                    if (ch == '\\' && next != '\0' && std::ispunct(static_cast<unsigned char>(next)) != 0)
                    {
                        Markup(i, i + 1);
                        Fill(i + 1, i + 2, base);
                        i += 2;
                        continue;
                    }

                    if (ch == '`')
                    {
                        std::size_t run = 0;
                        while (i + run < to && m_text[i + run] == '`')
                        {
                            ++run;
                        }
                        const std::string ticks(run, '`');
                        std::size_t close = Find(ticks, i + run, to);
                        while (close != std::string_view::npos && close + run < to && m_text[close + run] == '`')
                        {
                            close = Find(ticks, close + run + 1, to);
                        }
                        if (close != std::string_view::npos)
                        {
                            Markup(i, i + run);
                            Fill(i + run, close, S::Code);
                            Markup(close, close + run);
                            i = close + run;
                        }
                        else
                        {
                            Fill(i, i + run, base);
                            i += run;
                        }
                        continue;
                    }

                    if (ch == '<' && m_text.substr(i, 4) == "<!--")
                    {
                        const std::size_t close = Find("-->", i + 4, to);
                        const std::size_t end = close == std::string_view::npos ? to : close + 3;
                        Fill(i, end, S::Comment);
                        i = end;
                        continue;
                    }

                    if (ch == '!' && next == '[')
                    {
                        const std::size_t closeText = Find("](", i + 2, to);
                        const std::size_t closeUrl = closeText == std::string_view::npos ? std::string_view::npos : Find(")", closeText + 2, to);
                        if (closeUrl != std::string_view::npos)
                        {
                            // Images cannot render inline, so the alt text stands in for them
                            if (closeText > i + 2)
                            {
                                Markup(i, i + 2);
                                Fill(i + 2, closeText, S::Image);
                                Markup(closeText, closeUrl + 1);
                            }
                            else
                            {
                                Fill(i, closeUrl + 1, S::Image);
                            }
                            i = closeUrl + 1;
                            continue;
                        }
                    }

                    if (ch == '[' && next == '[')
                    {
                        const std::size_t close = Find("]]", i + 2, to);
                        if (close != std::string_view::npos)
                        {
                            const std::size_t bar = Find("|", i + 2, close);
                            Markup(i, i + 2);
                            if (bar != std::string_view::npos)
                            {
                                Markup(i + 2, bar + 1);
                                Fill(bar + 1, close, S::WikiLink);
                            }
                            else
                            {
                                Fill(i + 2, close, S::WikiLink);
                            }
                            Markup(close, close + 2);
                            i = close + 2;
                            continue;
                        }
                    }

                    if (ch == '[')
                    {
                        const std::size_t closeText = Find("](", i + 1, to);
                        const std::size_t closeUrl = closeText == std::string_view::npos ? std::string_view::npos : Find(")", closeText + 2, to);
                        const bool simpleText = closeText != std::string_view::npos &&
                                                m_text.substr(i + 1, closeText - i - 1).find('[') == std::string_view::npos;
                        if (closeUrl != std::string_view::npos && simpleText)
                        {
                            Markup(i, i + 1);
                            Inline(i + 1, closeText, S::Link);
                            Markup(closeText, closeText + 2);
                            Fill(closeText + 2, closeUrl, m_reveal ? S::Url : S::Hidden);
                            Markup(closeUrl, closeUrl + 1);
                            i = closeUrl + 1;
                            continue;
                        }
                    }

                    if ((ch == 'h' || ch == 'H') && (i == from || !IsWordByte(m_text[i - 1])) &&
                        (m_text.substr(i, 7) == "http://" || m_text.substr(i, 8) == "https://"))
                    {
                        std::size_t end = i;
                        while (end < to && !IsSpace(m_text[end]) && m_text[end] != ')' && m_text[end] != '>')
                        {
                            ++end;
                        }
                        Fill(i, end, S::Link);
                        i = end;
                        continue;
                    }

                    if (ch == '*' || ch == '_')
                    {
                        std::size_t run = 0;
                        while (i + run < to && m_text[i + run] == ch)
                        {
                            ++run;
                        }
                        const bool opens = i + run < to && !IsSpace(m_text[i + run]) &&
                                           (ch == '*' || i == from || !IsWordByte(m_text[i - 1]));
                        if (opens && run <= 3)
                        {
                            const std::string marker(run, ch);
                            std::size_t close = Find(marker, i + run + 1, to);
                            while (close != std::string_view::npos &&
                                   (IsSpace(m_text[close - 1]) ||
                                    (close + run < to && m_text[close + run] == ch) ||
                                    (ch == '_' && close + run < to && IsWordByte(m_text[close + run]))))
                            {
                                close = Find(marker, close + 1, to);
                            }
                            if (close != std::string_view::npos)
                            {
                                Markup(i, i + run);
                                Inline(i + run, close, EmphasisStyle(base, run));
                                Markup(close, close + run);
                                i = close + run;
                                continue;
                            }
                        }
                        Fill(i, i + run, base);
                        i += run;
                        continue;
                    }

                    if ((ch == '~' || ch == '=') && next == ch)
                    {
                        const std::string marker(2, ch);
                        const std::size_t close = Find(marker, i + 2, to);
                        if (close != std::string_view::npos && close > i + 2)
                        {
                            Markup(i, i + 2);
                            Inline(i + 2, close, ch == '~' ? S::Strike : S::Highlight);
                            if (ch == '~')
                            {
                                Strike(i + 2, close);
                            }
                            Markup(close, close + 2);
                            i = close + 2;
                            continue;
                        }
                    }

                    if (ch == '#' && (i == 0 || IsSpace(m_text[i - 1])) && next != '\0' &&
                        (IsWordByte(next) || next == '_') && std::isdigit(static_cast<unsigned char>(next)) == 0)
                    {
                        std::size_t end = i + 1;
                        while (end < to && (IsWordByte(m_text[end]) || m_text[end] == '_' || m_text[end] == '-' || m_text[end] == '/'))
                        {
                            ++end;
                        }
                        Fill(i, end, S::Tag);
                        i = end;
                        continue;
                    }

                    Fill(i, i + 1, base);
                    ++i;
                }
            }

        private:
            std::string_view m_text;
            bool m_reveal = false;
            LivePreviewLine* m_out = nullptr;
        };
    }

    LivePreviewLine StyleLivePreviewLine(std::string_view text, bool firstLine, int stateIn, bool reveal)
    {
        LivePreviewLine line;
        LineStyler styler(text, reveal, &line);
        const std::size_t size = text.size();

        // Frontmatter only opens on the first line of the file
        if ((stateIn & FrontmatterBit) != 0 || (firstLine && Trim(text) == "---"))
        {
            styler.Fill(0, size, S::Frontmatter);
            const bool closes = (stateIn & FrontmatterBit) != 0 && (Trim(text) == "---" || Trim(text) == "...");
            line.stateOut = closes ? 0 : FrontmatterBit;
            return line;
        }

        const int openFence = stateIn & FenceLengthMask;
        if (openFence > 0)
        {
            char fenceChar = '\0';
            std::size_t length = 0;
            const char expected = (stateIn & FenceTildeBit) != 0 ? '~' : '`';
            if (FenceAt(text, &fenceChar, &length) && fenceChar == expected && length >= static_cast<std::size_t>(openFence) &&
                Trim(text).find_first_not_of(fenceChar) == std::string_view::npos)
            {
                styler.Fill(0, size, S::Fence);
                line.stateOut = 0;
            }
            else
            {
                styler.Fill(0, size, S::CodeBlock);
                line.stateOut = stateIn;
            }
            return line;
        }

        {
            char fenceChar = '\0';
            std::size_t length = 0;
            if (FenceAt(text, &fenceChar, &length))
            {
                styler.Fill(0, size, S::Fence);
                line.stateOut = static_cast<int>(std::min<std::size_t>(length, FenceLengthMask)) | (fenceChar == '~' ? FenceTildeBit : 0);
                return line;
            }
        }

        if (IsRule(text))
        {
            styler.Fill(0, size, S::Rule);
            return line;
        }

        std::size_t pos = 0;
        while (pos < size && IsSpace(text[pos]))
        {
            ++pos;
        }

        int base = S::Default;

        // Block quotes, possibly nested, with optional [!callout] headers
        while (pos < size && text[pos] == '>')
        {
            styler.Fill(pos, pos + 1, S::QuoteMarker);
            ++pos;
            if (pos < size && text[pos] == ' ')
            {
                ++pos;
            }
            base = S::Quote;
            while (pos < size && IsSpace(text[pos]))
            {
                ++pos;
            }
        }
        if (base == S::Quote && text.substr(pos, 2) == "[!")
        {
            const std::size_t close = text.find(']', pos);
            if (close != std::string_view::npos)
            {
                styler.Fill(pos, size, S::Callout);
                styler.Markup(pos, pos + 2);
                styler.Markup(close, close + 1);
                return line;
            }
        }

        // Headings hide their hashes away from the caret
        if (pos < size && text[pos] == '#' && pos <= 3)
        {
            std::size_t level = 0;
            while (pos + level < size && text[pos + level] == '#')
            {
                ++level;
            }
            if (level <= 6 && (pos + level == size || IsSpace(text[pos + level])))
            {
                std::size_t contentStart = pos + level;
                while (contentStart < size && IsSpace(text[contentStart]))
                {
                    ++contentStart;
                }
                styler.Markup(pos, contentStart);
                styler.Inline(contentStart, size, S::Heading1 + static_cast<int>(level) - 1);
                return line;
            }
        }

        // Tables keep their pipes so monospace columns stay aligned
        if (pos < size && text[pos] == '|')
        {
            const std::string_view trimmed = Trim(text);
            const bool separator = trimmed.find_first_not_of("|-: \t") == std::string_view::npos;
            if (separator)
            {
                styler.Fill(pos, size, S::Table);
                return line;
            }
            std::size_t cellStart = pos;
            for (std::size_t i = pos; i <= size; ++i)
            {
                if (i == size || (text[i] == '|' && (i == 0 || text[i - 1] != '\\')))
                {
                    if (i > cellStart)
                    {
                        styler.Inline(cellStart, i, base);
                    }
                    if (i < size)
                    {
                        styler.Fill(i, i + 1, S::Table);
                    }
                    cellStart = i + 1;
                }
            }
            return line;
        }

        // List markers and task checkboxes
        std::size_t markerEnd = pos;
        if (pos < size && (text[pos] == '-' || text[pos] == '*' || text[pos] == '+') && pos + 1 < size && IsSpace(text[pos + 1]))
        {
            markerEnd = pos + 1;
        }
        else
        {
            std::size_t digits = pos;
            while (digits < size && std::isdigit(static_cast<unsigned char>(text[digits])) != 0)
            {
                ++digits;
            }
            if (digits > pos && digits - pos <= 9 && digits + 1 < size && (text[digits] == '.' || text[digits] == ')') &&
                IsSpace(text[digits + 1]))
            {
                markerEnd = digits + 1;
            }
        }

        if (markerEnd > pos)
        {
            styler.Fill(pos, markerEnd, S::Bullet);
            pos = markerEnd;
            while (pos < size && IsSpace(text[pos]))
            {
                ++pos;
            }

            if (pos + 2 < size && text[pos] == '[' && text[pos + 2] == ']' &&
                (text[pos + 1] == ' ' || text[pos + 1] == 'x' || text[pos + 1] == 'X'))
            {
                const bool done = text[pos + 1] != ' ';
                styler.Fill(pos, pos + 3, done ? S::TaskDone : S::Task);
                pos += 3;
                if (done)
                {
                    std::size_t contentStart = pos;
                    while (contentStart < size && IsSpace(text[contentStart]))
                    {
                        ++contentStart;
                    }
                    styler.Inline(pos, size, S::DoneText);
                    styler.Strike(contentStart, size);
                    return line;
                }
            }
        }

        styler.Inline(pos, size, base);
        return line;
    }

    bool LinkTargetAt(std::string_view line, std::size_t column, std::string* target, bool* isWikiLink)
    {
        const auto finish = [&](std::string_view value, bool wiki) {
            if (target)
            {
                *target = std::string(Trim(value));
            }
            if (isWikiLink)
            {
                *isWikiLink = wiki;
            }
            return true;
        };

        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line.substr(i, 2) == "[[")
            {
                const std::size_t close = line.find("]]", i + 2);
                if (close == std::string_view::npos)
                {
                    break;
                }
                if (column >= i && column < close + 2)
                {
                    std::string_view inner = line.substr(i + 2, close - i - 2);
                    inner = inner.substr(0, inner.find('|'));
                    return finish(inner, true);
                }
                i = close + 1;
                continue;
            }

            if (line[i] == '[')
            {
                const std::size_t closeText = line.find("](", i + 1);
                const std::size_t closeUrl = closeText == std::string_view::npos ? std::string_view::npos : line.find(')', closeText + 2);
                if (closeUrl != std::string_view::npos)
                {
                    const std::size_t start = (i > 0 && line[i - 1] == '!') ? i - 1 : i;
                    if (column >= start && column <= closeUrl)
                    {
                        std::string_view url = line.substr(closeText + 2, closeUrl - closeText - 2);
                        url = url.substr(0, url.find(' '));
                        return finish(url, false);
                    }
                    i = closeUrl;
                    continue;
                }
            }

            if (line.substr(i, 7) == "http://" || line.substr(i, 8) == "https://")
            {
                std::size_t end = i;
                while (end < line.size() && !IsSpace(line[end]) && line[end] != ')' && line[end] != '>')
                {
                    ++end;
                }
                if (column >= i && column < end)
                {
                    return finish(line.substr(i, end - i), false);
                }
                i = end;
            }
        }
        return false;
    }

    bool TaskMarkAt(std::string_view line, std::size_t column, std::size_t* markColumn)
    {
        std::size_t pos = 0;
        while (pos < line.size() && (IsSpace(line[pos]) || line[pos] == '>'))
        {
            ++pos;
        }
        if (pos + 1 >= line.size() || (line[pos] != '-' && line[pos] != '*' && line[pos] != '+'))
        {
            std::size_t digits = pos;
            while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits])) != 0)
            {
                ++digits;
            }
            if (digits == pos || digits >= line.size() || (line[digits] != '.' && line[digits] != ')'))
            {
                return false;
            }
            pos = digits;
        }
        ++pos;
        while (pos < line.size() && IsSpace(line[pos]))
        {
            ++pos;
        }
        if (pos + 2 >= line.size() || line[pos] != '[' || line[pos + 2] != ']' ||
            (line[pos + 1] != ' ' && line[pos + 1] != 'x' && line[pos + 1] != 'X'))
        {
            return false;
        }
        if (column + 1 < pos || column > pos + 3)
        {
            return false;
        }
        if (markColumn)
        {
            *markColumn = pos + 1;
        }
        return true;
    }

    void LivePreviewLexerHandle::SetRevealRange(long long firstLine, long long lastLine)
    {
        m_revealFirst = firstLine;
        m_revealLast = lastLine;
    }

    void LivePreviewLexerHandle::SetHideMarkup(bool hide)
    {
        m_hideMarkup = hide;
    }

    bool LivePreviewLexerHandle::Reveals(long long line) const
    {
        return !m_hideMarkup || (line >= m_revealFirst && line <= m_revealLast);
    }

#if defined(_WIN32)
    // Scintilla lexer that asks the handle which lines reveal markup
    class LivePreviewLexer final : public Lexilla::DefaultLexer
    {
    public:
        explicit LivePreviewLexer(LivePreviewLexerHandle* handle)
            : Lexilla::DefaultLexer("slate-markdown", 0), m_handle(handle)
        {
        }

        void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position length, int, Scintilla::IDocument* document) override
        {
            const Sci_Position docLength = document->Length();
            const Sci_Position endPos = std::min<Sci_Position>(docLength, static_cast<Sci_Position>(startPos) + length);
            Sci_Position line = document->LineFromPosition(static_cast<Sci_Position>(startPos));
            const Sci_Position lastLine = document->LineFromPosition(std::max<Sci_Position>(static_cast<Sci_Position>(startPos), endPos - 1));
            int state = line > 0 ? document->GetLineState(line - 1) : 0;

            std::string text;
            for (; line <= lastLine; ++line)
            {
                const Sci_Position lineStart = document->LineStart(line);
                const Sci_Position contentEnd = document->LineEnd(line);
                const Sci_Position nextStart = std::min<Sci_Position>(docLength, document->LineStart(line + 1));
                text.resize(static_cast<std::size_t>(std::max<Sci_Position>(0, contentEnd - lineStart)));
                if (!text.empty())
                {
                    document->GetCharRange(text.data(), lineStart, static_cast<Sci_Position>(text.size()));
                }

                LivePreviewLine styled = StyleLivePreviewLine(text, line == 0, state, m_handle->Reveals(line));

                // Line endings share the style of the block they close so filled backgrounds reach the edge
                char eolStyle = static_cast<char>(LivePreviewStyle::Default);
                if (!styled.styles.empty())
                {
                    const char last = styled.styles.back();
                    if (last == LivePreviewStyle::CodeBlock || last == LivePreviewStyle::Fence || last == LivePreviewStyle::Frontmatter)
                    {
                        eolStyle = last;
                    }
                }
                else if ((state & FenceLengthMask) != 0 && (styled.stateOut & FenceLengthMask) != 0)
                {
                    eolStyle = static_cast<char>(LivePreviewStyle::CodeBlock);
                }
                styled.styles.resize(static_cast<std::size_t>(std::max<Sci_Position>(0, nextStart - lineStart)), eolStyle);

                document->StartStyling(lineStart);
                if (!styled.styles.empty())
                {
                    document->SetStyles(static_cast<Sci_Position>(styled.styles.size()), styled.styles.data());
                }
                document->SetLineState(line, styled.stateOut);

                document->DecorationSetCurrentIndicator(LivePreviewStyle::StrikeIndicator);
                document->DecorationFillRange(lineStart, 0, nextStart - lineStart);
                for (const auto& [start, end] : styled.strikes)
                {
                    document->DecorationFillRange(lineStart + static_cast<Sci_Position>(start), 1,
                                                  static_cast<Sci_Position>(end - start));
                }
                state = styled.stateOut;
            }
        }

    private:
        LivePreviewLexerHandle* m_handle = nullptr;
    };

    void* LivePreviewLexerHandle::Create(LivePreviewLexerHandle* handle)
    {
        return static_cast<Scintilla::ILexer5*>(new LivePreviewLexer(handle));
    }
#else
    void* LivePreviewLexerHandle::Create(LivePreviewLexerHandle*)
    {
        return nullptr;
    }
#endif
}
