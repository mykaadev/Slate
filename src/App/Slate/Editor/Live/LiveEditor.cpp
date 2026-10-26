#include "App/Slate/Editor/Live/LiveEditor.h"

#include "App/Slate/Editor/LivePreviewLexer.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace Software::Slate::Live
{
    namespace
    {
        namespace S = LivePreviewStyle;
        using namespace Software::Slate::UI;

        constexpr int ActionCaret = 0;
        constexpr int ActionCopyCode = 1;
        constexpr int ActionToggleFold = 2;

        ImU32 U32(ImVec4 color, float alpha = 1.0f)
        {
            color.w *= alpha;
            return ImGui::ColorConvertFloat4ToU32(color);
        }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
        {
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f};
        }

        std::uint64_t HashCombine(std::uint64_t seed, std::uint64_t value)
        {
            return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
        }

        std::string_view TrimView(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            {
                text.remove_prefix(1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
            {
                text.remove_suffix(1);
            }
            return text;
        }

        // Structure of a list or quote prefix, used by smart enter, backspace, and home
        struct LinePrefix
        {
            std::string indent;
            std::string quotes;
            std::string marker;
            bool ordered = false;
            long long number = 0;
            char separator = '.';
            bool task = false;
            std::size_t contentStart = 0;
        };

        LinePrefix ParsePrefix(std::string_view line)
        {
            LinePrefix prefix;
            std::size_t pos = 0;
            while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
            {
                ++pos;
            }
            prefix.indent.assign(line.substr(0, pos));
            while (pos < line.size() && line[pos] == '>')
            {
                ++pos;
                if (pos < line.size() && line[pos] == ' ')
                {
                    ++pos;
                }
                prefix.quotes += "> ";
            }
            if (pos + 1 < line.size() && (line[pos] == '-' || line[pos] == '*' || line[pos] == '+') && line[pos + 1] == ' ')
            {
                prefix.marker = std::string(1, line[pos]) + " ";
                pos += 2;
            }
            else
            {
                std::size_t digits = pos;
                while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits])) != 0)
                {
                    ++digits;
                }
                if (digits > pos && digits - pos < 10 && digits + 1 < line.size() &&
                    (line[digits] == '.' || line[digits] == ')') && line[digits + 1] == ' ')
                {
                    prefix.ordered = true;
                    prefix.number = std::stoll(std::string(line.substr(pos, digits - pos)));
                    prefix.separator = line[digits];
                    prefix.marker = std::string(line.substr(pos, digits - pos + 2));
                    pos = digits + 2;
                }
            }
            if (!prefix.marker.empty() && pos + 2 < line.size() + 1 && line.substr(pos, 3).size() == 3 &&
                line[pos] == '[' && line[pos + 2] == ']' && (line[pos + 1] == ' ' || line[pos + 1] == 'x' || line[pos + 1] == 'X'))
            {
                prefix.task = true;
                pos += 3;
                if (pos < line.size() && line[pos] == ' ')
                {
                    ++pos;
                }
            }
            prefix.contentStart = pos;
            return prefix;
        }

        struct CalloutStyle
        {
            ImVec4 color;
            const char* icon;
        };

        CalloutStyle CalloutLook(const std::string& type)
        {
            const auto is = [&type](std::initializer_list<const char*> names) {
                return std::any_of(names.begin(), names.end(), [&type](const char* name) { return type == name; });
            };
            if (is({"tip", "hint", "important", "success", "check", "done"}))
            {
                return {Green, "*"};
            }
            if (is({"question", "help", "faq"}))
            {
                return {Amber, "?"};
            }
            if (is({"warning", "caution", "attention"}))
            {
                return {Amber, "!"};
            }
            if (is({"failure", "fail", "missing", "danger", "error", "bug"}))
            {
                return {Red, "x"};
            }
            if (is({"example"}))
            {
                return {MarkdownImage, "#"};
            }
            if (is({"quote", "cite"}))
            {
                return {Muted, "\""};
            }
            if (is({"abstract", "summary", "tldr"}))
            {
                return {UI::MarkdownLink, "="};
            }
            return {Cyan, "i"};
        }

        bool IsImageExtension(std::string_view target)
        {
            std::string lower(target.substr(target.find_last_of('.') == std::string_view::npos ? target.size()
                                                                                              : target.find_last_of('.')));
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return lower == ".png" || lower == ".jpg" || lower == ".jpeg" || lower == ".gif" || lower == ".bmp" ||
                   lower == ".webp" || lower == ".tga";
        }

        // Finds the first image on a line: ![alt](target) or ![[file.png]]
        bool FindImage(std::string_view line, std::string* target, bool* onlyImage)
        {
            const std::size_t embed = line.find("![[");
            if (embed != std::string_view::npos)
            {
                const std::size_t close = line.find("]]", embed + 3);
                if (close != std::string_view::npos)
                {
                    std::string_view inner = line.substr(embed + 3, close - embed - 3);
                    inner = inner.substr(0, inner.find('|'));
                    if (IsImageExtension(inner))
                    {
                        *target = std::string(inner);
                        *onlyImage = TrimView(line) == line.substr(embed, close + 2 - embed);
                        return true;
                    }
                }
            }
            const std::size_t open = line.find("![");
            if (open == std::string_view::npos)
            {
                return false;
            }
            const std::size_t closeText = line.find("](", open + 2);
            const std::size_t closeUrl = closeText == std::string_view::npos ? std::string_view::npos : line.find(')', closeText + 2);
            if (closeUrl == std::string_view::npos)
            {
                return false;
            }
            std::string_view url = TrimView(line.substr(closeText + 2, closeUrl - closeText - 2));
            if (!url.empty() && url.front() == '<')
            {
                url = url.substr(1, url.find('>') == std::string_view::npos ? url.size() - 1 : url.find('>') - 1);
            }
            else
            {
                url = url.substr(0, url.find(' '));
            }
            *target = std::string(url);
            *onlyImage = TrimView(line) == line.substr(open, closeUrl + 1 - open);
            return !target->empty();
        }

        float TextWidth(float size, std::string_view text)
        {
            return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text.data(), text.data() + text.size()).x;
        }
    }

    // ------------------------------------------------------------------ document

    void LiveEditor::Load(std::string_view text, std::string lineEnding, std::string key)
    {
        m_buffer.SetText(text);
        m_lineEnding = lineEnding.empty() ? "\n" : std::move(lineEnding);
        m_key = std::move(key);
        m_selection = Selection::At(0);
        m_structureRevision = 0;
        m_layoutCache.clear();
        m_visuals.clear();
        m_stops.clear();
        m_hits.clear();
        m_foldedCallouts.clear();
        m_unfoldedCallouts.clear();
        m_desiredX = -1.0f;
        m_scrollY = 0.0f;
        m_scrollToCaret = false;
        m_centerCaret = false;
        m_dragging = false;
        m_resetScroll = true;
    }

    void LiveEditor::Clear()
    {
        Load("", "\n", "");
        m_focused = false;
    }

    std::string LiveEditor::Text() const
    {
        return m_buffer.TextWithLineEnding(m_lineEnding);
    }

    void LiveEditor::SetFocused(bool focused)
    {
        m_focused = focused;
        if (focused)
        {
            m_scrollToCaret = true;
        }
    }

    // ------------------------------------------------------------------ editing API

    void LiveEditor::InsertText(std::string_view text)
    {
        std::string normalized;
        normalized.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\r')
            {
                normalized.push_back('\n');
                if (i + 1 < text.size() && text[i + 1] == '\n')
                {
                    ++i;
                }
                continue;
            }
            normalized.push_back(text[i]);
        }
        ReplaceSelection(normalized, EditKind::Other, m_now);
    }

    bool LiveEditor::SelectedText(std::string* text) const
    {
        if (!text)
        {
            return false;
        }
        text->assign(m_buffer.Text().substr(m_selection.Min(), m_selection.Max() - m_selection.Min()));
        return true;
    }

    std::string LiveEditor::CurrentLine() const
    {
        return std::string(m_buffer.Line(CaretLine()));
    }

    std::size_t LiveEditor::CaretLine() const
    {
        return m_buffer.LineOfOffset(m_selection.caret);
    }

    std::size_t LiveEditor::CaretColumn() const
    {
        return m_selection.caret - m_buffer.LineStart(CaretLine());
    }

    void LiveEditor::ReplaceCurrentLine(std::string_view text)
    {
        const std::size_t line = CaretLine();
        const std::size_t start = m_buffer.LineStart(line);
        std::string normalized(text);
        normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());
        Edit(start, m_buffer.LineEnd(line), normalized, EditKind::Other, m_now, start + normalized.size());
    }

    void LiveEditor::DeleteCurrentLine()
    {
        const std::size_t line = CaretLine();
        const std::size_t start = m_buffer.LineStart(line);
        std::size_t end = m_buffer.LineEnd(line);
        std::size_t from = start;
        if (end < m_buffer.Size())
        {
            ++end;
        }
        else if (start > 0)
        {
            from = start - 1;
        }
        Edit(from, end, "", EditKind::Other, m_now, from);
    }

    void LiveEditor::ReplaceAll(std::string_view text)
    {
        std::string normalized(text);
        normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());
        const std::size_t caret = std::min(m_selection.caret, normalized.size());
        Edit(0, m_buffer.Size(), normalized, EditKind::Other, m_now, caret);
    }

    void LiveEditor::JumpToLine(std::size_t line)
    {
        const std::size_t index = std::min(line > 0 ? line - 1 : 0, m_buffer.LineCount() - 1);
        m_selection = Selection::At(m_buffer.LineEnd(index));
        m_buffer.BreakUndoGroup();
        m_scrollToCaret = true;
        m_centerCaret = true;
        m_focused = true;
        m_desiredX = -1.0f;
    }

    bool LiveEditor::Undo()
    {
        Selection selection;
        if (!m_buffer.Undo(&selection))
        {
            return false;
        }
        m_selection = {std::min(selection.anchor, m_buffer.Size()), std::min(selection.caret, m_buffer.Size())};
        m_scrollToCaret = true;
        return true;
    }

    bool LiveEditor::Redo()
    {
        Selection selection;
        if (!m_buffer.Redo(&selection))
        {
            return false;
        }
        m_selection = {std::min(selection.anchor, m_buffer.Size()), std::min(selection.caret, m_buffer.Size())};
        m_scrollToCaret = true;
        return true;
    }

    void LiveEditor::Edit(std::size_t start, std::size_t end, std::string_view text, EditKind kind, double now,
                          std::size_t caretAfter)
    {
        const Selection before = m_selection;
        const Selection after = Selection::At(caretAfter);
        m_buffer.Replace(start, end, text, before, after, kind, now);
        m_selection = Selection::At(std::min(caretAfter, m_buffer.Size()));
        m_scrollToCaret = true;
        m_lastInputTime = now;
        m_desiredX = -1.0f;
    }

    void LiveEditor::ReplaceSelection(std::string_view text, EditKind kind, double now)
    {
        const std::size_t start = m_selection.Min();
        Edit(start, m_selection.Max(), text, kind, now, start + text.size());
    }

    void LiveEditor::SetCaret(std::size_t offset, bool extend)
    {
        m_selection.caret = std::min(offset, m_buffer.Size());
        if (!extend)
        {
            m_selection.anchor = m_selection.caret;
        }
        m_buffer.BreakUndoGroup();
        m_scrollToCaret = true;
        m_lastInputTime = m_now;
    }

    std::size_t LiveEditor::LineHomeOffset(std::size_t line) const
    {
        const LinePrefix prefix = ParsePrefix(m_buffer.Line(line));
        return m_buffer.LineStart(line) + prefix.contentStart;
    }

    void LiveEditor::InsertNewline(bool smart, double now)
    {
        const std::size_t line = m_buffer.LineOfOffset(m_selection.Min());
        const std::size_t lineStart = m_buffer.LineStart(line);
        const std::string_view text = m_buffer.Line(line);
        const bool inCode = line < m_lineBlock.size() && m_lineBlock[line] < m_blocks.size() &&
                            m_blocks[m_lineBlock[line]].kind == BlockKind::Code;

        if (inCode || !smart || !m_settings.smartLists)
        {
            std::size_t indent = 0;
            while (indent < text.size() && (text[indent] == ' ' || text[indent] == '\t'))
            {
                ++indent;
            }
            const std::string insert = "\n" + std::string(text.substr(0, smart ? indent : 0));
            ReplaceSelection(insert, EditKind::Other, now);
            return;
        }

        const LinePrefix prefix = ParsePrefix(text);
        const bool hasPrefix = !prefix.marker.empty() || !prefix.quotes.empty();
        const std::size_t caretColumn = m_selection.Min() - lineStart;
        if (hasPrefix && m_selection.Empty() && TrimView(text.substr(prefix.contentStart)).empty() && caretColumn >= prefix.contentStart)
        {
            // Enter on an empty item ends the list: outdent nested items, otherwise drop the marker
            std::string replacement;
            if (!prefix.marker.empty() && !prefix.indent.empty())
            {
                const std::size_t remove = std::min<std::size_t>(prefix.indent.size(), prefix.indent[0] == '\t' ? 1 : m_settings.tabWidth);
                replacement = prefix.indent.substr(remove) + prefix.quotes + prefix.marker + (prefix.task ? "[ ] " : "");
            }
            else if (!prefix.marker.empty())
            {
                replacement = prefix.indent + prefix.quotes;
            }
            else
            {
                replacement = prefix.indent;
            }
            Edit(lineStart, m_buffer.LineEnd(line), replacement, EditKind::Other, now, lineStart + replacement.size());
            return;
        }

        std::string continuation = prefix.indent + prefix.quotes;
        if (prefix.ordered)
        {
            continuation += std::to_string(prefix.number + 1) + prefix.separator + " ";
        }
        else
        {
            continuation += prefix.marker;
        }
        if (prefix.task)
        {
            continuation += "[ ] ";
        }
        ReplaceSelection("\n" + continuation, EditKind::Other, now);
    }

    void LiveEditor::Backspace(bool word, double now)
    {
        if (!m_selection.Empty())
        {
            ReplaceSelection("", EditKind::Deleting, now);
            return;
        }
        const std::size_t caret = m_selection.caret;
        if (caret == 0)
        {
            return;
        }

        const std::size_t line = m_buffer.LineOfOffset(caret);
        const std::size_t lineStart = m_buffer.LineStart(line);
        if (!word && caret > lineStart)
        {
            const LinePrefix prefix = ParsePrefix(m_buffer.Line(line));
            // Backspace right after a list marker removes the marker, like Obsidian
            if ((!prefix.marker.empty() || !prefix.quotes.empty()) && caret == lineStart + prefix.contentStart)
            {
                const std::size_t from = lineStart + prefix.indent.size();
                Edit(from, caret, "", EditKind::Other, now, from);
                return;
            }
        }

        const std::size_t start = word ? m_buffer.PrevWord(caret) : m_buffer.PrevChar(caret);
        Edit(start, caret, "", EditKind::Deleting, now, start);
    }

    void LiveEditor::DeleteForward(bool word, double now)
    {
        if (!m_selection.Empty())
        {
            ReplaceSelection("", EditKind::Deleting, now);
            return;
        }
        const std::size_t caret = m_selection.caret;
        const std::size_t end = word ? m_buffer.NextWord(caret) : m_buffer.NextChar(caret);
        if (end > caret)
        {
            Edit(caret, end, "", EditKind::Deleting, now, caret);
        }
    }

    void LiveEditor::IndentLines(bool outdent, double now)
    {
        std::size_t firstLine = m_buffer.LineOfOffset(m_selection.Min());
        std::size_t lastLine = m_buffer.LineOfOffset(m_selection.Max());
        if (lastLine > firstLine && m_selection.Max() == m_buffer.LineStart(lastLine))
        {
            --lastLine;
        }

        const std::string unit = m_settings.indentWithTabs ? "\t" : std::string(static_cast<std::size_t>(m_settings.tabWidth), ' ');
        const std::size_t rangeStart = m_buffer.LineStart(firstLine);
        const std::size_t rangeEnd = m_buffer.LineEnd(lastLine);
        std::string result;
        long long caretShift = 0;
        long long anchorShift = 0;
        for (std::size_t line = firstLine; line <= lastLine; ++line)
        {
            std::string text(m_buffer.Line(line));
            long long delta = 0;
            if (outdent)
            {
                std::size_t remove = 0;
                if (!text.empty() && text[0] == '\t')
                {
                    remove = 1;
                }
                else
                {
                    while (remove < text.size() && remove < static_cast<std::size_t>(m_settings.tabWidth) && text[remove] == ' ')
                    {
                        ++remove;
                    }
                }
                text.erase(0, remove);
                delta = -static_cast<long long>(remove);
            }
            else
            {
                text.insert(0, unit);
                delta = static_cast<long long>(unit.size());
            }
            if (line == m_buffer.LineOfOffset(m_selection.caret))
            {
                caretShift = delta;
            }
            if (line == m_buffer.LineOfOffset(m_selection.anchor))
            {
                anchorShift = delta;
            }
            result += text;
            if (line < lastLine)
            {
                result.push_back('\n');
            }
        }

        const Selection before = m_selection;
        const auto shifted = [](std::size_t offset, long long delta, std::size_t floor) {
            const long long value = static_cast<long long>(offset) + delta;
            return static_cast<std::size_t>(std::max<long long>(static_cast<long long>(floor), value));
        };
        Selection after{shifted(before.anchor, anchorShift, m_buffer.LineStart(m_buffer.LineOfOffset(before.anchor))),
                        shifted(before.caret, caretShift, m_buffer.LineStart(m_buffer.LineOfOffset(before.caret)))};
        m_buffer.Replace(rangeStart, rangeEnd, result, before, after, EditKind::Other, now);
        m_selection = {std::min(after.anchor, m_buffer.Size()), std::min(after.caret, m_buffer.Size())};
        m_scrollToCaret = true;
        m_lastInputTime = now;
    }

    void LiveEditor::ToggleWrap(std::string_view marker, double now)
    {
        const std::string& text = m_buffer.Text();
        const std::size_t start = m_selection.Min();
        const std::size_t end = m_selection.Max();
        const std::string mark(marker);
        if (start >= mark.size() && end + mark.size() <= text.size() &&
            text.compare(start - mark.size(), mark.size(), mark) == 0 && text.compare(end, mark.size(), mark) == 0)
        {
            const std::string inner = text.substr(start, end - start);
            const Selection before = m_selection;
            const Selection after{start - mark.size(), end - mark.size()};
            m_buffer.Replace(start - mark.size(), end + mark.size(), inner, before, after, EditKind::Other, now);
            m_selection = after;
            return;
        }
        const std::string inner = text.substr(start, end - start);
        const Selection before = m_selection;
        const Selection after{start + mark.size(), start + mark.size() + inner.size()};
        m_buffer.Replace(start, end, mark + inner + mark, before, after, EditKind::Other, now);
        m_selection = after;
        m_scrollToCaret = true;
    }

    void LiveEditor::MoveLines(int delta, double now)
    {
        std::size_t firstLine = m_buffer.LineOfOffset(m_selection.Min());
        std::size_t lastLine = m_buffer.LineOfOffset(m_selection.Max());
        if (lastLine > firstLine && m_selection.Max() == m_buffer.LineStart(lastLine))
        {
            --lastLine;
        }
        if ((delta < 0 && firstLine == 0) || (delta > 0 && lastLine + 1 >= m_buffer.LineCount()))
        {
            return;
        }

        const std::string& text = m_buffer.Text();
        const std::string moving = text.substr(m_buffer.LineStart(firstLine), m_buffer.LineEnd(lastLine) - m_buffer.LineStart(firstLine));
        const Selection before = m_selection;
        if (delta < 0)
        {
            const std::size_t neighbour = firstLine - 1;
            const std::string other(m_buffer.Line(neighbour));
            const std::size_t start = m_buffer.LineStart(neighbour);
            const std::size_t shift = other.size() + 1;
            const Selection after{before.anchor - shift, before.caret - shift};
            m_buffer.Replace(start, m_buffer.LineEnd(lastLine), moving + "\n" + other, before, after, EditKind::Other, now);
            m_selection = after;
        }
        else
        {
            const std::size_t neighbour = lastLine + 1;
            const std::string other(m_buffer.Line(neighbour));
            const std::size_t start = m_buffer.LineStart(firstLine);
            const std::size_t shift = other.size() + 1;
            const Selection after{before.anchor + shift, before.caret + shift};
            m_buffer.Replace(start, m_buffer.LineEnd(neighbour), other + "\n" + moving, before, after, EditKind::Other, now);
            m_selection = after;
        }
        m_scrollToCaret = true;
    }

    void LiveEditor::ToggleTaskAt(std::size_t markOffset, double now)
    {
        if (markOffset >= m_buffer.Size())
        {
            return;
        }
        const char current = m_buffer.Text()[markOffset];
        const char replacement[2] = {current == ' ' ? 'x' : ' ', '\0'};
        const Selection before = m_selection;
        m_buffer.Replace(markOffset, markOffset + 1, replacement, before, before, EditKind::Other, now);
    }

    // ------------------------------------------------------------------ structure and layout

    const InlineLayout& LiveEditor::CachedLayout(std::string_view text, const InlineOptions& options)
    {
        std::uint64_t key = std::hash<std::string_view>{}(text);
        key = HashCombine(key, options.reveal ? 1 : 2);
        key = HashCombine(key, options.code ? 3 : 4);
        key = HashCombine(key, std::hash<std::string>{}(options.language));
        key = HashCombine(key, static_cast<std::uint64_t>(options.codeState));
        key = HashCombine(key, static_cast<std::uint64_t>(std::lround(options.fontSize * 10.0f)));
        key = HashCombine(key, static_cast<std::uint64_t>(std::lround(options.wrapWidth)));
        key = HashCombine(key, static_cast<std::uint64_t>(std::lround(options.lineGap * 10.0f)));
        key = HashCombine(key, options.headingScale ? 5 : 6);
        key = HashCombine(key, options.offsetBase);

        auto it = m_layoutCache.find(key);
        if (it == m_layoutCache.end())
        {
            it = m_layoutCache.emplace(key, LayoutInline(text, options)).first;
        }
        return it->second;
    }

    void LiveEditor::RebuildStructure()
    {
        if (m_structureRevision == m_buffer.Revision())
        {
            return;
        }
        m_structureRevision = m_buffer.Revision();
        m_lines.clear();
        m_lines.reserve(m_buffer.LineCount());
        for (std::size_t line = 0; line < m_buffer.LineCount(); ++line)
        {
            m_lines.push_back(m_buffer.Line(line));
        }
        m_blocks = ParseBlocks(m_lines);
        m_lineBlock.assign(m_lines.size(), 0);
        for (std::size_t b = 0; b < m_blocks.size(); ++b)
        {
            for (std::size_t line = m_blocks[b].firstLine; line <= m_blocks[b].lastLine && line < m_lines.size(); ++line)
            {
                m_lineBlock[line] = b;
            }
        }
    }

    void LiveEditor::AddLineRows(std::size_t line, std::size_t textOffset, float x, float width, const InlineOptions& base,
                                 float* y, float alpha)
    {
        InlineOptions options = base;
        options.wrapWidth = std::max(40.0f, width);
        options.offsetBase = static_cast<std::uint32_t>(textOffset);
        const std::string_view text = m_lines[line].substr(std::min(textOffset, m_lines[line].size()));
        const InlineLayout& layout = CachedLayout(text, options);

        Visual visual;
        visual.kind = VisualKind::Rows;
        visual.line = line;
        visual.lineStart = m_buffer.LineStart(line);
        visual.block = line < m_lineBlock.size() ? m_lineBlock[line] : 0;
        visual.x = x;
        visual.y = *y;
        visual.width = width;
        visual.height = layout.height;
        visual.layout = &layout;
        visual.options = options;
        visual.revealed = options.reveal || options.code;
        visual.alpha = alpha;
        const std::size_t index = m_visuals.size();
        m_visuals.push_back(visual);

        for (std::size_t r = 0; r < layout.rows.size(); ++r)
        {
            CaretStop stop;
            stop.y = *y + layout.rows[r].y;
            stop.height = layout.rows[r].height;
            stop.visual = index;
            stop.row = r;
            m_stops.push_back(stop);
        }
        *y += layout.height;
    }

    void LiveEditor::BuildVisuals(float contentWidth, const LiveEditorSettings& settings, const LiveEditorHooks& hooks)
    {
        if (m_layoutCache.size() > 6000)
        {
            m_layoutCache.clear();
        }
        m_visuals.clear();
        m_hits.clear();
        m_stops.clear();
        m_tables.clear();

        const float fontSize = settings.fontSize;
        const float gap = settings.lineGap;
        m_lineHeight = fontSize * 1.18f + gap;

        // Lines touched by the selection show their source
        m_hasReveal = m_focused && !settings.readOnly;
        m_revealFirst = m_buffer.LineOfOffset(m_selection.Min());
        m_revealLast = m_buffer.LineOfOffset(m_selection.Max());

        InlineOptions inlineBase;
        inlineBase.fontSize = fontSize;
        inlineBase.lineGap = gap;
        inlineBase.headingScale = settings.headingScale;

        float y = fontSize * 0.6f;
        for (std::size_t b = 0; b < m_blocks.size(); ++b)
        {
            const Block& block = m_blocks[b];
            const bool touched = m_hasReveal && block.lastLine >= m_revealFirst && block.firstLine <= m_revealLast;
            const bool revealed = !settings.livePreview || touched;

            switch (block.kind)
            {
            case BlockKind::Line:
            {
                const std::size_t line = block.firstLine;
                InlineOptions options = inlineBase;
                options.reveal = revealed;
                std::string target;
                bool onlyImage = false;
                const bool hasImage = settings.livePreview && FindImage(m_lines[line], &target, &onlyImage);
                if (!(hasImage && onlyImage && !revealed))
                {
                    AddLineRows(line, 0, 0.0f, contentWidth, options, &y);
                }
                if (hasImage)
                {
                    ImTextureID texture = 0;
                    ImVec2 size{};
                    const auto path = hooks.resolveImage ? hooks.resolveImage(target) : std::filesystem::path();
                    Visual image;
                    image.kind = VisualKind::Image;
                    image.line = line;
                    image.lineStart = m_buffer.LineStart(line);
                    image.block = b;
                    image.target = target;
                    image.y = y + gap;
                    if (LookupImageTexture(path, target, &texture, &size))
                    {
                        const float scale = std::min({1.0f, contentWidth / size.x, 520.0f / size.y});
                        image.texture = texture;
                        image.imageSize = ImVec2(size.x * scale, size.y * scale);
                    }
                    else
                    {
                        image.imageSize = ImVec2(std::min(contentWidth, 360.0f), fontSize * 3.0f);
                    }
                    image.width = image.imageSize.x;
                    image.height = image.imageSize.y;
                    m_hits.push_back({ImVec2(0.0f, image.y), ImVec2(image.width, image.y + image.height),
                                      m_buffer.LineEnd(line), ActionCaret, b});
                    CaretStop stop;
                    stop.y = image.y;
                    stop.height = image.height;
                    stop.widget = true;
                    stop.firstOffset = m_buffer.LineStart(line);
                    stop.lastOffset = m_buffer.LineEnd(line);
                    if (onlyImage && !revealed)
                    {
                        m_stops.push_back(stop);
                    }
                    m_visuals.push_back(image);
                    y = image.y + image.height + gap * 2.0f;
                }
                break;
            }

            case BlockKind::Code:
            {
                const float pad = fontSize * 0.75f;
                Visual panel;
                panel.kind = VisualKind::CodePanel;
                panel.block = b;
                panel.line = block.firstLine;
                panel.lineStart = m_buffer.LineStart(block.firstLine);
                panel.revealed = revealed;
                panel.y = y + gap;
                panel.width = contentWidth;
                panel.target = block.language;
                const std::size_t panelIndex = m_visuals.size();
                m_visuals.push_back(panel);
                y = panel.y + pad;

                std::size_t first = revealed ? block.firstLine : block.firstLine + 1;
                std::size_t last = revealed || block.unclosed ? block.lastLine : (block.lastLine > 0 ? block.lastLine - 1 : 0);
                int state = 0;
                const bool empty = first > last || (!revealed && block.lastLine == block.firstLine);
                if (!empty)
                {
                    for (std::size_t line = first; line <= last; ++line)
                    {
                        InlineOptions options = inlineBase;
                        options.code = true;
                        const bool fence = line == block.firstLine || (line == block.lastLine && !block.unclosed);
                        options.language = fence ? std::string() : block.language;
                        options.codeState = fence ? 0 : state;
                        AddLineRows(line, 0, pad, contentWidth - pad * 2.0f, options, &y, fence ? 0.5f : 1.0f);
                        if (!fence)
                        {
                            state = m_visuals.back().layout->codeStateOut;
                        }
                    }
                }
                else
                {
                    CaretStop stop;
                    stop.y = y;
                    stop.height = m_lineHeight;
                    stop.widget = true;
                    stop.firstOffset = m_buffer.LineEnd(block.firstLine);
                    stop.lastOffset = stop.firstOffset;
                    m_stops.push_back(stop);
                    m_hits.push_back({ImVec2(0.0f, y), ImVec2(contentWidth, y + m_lineHeight), stop.firstOffset, ActionCaret, b});
                    y += m_lineHeight;
                }
                y += pad;
                m_visuals[panelIndex].height = y - m_visuals[panelIndex].y;
                if (!revealed)
                {
                    const float buttonWidth = TextWidth(fontSize * 0.82f, "copy") + fontSize;
                    const float top = m_visuals[panelIndex].y + fontSize * 0.25f;
                    m_hits.insert(m_hits.begin(), {ImVec2(contentWidth - buttonWidth - fontSize * 0.3f, top),
                                                   ImVec2(contentWidth - fontSize * 0.3f, top + fontSize * 1.3f), 0,
                                                   ActionCopyCode, b});
                }
                y += gap * 2.0f;
                break;
            }

            case BlockKind::Table:
            {
                if (revealed || block.lastLine < block.firstLine + 1)
                {
                    for (std::size_t line = block.firstLine; line <= block.lastLine; ++line)
                    {
                        InlineOptions options = inlineBase;
                        options.reveal = true;
                        AddLineRows(line, 0, 0.0f, contentWidth, options, &y);
                    }
                    break;
                }

                TableGeometry table;
                table.aligns = TableAlignments(m_lines[block.firstLine + 1]);
                for (std::size_t line = block.firstLine; line <= block.lastLine; ++line)
                {
                    if (line != block.firstLine + 1)
                    {
                        table.rowLines.push_back(line);
                    }
                }
                std::size_t columns = 0;
                std::vector<std::vector<TableCell>> rows;
                for (const std::size_t line : table.rowLines)
                {
                    rows.push_back(SplitTableRow(m_lines[line]));
                    columns = std::max(columns, rows.back().size());
                }
                columns = std::max<std::size_t>(columns, 1);

                const float padX = fontSize * 0.65f;
                const float padY = fontSize * 0.35f;
                InlineOptions cellOptions = inlineBase;
                cellOptions.reveal = false;
                cellOptions.headingScale = false;
                cellOptions.lineGap = gap * 0.5f;

                // Natural column widths, then shrink proportionally to fit the page
                table.columnWidths.assign(columns, fontSize * 3.0f);
                for (std::size_t r = 0; r < rows.size(); ++r)
                {
                    for (std::size_t c = 0; c < rows[r].size(); ++c)
                    {
                        InlineOptions natural = cellOptions;
                        natural.wrapWidth = 100000.0f;
                        natural.offsetBase = static_cast<std::uint32_t>(rows[r][c].offset);
                        const auto& layout = CachedLayout(rows[r][c].text, natural);
                        // A couple of pixels of slack keeps rounding from wrapping the widest cell
                        table.columnWidths[c] = std::max(table.columnWidths[c], std::ceil(layout.width) + padX * 2.0f + 2.0f);
                    }
                }
                float total = 0.0f;
                for (const float width : table.columnWidths)
                {
                    total += width;
                }
                if (total > contentWidth)
                {
                    const float scale = contentWidth / total;
                    for (auto& width : table.columnWidths)
                    {
                        width = std::max(fontSize * 3.5f, width * scale);
                    }
                }

                table.cells.assign(rows.size(), std::vector<const InlineLayout*>(columns, nullptr));
                table.cellOffsets.assign(rows.size(), std::vector<std::size_t>(columns, 0));
                table.cellTexts.assign(rows.size(), std::vector<std::string_view>(columns));
                table.rowHeights.assign(rows.size(), m_lineHeight + padY * 2.0f);
                for (std::size_t r = 0; r < rows.size(); ++r)
                {
                    float rowHeight = cellOptions.fontSize * 1.18f + cellOptions.lineGap;
                    for (std::size_t c = 0; c < rows[r].size(); ++c)
                    {
                        InlineOptions wrapped = cellOptions;
                        wrapped.wrapWidth = table.columnWidths[c] - padX * 2.0f;
                        wrapped.offsetBase = static_cast<std::uint32_t>(rows[r][c].offset);
                        const auto& layout = CachedLayout(rows[r][c].text, wrapped);
                        table.cells[r][c] = &layout;
                        table.cellOffsets[r][c] = m_buffer.LineStart(table.rowLines[r]) + rows[r][c].offset + rows[r][c].text.size();
                        table.cellTexts[r][c] = rows[r][c].text;
                        rowHeight = std::max(rowHeight, layout.height);
                    }
                    table.rowHeights[r] = rowHeight + padY * 2.0f;
                }

                Visual visual;
                visual.kind = VisualKind::Table;
                visual.block = b;
                visual.line = block.firstLine;
                visual.lineStart = m_buffer.LineStart(block.firstLine);
                visual.y = y + gap;
                visual.extra = m_tables.size();
                float tableWidth = 0.0f;
                for (const float width : table.columnWidths)
                {
                    tableWidth += width;
                }
                visual.width = tableWidth;
                float rowY = visual.y;
                for (std::size_t r = 0; r < rows.size(); ++r)
                {
                    float cellX = 0.0f;
                    for (std::size_t c = 0; c < columns; ++c)
                    {
                        const std::size_t offset = c < rows[r].size() ? table.cellOffsets[r][c] : m_buffer.LineEnd(table.rowLines[r]);
                        m_hits.push_back({ImVec2(cellX, rowY), ImVec2(cellX + table.columnWidths[c], rowY + table.rowHeights[r]),
                                          offset, ActionCaret, b});
                        cellX += table.columnWidths[c];
                    }
                    rowY += table.rowHeights[r];
                }
                visual.height = rowY - visual.y;
                CaretStop stop;
                stop.y = visual.y;
                stop.height = visual.height;
                stop.widget = true;
                stop.firstOffset = m_buffer.LineStart(block.firstLine);
                stop.lastOffset = m_buffer.LineStart(block.lastLine);
                m_stops.push_back(stop);
                m_tables.push_back(std::move(table));
                m_visuals.push_back(visual);
                y = rowY + gap * 2.0f;
                break;
            }

            case BlockKind::Callout:
            {
                if (revealed)
                {
                    for (std::size_t line = block.firstLine; line <= block.lastLine; ++line)
                    {
                        InlineOptions options = inlineBase;
                        options.reveal = true;
                        AddLineRows(line, 0, 0.0f, contentWidth, options, &y);
                    }
                    break;
                }

                const bool folded = (block.calloutFold == '-' && m_unfoldedCallouts.count(block.firstLine) == 0) ||
                                    m_foldedCallouts.count(block.firstLine) != 0;
                const float pad = fontSize * 0.7f;
                const float inset = pad + fontSize * 0.5f;
                Visual box;
                box.kind = VisualKind::Callout;
                box.block = b;
                box.line = block.firstLine;
                box.lineStart = m_buffer.LineStart(block.firstLine);
                box.y = y + gap;
                box.width = contentWidth;
                box.revealed = folded;
                const std::size_t boxIndex = m_visuals.size();
                m_visuals.push_back(box);

                const float headerY = box.y + pad * 0.7f;
                const float headerHeight = fontSize * 1.3f;
                m_hits.push_back({ImVec2(0.0f, headerY - pad * 0.5f), ImVec2(contentWidth, headerY + headerHeight),
                                  m_buffer.LineEnd(block.firstLine), block.calloutFold ? ActionToggleFold : ActionCaret, b});
                CaretStop header;
                header.y = headerY;
                header.height = headerHeight;
                header.widget = true;
                header.firstOffset = m_buffer.LineEnd(block.firstLine);
                header.lastOffset = header.firstOffset;
                m_stops.push_back(header);
                y = headerY + headerHeight + gap;

                if (!folded)
                {
                    InlineOptions options = inlineBase;
                    options.reveal = false;
                    for (std::size_t line = block.firstLine + 1; line <= block.lastLine; ++line)
                    {
                        AddLineRows(line, QuoteContentOffset(m_lines[line]), inset, contentWidth - inset - pad, options, &y);
                    }
                }
                y += pad * 0.6f;
                m_visuals[boxIndex].height = y - m_visuals[boxIndex].y;
                y += gap * 2.0f;
                break;
            }

            case BlockKind::Frontmatter:
            case BlockKind::Math:
            {
                if (revealed)
                {
                    for (std::size_t line = block.firstLine; line <= block.lastLine; ++line)
                    {
                        InlineOptions options = inlineBase;
                        options.code = true;
                        options.language = block.kind == BlockKind::Frontmatter ? "yaml" : "";
                        const bool fence = line == block.firstLine || line == block.lastLine;
                        AddLineRows(line, 0, 0.0f, contentWidth, options, &y, fence ? 0.5f : 1.0f);
                    }
                    break;
                }

                const std::size_t first = block.firstLine + 1;
                const std::size_t last = block.unclosed ? block.lastLine : (block.lastLine > 0 ? block.lastLine - 1 : 0);
                std::size_t rows = 0;
                for (std::size_t line = first; line <= last && line < m_lines.size() && first <= last; ++line)
                {
                    const std::string_view text = TrimView(m_lines[line]);
                    if (block.kind == BlockKind::Math || (!text.empty() && text.front() != '-'))
                    {
                        ++rows;
                        const float rowY = y + gap + fontSize * 0.6f + static_cast<float>(rows - 1) * m_lineHeight;
                        m_hits.push_back({ImVec2(0.0f, rowY), ImVec2(contentWidth, rowY + m_lineHeight), m_buffer.LineEnd(line),
                                          ActionCaret, b});
                    }
                }
                rows = std::max<std::size_t>(rows, 1);
                Visual panel;
                panel.kind = block.kind == BlockKind::Frontmatter ? VisualKind::Properties : VisualKind::Math;
                panel.block = b;
                panel.line = block.firstLine;
                panel.lineStart = m_buffer.LineStart(block.firstLine);
                panel.y = y + gap;
                panel.width = contentWidth;
                panel.height = static_cast<float>(rows) * m_lineHeight + fontSize * 1.2f;
                m_hits.push_back({ImVec2(0.0f, panel.y), ImVec2(contentWidth, panel.y + panel.height),
                                  m_buffer.LineEnd(block.firstLine), ActionCaret, b});
                CaretStop stop;
                stop.y = panel.y;
                stop.height = panel.height;
                stop.widget = true;
                stop.firstOffset = m_buffer.LineEnd(block.firstLine);
                stop.lastOffset = m_buffer.LineStart(block.lastLine);
                m_stops.push_back(stop);
                m_visuals.push_back(panel);
                y = panel.y + panel.height + gap * 2.0f;
                break;
            }
            }
        }

        m_contentHeight = y + std::max(m_viewHeight * 0.45f, fontSize * 4.0f);
        m_visualRevision = m_buffer.Revision();
    }

    // ------------------------------------------------------------------ drawing

    void LiveEditor::DrawVisuals(ImDrawList* drawList, ImVec2 origin, float scrollY, float viewHeight, double,
                                 const LiveEditorSettings& settings)
    {
        const float fontSize = settings.fontSize;
        const float top = scrollY - fontSize * 2.0f;
        const float bottom = scrollY + viewHeight + fontSize * 2.0f;
        const auto visible = [&](const Visual& visual) { return visual.y + visual.height >= top && visual.y <= bottom; };
        m_firstVisibleLine = 0;
        bool firstFound = false;

        // Caret line highlight sits under everything
        if (m_focused && settings.highlightLine && m_selection.Empty() && !settings.readOnly)
        {
            const std::size_t caretLine = CaretLine();
            for (const auto& visual : m_visuals)
            {
                if (visual.kind == VisualKind::Rows && visual.line == caretLine && visible(visual))
                {
                    const ImVec4 fill = Mix(Background, Panel, 0.75f);
                    drawList->AddRectFilled(ImVec2(origin.x - fontSize * 0.5f, origin.y + visual.y),
                                            ImVec2(origin.x + m_contentWidth + fontSize * 0.5f, origin.y + visual.y + visual.height),
                                            U32(fill), 4.0f);
                }
            }
        }

        // Panels first, so rows inside them draw on top
        for (const auto& visual : m_visuals)
        {
            if (!visible(visual))
            {
                continue;
            }
            const ImVec2 min(origin.x + visual.x, origin.y + visual.y);
            const ImVec2 max(min.x + visual.width, min.y + visual.height);
            switch (visual.kind)
            {
            case VisualKind::CodePanel:
            {
                const ImVec4 fill = Mix(Background, Panel, 1.0f);
                drawList->AddRectFilled(min, max, U32(fill), 6.0f);
                drawList->AddRect(min, max, U32(Muted, 0.18f), 6.0f);
                if (!visual.revealed)
                {
                    const float small = fontSize * 0.82f;
                    const std::string label = visual.target.empty() ? std::string("text") : visual.target;
                    const bool hovered = ImGui::IsMouseHoveringRect(min, max);
                    const float copyWidth = TextWidth(small, "copy") + fontSize;
                    const float labelX = max.x - copyWidth - TextWidth(small, label) - fontSize * 0.9f;
                    drawList->AddText(ImGui::GetFont(), small, ImVec2(labelX, min.y + fontSize * 0.4f), U32(Muted, 0.75f),
                                      label.c_str());
                    if (hovered)
                    {
                        const ImVec2 buttonMin(max.x - copyWidth - fontSize * 0.3f, min.y + fontSize * 0.25f);
                        const ImVec2 buttonMax(max.x - fontSize * 0.3f, buttonMin.y + fontSize * 1.3f);
                        const bool overButton = ImGui::IsMouseHoveringRect(buttonMin, buttonMax);
                        drawList->AddRectFilled(buttonMin, buttonMax, U32(overButton ? Mix(Panel, Cyan, 0.25f) : Mix(Panel, Muted, 0.18f)),
                                                4.0f);
                        drawList->AddText(ImGui::GetFont(), small, ImVec2(buttonMin.x + fontSize * 0.5f, buttonMin.y + fontSize * 0.15f),
                                          U32(overButton ? Cyan : Primary), "copy");
                    }
                }
                break;
            }
            case VisualKind::Callout:
            {
                const Block& block = m_blocks[visual.block];
                const CalloutStyle look = CalloutLook(block.calloutType);
                ImVec4 tint = look.color;
                tint.w = 0.07f;
                drawList->AddRectFilled(min, max, U32(tint), 6.0f);
                drawList->AddRectFilled(min, ImVec2(min.x + 3.0f, max.y), U32(look.color, 0.85f), 2.0f);
                const float pad = fontSize * 0.7f;
                const float headerY = min.y + pad * 0.7f;
                const ImVec2 badgeMin(min.x + pad, headerY + fontSize * 0.08f);
                const ImVec2 badgeMax(badgeMin.x + fontSize * 1.05f, badgeMin.y + fontSize * 1.05f);
                drawList->AddRectFilled(badgeMin, badgeMax, U32(look.color, 0.22f), 4.0f);
                const float iconWidth = TextWidth(fontSize * 0.85f, look.icon);
                drawList->AddText(ImGui::GetFont(), fontSize * 0.85f,
                                  ImVec2(badgeMin.x + (badgeMax.x - badgeMin.x - iconWidth) * 0.5f, badgeMin.y + fontSize * 0.07f),
                                  U32(look.color), look.icon);
                const std::string title = block.calloutTitle.empty() ? CalloutDefaultTitle(block.calloutType) : block.calloutTitle;
                DrawStyledText(drawList, fontSize, ImVec2(badgeMax.x + fontSize * 0.5f, headerY), U32(look.color),
                               title.c_str(), title.c_str() + title.size(), true, false);
                if (block.calloutFold)
                {
                    // Chevron: right when folded, down when open
                    const float cx = max.x - pad - fontSize * 0.3f;
                    const float cy = headerY + fontSize * 0.6f;
                    const float s = fontSize * 0.28f;
                    if (visual.revealed)
                    {
                        drawList->AddTriangleFilled(ImVec2(cx - s * 0.6f, cy - s), ImVec2(cx - s * 0.6f, cy + s),
                                                    ImVec2(cx + s * 0.7f, cy), U32(look.color, 0.8f));
                    }
                    else
                    {
                        drawList->AddTriangleFilled(ImVec2(cx - s, cy - s * 0.6f), ImVec2(cx + s, cy - s * 0.6f),
                                                    ImVec2(cx, cy + s * 0.7f), U32(look.color, 0.8f));
                    }
                }
                break;
            }
            case VisualKind::Table:
            {
                const TableGeometry& table = m_tables[visual.extra];
                drawList->AddRectFilled(min, max, U32(Mix(Background, Panel, 0.45f)), 6.0f);
                if (!table.rowHeights.empty())
                {
                    drawList->AddRectFilled(min, ImVec2(max.x, min.y + table.rowHeights[0]), U32(Panel), 6.0f,
                                            ImDrawFlags_RoundCornersTop);
                }
                const float padX = fontSize * 0.65f;
                const float padY = fontSize * 0.35f;
                float rowY = min.y;
                for (std::size_t r = 0; r < table.cells.size(); ++r)
                {
                    if (r > 0)
                    {
                        drawList->AddLine(ImVec2(min.x, rowY), ImVec2(max.x, rowY), U32(Muted, r == 1 ? 0.45f : 0.16f), 1.0f);
                    }
                    float cellX = min.x;
                    for (std::size_t c = 0; c < table.cells[r].size(); ++c)
                    {
                        if (c > 0 && r == 0)
                        {
                            drawList->AddLine(ImVec2(cellX, min.y), ImVec2(cellX, max.y), U32(Muted, 0.16f), 1.0f);
                        }
                        const InlineLayout* layout = table.cells[r][c];
                        if (layout)
                        {
                            float alignOffset = 0.0f;
                            const TableAlign align = c < table.aligns.size() ? table.aligns[c] : TableAlign::Left;
                            const float free = table.columnWidths[c] - padX * 2.0f - layout->width;
                            if (align == TableAlign::Center)
                            {
                                alignOffset = std::max(0.0f, free * 0.5f);
                            }
                            else if (align == TableAlign::Right)
                            {
                                alignOffset = std::max(0.0f, free);
                            }
                            InlineOptions options;
                            options.fontSize = fontSize;
                            options.headingScale = false;
                            options.wrapWidth = table.columnWidths[c] - padX * 2.0f;
                            DrawInline(drawList, *layout, table.cellTexts[r][c], ImVec2(cellX + padX + alignOffset, rowY + padY),
                                       options, r == 0 ? 1.0f : 0.95f);
                        }
                        cellX += table.columnWidths[c];
                    }
                    rowY += table.rowHeights[r];
                }
                drawList->AddRect(min, max, U32(Muted, 0.3f), 6.0f);
                break;
            }
            case VisualKind::Properties:
            {
                const Block& block = m_blocks[visual.block];
                drawList->AddRectFilled(min, max, U32(Mix(Background, Panel, 0.6f)), 6.0f);
                drawList->AddRect(min, max, U32(Muted, 0.16f), 6.0f);
                const float small = fontSize * 0.86f;
                float rowY = min.y + fontSize * 0.6f;
                float keyWidth = fontSize * 5.0f;
                for (std::size_t line = block.firstLine + 1; line < block.lastLine; ++line)
                {
                    const std::string_view text = m_lines[line];
                    const std::size_t colon = text.find(':');
                    if (!text.empty() && text.front() != ' ' && text.front() != '-' && colon != std::string_view::npos)
                    {
                        keyWidth = std::max(keyWidth, TextWidth(small, text.substr(0, colon)) + fontSize * 1.5f);
                    }
                }
                keyWidth = std::min(keyWidth, visual.width * 0.4f);
                for (std::size_t line = block.firstLine + 1; line < block.lastLine; ++line)
                {
                    const std::string_view text = m_lines[line];
                    const std::size_t colon = text.find(':');
                    if (text.empty() || text.front() == ' ' || text.front() == '-' || colon == std::string_view::npos)
                    {
                        continue;
                    }
                    const std::string key(text.substr(0, colon));
                    drawList->AddText(ImGui::GetFont(), small, ImVec2(min.x + fontSize, rowY + (fontSize - small) * 0.5f),
                                      U32(Muted), key.c_str());

                    // Values: inline lists and following "- item" lines render as pills
                    std::vector<std::string> values;
                    std::string_view value = TrimView(text.substr(colon + 1));
                    if (!value.empty() && value.front() == '[' && value.back() == ']')
                    {
                        value = value.substr(1, value.size() - 2);
                        std::size_t start = 0;
                        while (start <= value.size())
                        {
                            const std::size_t comma = value.find(',', start);
                            const std::string_view item = TrimView(value.substr(start, comma - start));
                            if (!item.empty())
                            {
                                values.emplace_back(item);
                            }
                            if (comma == std::string_view::npos)
                            {
                                break;
                            }
                            start = comma + 1;
                        }
                    }
                    else if (value.empty())
                    {
                        for (std::size_t next = line + 1; next < block.lastLine; ++next)
                        {
                            const std::string_view item = TrimView(m_lines[next]);
                            if (item.size() < 2 || item[0] != '-')
                            {
                                break;
                            }
                            values.emplace_back(TrimView(item.substr(1)));
                        }
                    }
                    float x = min.x + fontSize + keyWidth;
                    if (values.empty())
                    {
                        const std::string plain(value);
                        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(x, rowY), U32(Primary), plain.c_str());
                    }
                    for (const auto& item : values)
                    {
                        const float width = TextWidth(small, item) + fontSize * 0.9f;
                        const ImVec2 pillMin(x, rowY + fontSize * 0.02f);
                        const ImVec2 pillMax(x + width, pillMin.y + fontSize * 1.12f);
                        drawList->AddRectFilled(pillMin, pillMax, U32(Cyan, 0.14f), fontSize * 0.56f);
                        drawList->AddText(ImGui::GetFont(), small, ImVec2(x + fontSize * 0.45f, pillMin.y + (fontSize * 1.12f - small) * 0.5f),
                                          U32(Cyan), item.c_str());
                        x += width + fontSize * 0.4f;
                    }
                    rowY += m_lineHeight;
                }
                break;
            }
            case VisualKind::Math:
            {
                const Block& block = m_blocks[visual.block];
                drawList->AddRectFilled(min, max, U32(Mix(Background, Panel, 0.5f)), 6.0f);
                float rowY = min.y + fontSize * 0.6f;
                const std::size_t last = block.unclosed ? block.lastLine : block.lastLine - 1;
                for (std::size_t line = block.firstLine + 1; line <= last && line < m_lines.size(); ++line)
                {
                    const std::string text(TrimView(m_lines[line]));
                    const float width = TextWidth(fontSize, text);
                    DrawStyledText(drawList, fontSize, ImVec2(min.x + (visual.width - width) * 0.5f, rowY), U32(MarkdownItalic),
                                   text.c_str(), text.c_str() + text.size(), false, true);
                    rowY += m_lineHeight;
                }
                break;
            }
            case VisualKind::Image:
            {
                if (visual.texture)
                {
                    drawList->AddImageRounded(ImTextureRef(visual.texture), min, max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 6.0f);
                }
                else
                {
                    drawList->AddRectFilled(min, max, U32(Panel), 6.0f);
                    drawList->AddRect(min, max, U32(MarkdownImage, 0.5f), 6.0f);
                    const std::string label = "image not found: " + visual.target;
                    drawList->PushClipRect(min, max, true);
                    drawList->AddText(ImGui::GetFont(), fontSize * 0.9f, ImVec2(min.x + fontSize, min.y + fontSize),
                                      U32(MarkdownImage), label.c_str());
                    drawList->PopClipRect();
                }
                break;
            }
            case VisualKind::Rows:
                break;
            }
        }

        for (const auto& visual : m_visuals)
        {
            if (visual.kind != VisualKind::Rows || !visible(visual))
            {
                continue;
            }
            if (!firstFound && visual.y + visual.height >= scrollY)
            {
                m_firstVisibleLine = visual.line;
                firstFound = true;
            }
            const std::string_view text = m_lines[visual.line].substr(std::min<std::size_t>(visual.options.offsetBase, m_lines[visual.line].size()));
            DrawInline(drawList, *visual.layout, text, ImVec2(origin.x + visual.x, origin.y + visual.y), visual.options, visual.alpha);
        }
    }

    bool LiveEditor::LocateCaret(std::size_t offset, std::size_t* visualIndex, std::size_t* rowIndex) const
    {
        const std::size_t line = m_buffer.LineOfOffset(offset);
        const std::size_t relative = offset - m_buffer.LineStart(line);
        for (std::size_t v = 0; v < m_visuals.size(); ++v)
        {
            const Visual& visual = m_visuals[v];
            if (visual.kind != VisualKind::Rows || visual.line != line || !visual.layout)
            {
                continue;
            }
            if (relative < visual.layout->sourceStart && visual.layout->sourceStart > 0)
            {
                continue;
            }
            *visualIndex = v;
            *rowIndex = RowOfOffset(*visual.layout, static_cast<std::uint32_t>(relative));
            return true;
        }
        return false;
    }

    void LiveEditor::DrawSelectionAndCaret(ImDrawList* drawList, ImVec2 origin, double now, const LiveEditorSettings& settings)
    {
        const float fontSize = settings.fontSize;
        if (!m_selection.Empty())
        {
            const std::size_t selMin = m_selection.Min();
            const std::size_t selMax = m_selection.Max();
            ImVec4 fill = Cyan;
            fill.w = m_focused ? 0.28f : 0.16f;
            for (const auto& visual : m_visuals)
            {
                if (visual.kind != VisualKind::Rows || !visual.layout)
                {
                    continue;
                }
                const std::size_t lineEnd = visual.lineStart + m_lines[visual.line].size();
                if (selMax < visual.lineStart || selMin > lineEnd)
                {
                    continue;
                }
                const InlineLayout& layout = *visual.layout;
                for (std::size_t r = 0; r < layout.rows.size(); ++r)
                {
                    const Row& row = layout.rows[r];
                    const std::size_t rowStart = visual.lineStart + row.startOffset;
                    const std::size_t rowEnd = visual.lineStart + row.endOffset;
                    const std::size_t a = std::max(selMin, rowStart);
                    const std::size_t b = std::min(selMax, rowEnd);
                    const bool coversNewline = r + 1 == layout.rows.size() && selMax > lineEnd && selMin <= lineEnd;
                    if (a > b || (a == b && !coversNewline))
                    {
                        continue;
                    }
                    float x0 = OffsetX(layout, row, static_cast<std::uint32_t>(a - visual.lineStart));
                    float x1 = OffsetX(layout, row, static_cast<std::uint32_t>(b - visual.lineStart));
                    if (coversNewline)
                    {
                        x1 += fontSize * 0.45f;
                    }
                    drawList->AddRectFilled(ImVec2(origin.x + visual.x + x0, origin.y + visual.y + row.y),
                                            ImVec2(origin.x + visual.x + x1, origin.y + visual.y + row.y + row.textHeight + 2.0f),
                                            U32(fill), 2.0f);
                }
            }
        }

        std::size_t visualIndex = 0;
        std::size_t rowIndex = 0;
        if (!LocateCaret(m_selection.caret, &visualIndex, &rowIndex))
        {
            return;
        }
        const Visual& visual = m_visuals[visualIndex];
        const Row& row = visual.layout->rows[rowIndex];
        const float x = OffsetX(*visual.layout, row, static_cast<std::uint32_t>(m_selection.caret - visual.lineStart));
        const ImVec2 top(origin.x + visual.x + x, origin.y + visual.y + row.y);
        m_caretScreen = ImVec2(top.x, top.y + row.textHeight + 4.0f);
        m_caretHeight = row.textHeight;

        if (!m_focused || settings.readOnly)
        {
            return;
        }
        const double sinceInput = now - m_lastInputTime;
        const bool on = sinceInput < 0.6 || std::fmod(sinceInput, 1.1) < 0.65;
        if (on)
        {
            drawList->AddRectFilled(ImVec2(top.x - 1.0f, top.y + 1.0f), ImVec2(top.x + 1.0f, top.y + row.textHeight + 1.0f),
                                    U32(Amber));
        }

        // Tell the platform where to put the IME composition window
        ImGuiContext& g = *GImGui;
        g.PlatformImeData.WantVisible = true;
        g.PlatformImeData.WantTextInput = true;
        g.PlatformImeData.InputPos = ImVec2(top.x, top.y);
        g.PlatformImeData.InputLineHeight = row.textHeight;
        g.PlatformImeData.ViewportId = ImGui::GetWindowViewport()->ID;
    }

    // ------------------------------------------------------------------ input

    bool LiveEditor::OffsetAtScreen(ImVec2 origin, ImVec2 point, std::size_t* offset) const
    {
        const ImVec2 local(point.x - origin.x, point.y - origin.y);
        const Visual* best = nullptr;
        float bestDistance = FLT_MAX;
        for (const auto& visual : m_visuals)
        {
            if (visual.kind != VisualKind::Rows || !visual.layout)
            {
                continue;
            }
            float distance = 0.0f;
            if (local.y < visual.y)
            {
                distance = visual.y - local.y;
            }
            else if (local.y > visual.y + visual.height)
            {
                distance = local.y - visual.y - visual.height;
            }
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &visual;
            }
        }
        if (!best)
        {
            *offset = local.y < 0.0f ? 0 : m_buffer.Size();
            return true;
        }
        const ImVec2 inside(local.x - best->x, std::clamp(local.y - best->y, 0.0f, std::max(0.0f, best->height - 1.0f)));
        *offset = std::min(best->lineStart + OffsetAtPoint(*best->layout, inside), m_buffer.Size());
        return true;
    }

    void LiveEditor::HandleMouse(ImVec2 origin, const ImVec2& viewMin, const ImVec2& viewMax, const LiveEditorSettings& settings,
                                 const LiveEditorHooks& hooks, double now)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 mouse = io.MousePos;
        const bool inside = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && mouse.x >= viewMin.x && mouse.y >= viewMin.y &&
                            mouse.x < viewMax.x && mouse.y < viewMax.y;
        const ImVec2 local(mouse.x - origin.x, mouse.y - origin.y);

        // Cursor feedback: hand over clickable rendered things, text beam elsewhere
        if (inside && !m_dragging)
        {
            bool hand = false;
            for (const auto& hit : m_hits)
            {
                if (hit.action != ActionCaret && local.x >= hit.min.x && local.y >= hit.min.y && local.x < hit.max.x && local.y < hit.max.y)
                {
                    hand = true;
                }
            }
            for (const auto& visual : m_visuals)
            {
                if (hand || visual.kind != VisualKind::Rows || local.y < visual.y || local.y >= visual.y + visual.height)
                {
                    continue;
                }
                const Atom* atom = AtomAtPoint(*visual.layout, ImVec2(local.x - visual.x, local.y - visual.y));
                if (atom && (!visual.revealed || io.KeyCtrl) &&
                    (atom->kind == AtomKind::Checkbox || atom->style == S::Link || atom->style == S::WikiLink ||
                     atom->style == S::Url || atom->style == S::Tag))
                {
                    hand = true;
                }
            }
            ImGui::SetMouseCursor(hand ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_TextInput);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (!inside)
            {
                // Clicks elsewhere in the same window take focus away; popups like the slash palette do not
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
                {
                    m_focused = false;
                }
                return;
            }

            const int clicks = ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);
            m_focused = true;
            m_lastInputTime = now;

            if (clicks == 1)
            {
                for (const auto& hit : m_hits)
                {
                    if (local.x < hit.min.x || local.y < hit.min.y || local.x >= hit.max.x || local.y >= hit.max.y)
                    {
                        continue;
                    }
                    const Block& block = m_blocks[std::min(hit.block, m_blocks.size() - 1)];
                    if (hit.action == ActionCopyCode)
                    {
                        std::string code;
                        const std::size_t last = block.unclosed ? block.lastLine : block.lastLine - 1;
                        for (std::size_t line = block.firstLine + 1; line <= last && line < m_lines.size(); ++line)
                        {
                            code.append(m_lines[line]);
                            code.push_back('\n');
                        }
                        ImGui::SetClipboardText(code.c_str());
                        return;
                    }
                    if (hit.action == ActionToggleFold)
                    {
                        const bool defaultFolded = block.calloutFold == '-';
                        const bool folded = (defaultFolded && m_unfoldedCallouts.count(block.firstLine) == 0) ||
                                            m_foldedCallouts.count(block.firstLine) != 0;
                        if (folded)
                        {
                            m_foldedCallouts.erase(block.firstLine);
                            if (defaultFolded)
                            {
                                m_unfoldedCallouts.insert(block.firstLine);
                            }
                        }
                        else
                        {
                            m_unfoldedCallouts.erase(block.firstLine);
                            if (!defaultFolded)
                            {
                                m_foldedCallouts.insert(block.firstLine);
                            }
                        }
                        return;
                    }
                    if (!settings.readOnly)
                    {
                        SetCaret(hit.offset, io.KeyShift);
                    }
                    return;
                }

                // Rendered checkboxes, links, and tags act on a plain click; revealed links need ctrl
                for (const auto& visual : m_visuals)
                {
                    if (visual.kind != VisualKind::Rows || local.y < visual.y || local.y >= visual.y + visual.height)
                    {
                        continue;
                    }
                    const Atom* atom = AtomAtPoint(*visual.layout, ImVec2(local.x - visual.x, local.y - visual.y));
                    if (!atom)
                    {
                        break;
                    }
                    const std::string_view lineText = m_lines[visual.line];
                    if (atom->kind == AtomKind::Checkbox && !settings.readOnly)
                    {
                        ToggleTaskAt(visual.lineStart + atom->start + 1, now);
                        return;
                    }
                    const bool linkStyle = atom->style == S::Link || atom->style == S::WikiLink || atom->style == S::Url;
                    if (linkStyle && (!visual.revealed || io.KeyCtrl || settings.readOnly))
                    {
                        std::string target;
                        bool wiki = false;
                        if (LinkTargetAt(lineText, atom->start, &target, &wiki) && hooks.openLink)
                        {
                            hooks.openLink(target, wiki);
                            return;
                        }
                    }
                    if (atom->style == S::Tag && (!visual.revealed || io.KeyCtrl) && hooks.openTag)
                    {
                        std::size_t start = atom->start;
                        while (start > 0 && lineText[start] != '#')
                        {
                            --start;
                        }
                        std::size_t end = start + 1;
                        while (end < lineText.size() && lineText[end] != ' ' && lineText[end] != '\t' && lineText[end] != ',')
                        {
                            ++end;
                        }
                        hooks.openTag(std::string(lineText.substr(start, end - start)));
                        return;
                    }
                    break;
                }
            }

            std::size_t offset = 0;
            OffsetAtScreen(origin, mouse, &offset);
            if (clicks == 2)
            {
                std::size_t start = offset;
                std::size_t end = offset;
                const std::string& text = m_buffer.Text();
                const auto word = [](char ch) {
                    const auto byte = static_cast<unsigned char>(ch);
                    return std::isalnum(byte) != 0 || ch == '_' || ch == '-' || byte >= 0x80;
                };
                while (start > 0 && word(text[start - 1]))
                {
                    --start;
                }
                while (end < text.size() && word(text[end]))
                {
                    ++end;
                }
                m_selection = {start, end};
                m_dragOrigin = start;
                m_dragOriginEnd = end;
                m_clickCount = 2;
            }
            else if (clicks >= 3)
            {
                const std::size_t line = m_buffer.LineOfOffset(offset);
                const std::size_t end = line + 1 < m_buffer.LineCount() ? m_buffer.LineStart(line + 1) : m_buffer.Size();
                m_selection = {m_buffer.LineStart(line), end};
                m_dragOrigin = m_selection.anchor;
                m_dragOriginEnd = end;
                m_clickCount = 3;
            }
            else
            {
                SetCaret(offset, io.KeyShift);
                m_dragOrigin = m_selection.anchor;
                m_dragOriginEnd = m_selection.anchor;
                m_clickCount = 1;
            }
            m_buffer.BreakUndoGroup();
            m_dragging = true;
            m_desiredX = -1.0f;
            return;
        }

        if (m_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Dragging past the top or bottom edge scrolls
            if (mouse.y < viewMin.y + 8.0f)
            {
                ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - (viewMin.y + 8.0f - mouse.y) * 0.5f));
            }
            else if (mouse.y > viewMax.y - 8.0f)
            {
                ImGui::SetScrollY(ImGui::GetScrollY() + (mouse.y - viewMax.y + 8.0f) * 0.5f);
            }
            std::size_t offset = 0;
            OffsetAtScreen(origin, mouse, &offset);
            if (m_clickCount >= 2)
            {
                // Word and line drags extend by whole units from the original pick
                if (offset < m_dragOrigin)
                {
                    m_selection = {m_dragOriginEnd, offset};
                }
                else
                {
                    m_selection = {m_dragOrigin, std::max(offset, m_dragOriginEnd)};
                }
            }
            else
            {
                m_selection.caret = offset;
            }
            return;
        }
        m_dragging = false;
    }

    void LiveEditor::MoveVertical(int delta, bool extend)
    {
        std::size_t visualIndex = 0;
        std::size_t rowIndex = 0;
        if (m_stops.empty() || !LocateCaret(m_selection.caret, &visualIndex, &rowIndex))
        {
            const std::size_t line = m_buffer.LineOfOffset(m_selection.caret);
            const long long target = std::clamp<long long>(static_cast<long long>(line) + delta, 0,
                                                           static_cast<long long>(m_buffer.LineCount()) - 1);
            SetCaret(m_buffer.LineStart(static_cast<std::size_t>(target)), extend);
            return;
        }

        const Visual& visual = m_visuals[visualIndex];
        if (m_desiredX < 0.0f)
        {
            m_desiredX = visual.x + OffsetX(*visual.layout, visual.layout->rows[rowIndex],
                                            static_cast<std::uint32_t>(m_selection.caret - visual.lineStart));
        }

        std::size_t current = 0;
        for (std::size_t s = 0; s < m_stops.size(); ++s)
        {
            if (!m_stops[s].widget && m_stops[s].visual == visualIndex && m_stops[s].row == rowIndex)
            {
                current = s;
                break;
            }
        }
        const long long targetIndex = static_cast<long long>(current) + delta;
        if (targetIndex < 0)
        {
            const float keep = m_desiredX;
            SetCaret(0, extend);
            m_desiredX = keep;
            return;
        }
        if (targetIndex >= static_cast<long long>(m_stops.size()))
        {
            const float keep = m_desiredX;
            SetCaret(m_buffer.Size(), extend);
            m_desiredX = keep;
            return;
        }

        const CaretStop& stop = m_stops[static_cast<std::size_t>(targetIndex)];
        const float keep = m_desiredX;
        if (stop.widget)
        {
            SetCaret(delta > 0 ? stop.firstOffset : stop.lastOffset, extend);
        }
        else
        {
            const Visual& target = m_visuals[stop.visual];
            const Row& row = target.layout->rows[stop.row];
            const std::uint32_t local = OffsetAtPoint(*target.layout, ImVec2(m_desiredX - target.x, row.y + 1.0f));
            SetCaret(target.lineStart + local, extend);
        }
        m_desiredX = keep;
    }

    void LiveEditor::HandleKeyboard(const LiveEditorSettings& settings, double now)
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool ctrl = io.KeyCtrl;
        const bool shift = io.KeyShift;
        const bool alt = io.KeyAlt;
        const bool nav = !settings.captureNavigation;
        const bool editable = !settings.readOnly;
        const auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, true); };

        // Copy and select-all also work in reading view
        if (ctrl && !alt && pressed(ImGuiKey_A))
        {
            m_selection = {0, m_buffer.Size()};
            return;
        }
        if (ctrl && !alt && (pressed(ImGuiKey_C) || pressed(ImGuiKey_X)))
        {
            const bool cut = ImGui::IsKeyPressed(ImGuiKey_X, true) && editable;
            std::string text;
            if (m_selection.Empty())
            {
                // Copying with no selection takes the whole line, like most code editors
                const std::size_t line = CaretLine();
                text = std::string(m_buffer.Line(line)) + "\n";
                if (cut)
                {
                    DeleteCurrentLine();
                }
            }
            else
            {
                SelectedText(&text);
                if (cut)
                {
                    ReplaceSelection("", EditKind::Other, now);
                }
            }
            ImGui::SetClipboardText(text.c_str());
            return;
        }
        if (!editable)
        {
            return;
        }

        if (ctrl && !alt && pressed(ImGuiKey_V))
        {
            if (const char* clipboard = ImGui::GetClipboardText(); clipboard && *clipboard)
            {
                InsertText(clipboard);
            }
            return;
        }
        if (ctrl && !alt && ((pressed(ImGuiKey_Z) && shift) || pressed(ImGuiKey_Y)))
        {
            Redo();
            return;
        }
        if (ctrl && !alt && pressed(ImGuiKey_Z))
        {
            Undo();
            return;
        }
        if (ctrl && !alt && !shift && pressed(ImGuiKey_B))
        {
            ToggleWrap("**", now);
            return;
        }
        if (ctrl && !alt && !shift && pressed(ImGuiKey_I))
        {
            ToggleWrap("*", now);
            return;
        }
        if (ctrl && !alt && !shift && pressed(ImGuiKey_E))
        {
            ToggleWrap("`", now);
            return;
        }
        if (ctrl && !alt && !shift && pressed(ImGuiKey_K))
        {
            if (m_selection.Empty())
            {
                const std::size_t at = m_selection.caret;
                Edit(at, at, "[]()", EditKind::Other, now, at + 1);
            }
            else
            {
                std::string label;
                SelectedText(&label);
                const std::size_t start = m_selection.Min();
                Edit(start, m_selection.Max(), "[" + label + "]()", EditKind::Other, now, start + label.size() + 3);
            }
            return;
        }
        if (ctrl && !alt && !shift && pressed(ImGuiKey_L))
        {
            const std::size_t line = m_buffer.LineOfOffset(m_selection.Max());
            const std::size_t end = line + 1 < m_buffer.LineCount() ? m_buffer.LineStart(line + 1) : m_buffer.Size();
            m_selection = {m_buffer.LineStart(m_buffer.LineOfOffset(m_selection.Min())), end};
            return;
        }
        if (ctrl && shift && pressed(ImGuiKey_K))
        {
            DeleteCurrentLine();
            return;
        }
        if (alt && !ctrl && (pressed(ImGuiKey_UpArrow) || pressed(ImGuiKey_DownArrow)))
        {
            MoveLines(ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) ? -1 : 1, now);
            return;
        }

        if (pressed(ImGuiKey_LeftArrow))
        {
            if (!shift && !m_selection.Empty())
            {
                SetCaret(m_selection.Min(), false);
            }
            else
            {
                SetCaret(ctrl ? m_buffer.PrevWord(m_selection.caret) : m_buffer.PrevChar(m_selection.caret), shift);
            }
            m_desiredX = -1.0f;
        }
        if (pressed(ImGuiKey_RightArrow))
        {
            if (!shift && !m_selection.Empty())
            {
                SetCaret(m_selection.Max(), false);
            }
            else
            {
                SetCaret(ctrl ? m_buffer.NextWord(m_selection.caret) : m_buffer.NextChar(m_selection.caret), shift);
            }
            m_desiredX = -1.0f;
        }
        if (nav && !alt && pressed(ImGuiKey_UpArrow))
        {
            MoveVertical(-1, shift);
        }
        if (nav && !alt && pressed(ImGuiKey_DownArrow))
        {
            MoveVertical(1, shift);
        }
        if (pressed(ImGuiKey_PageUp) || pressed(ImGuiKey_PageDown))
        {
            const int rows = std::max(1, static_cast<int>(m_viewHeight / std::max(1.0f, m_lineHeight)) - 2);
            MoveVertical(ImGui::IsKeyPressed(ImGuiKey_PageUp, true) ? -rows : rows, shift);
        }
        if (pressed(ImGuiKey_Home))
        {
            if (ctrl)
            {
                SetCaret(0, shift);
            }
            else
            {
                const std::size_t line = CaretLine();
                const std::size_t home = LineHomeOffset(line);
                SetCaret(m_selection.caret == home ? m_buffer.LineStart(line) : home, shift);
            }
            m_desiredX = -1.0f;
        }
        if (pressed(ImGuiKey_End))
        {
            SetCaret(ctrl ? m_buffer.Size() : m_buffer.LineEnd(CaretLine()), shift);
            m_desiredX = -1.0f;
        }

        if (pressed(ImGuiKey_Backspace))
        {
            Backspace(ctrl, now);
        }
        if (pressed(ImGuiKey_Delete))
        {
            DeleteForward(ctrl, now);
        }
        if (nav && !ctrl && (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)))
        {
            InsertNewline(!shift, now);
        }
        if (nav && !ctrl && pressed(ImGuiKey_Tab))
        {
            const std::size_t line = CaretLine();
            const LinePrefix prefix = ParsePrefix(m_buffer.Line(line));
            const bool multiLine = m_buffer.LineOfOffset(m_selection.Min()) != m_buffer.LineOfOffset(m_selection.Max());
            if (shift || multiLine || !prefix.marker.empty() || !prefix.quotes.empty())
            {
                IndentLines(shift, now);
            }
            else
            {
                ReplaceSelection(settings.indentWithTabs ? "\t" : std::string(static_cast<std::size_t>(settings.tabWidth), ' '),
                                 EditKind::Typing, now);
            }
        }

        // Typed characters; wrapping a selection with markdown punctuation instead of replacing it
        if (!io.InputQueueCharacters.empty() && !(ctrl && !alt))
        {
            std::string typed;
            for (const ImWchar ch : io.InputQueueCharacters)
            {
                if (ch < 32 || ch == 127)
                {
                    continue;
                }
                char buffer[5] = {};
                ImTextCharToUtf8(buffer, ch);
                typed += buffer;
            }
            if (!typed.empty())
            {
                static const std::string wrappers = "*_`~=";
                if (!m_selection.Empty() && typed.size() == 1 && wrappers.find(typed[0]) != std::string::npos)
                {
                    ToggleWrap(typed, now);
                }
                else if (!m_selection.Empty() && (typed == "[" || typed == "(" || typed == "\""))
                {
                    std::string inner;
                    SelectedText(&inner);
                    const std::string close = typed == "[" ? "]" : (typed == "(" ? ")" : "\"");
                    const std::size_t start = m_selection.Min();
                    const Selection before = m_selection;
                    const Selection after{start + 1, start + 1 + inner.size()};
                    m_buffer.Replace(start, m_selection.Max(), typed + inner + close, before, after, EditKind::Other, now);
                    m_selection = after;
                }
                else
                {
                    ReplaceSelection(typed, EditKind::Typing, now);
                }
            }
        }
    }

    // ------------------------------------------------------------------ frame

    void LiveEditor::Draw(const char* id, ImVec2 size, const LiveEditorSettings& settings, const LiveEditorHooks& hooks, double now)
    {
        m_now = now;
        m_settings = settings;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild(id, size, ImGuiChildFlags_None, ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

        if (m_resetScroll)
        {
            ImGui::SetScrollY(0.0f);
            m_resetScroll = false;
        }
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float gutter = settings.fontSize * 1.2f;
        float contentWidth = std::max(120.0f, available.x - gutter * 2.0f);
        if (settings.pageWidth > 0.0f)
        {
            contentWidth = std::min(contentWidth, settings.pageWidth);
        }
        m_contentWidth = contentWidth;
        m_columnX = std::max(gutter, (available.x - contentWidth) * 0.5f);
        m_scrollY = ImGui::GetScrollY();
        m_viewHeight = ImGui::GetWindowHeight();
        const ImVec2 viewMin = ImGui::GetWindowPos();
        const ImVec2 viewMax(viewMin.x + ImGui::GetWindowWidth(), viewMin.y + m_viewHeight);
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 origin(cursor.x + m_columnX, cursor.y);

        if (settings.acceptInput && m_visualRevision == m_buffer.Revision() && !m_visuals.empty())
        {
            HandleMouse(origin, viewMin, viewMax, settings, hooks, now);
        }
        // Another text field (a prompt or the todo form) owns the keyboard while it is active
        if (m_focused && settings.acceptInput && !ImGui::IsAnyItemActive())
        {
            HandleKeyboard(settings, now);
        }

        RebuildStructure();
        BuildVisuals(contentWidth, settings, hooks);

        // Keep the caret on screen after keyboard moves and edits
        if (m_scrollToCaret)
        {
            m_scrollToCaret = false;
            std::size_t visualIndex = 0;
            std::size_t rowIndex = 0;
            if (LocateCaret(m_selection.caret, &visualIndex, &rowIndex))
            {
                const Visual& visual = m_visuals[visualIndex];
                const Row& row = visual.layout->rows[rowIndex];
                const float caretTop = visual.y + row.y;
                const float caretBottom = caretTop + row.height;
                const float margin = m_lineHeight * 2.0f;
                if (m_centerCaret)
                {
                    ImGui::SetScrollY(std::max(0.0f, caretTop - m_viewHeight * 0.35f));
                }
                else if (caretTop < m_scrollY + margin)
                {
                    ImGui::SetScrollY(std::max(0.0f, caretTop - margin));
                }
                else if (caretBottom > m_scrollY + m_viewHeight - margin)
                {
                    ImGui::SetScrollY(caretBottom - m_viewHeight + margin);
                }
            }
            m_centerCaret = false;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawVisuals(drawList, origin, m_scrollY, m_viewHeight, now, settings);
        DrawSelectionAndCaret(drawList, origin, now, settings);

        ImGui::Dummy(ImVec2(available.x, m_contentHeight));
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}
