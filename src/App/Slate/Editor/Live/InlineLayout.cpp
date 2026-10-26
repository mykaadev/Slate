#include "App/Slate/Editor/Live/InlineLayout.h"

#include "App/Slate/Editor/LivePreviewLexer.h"
#include "App/Slate/UI/SlateUi.h"

#include <algorithm>
#include <cmath>

namespace Software::Slate::Live
{
    namespace
    {
        namespace S = LivePreviewStyle;

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
        {
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
        }

        ImU32 ToU32(ImVec4 color, float alpha)
        {
            color.w *= alpha;
            return ImGui::ColorConvertFloat4ToU32(color);
        }

        bool IsBold(int style)
        {
            return style == S::Bold || style == S::BoldItalic || style == S::Heading1 || style == S::Heading2 ||
                   style == S::Heading3 || style == S::Callout || style == S::TaskDone || style == S::Task;
        }

        bool IsItalic(int style)
        {
            return style == S::Italic || style == S::BoldItalic || style == S::Quote || style == S::Image ||
                   style == S::Comment || style == CodeStyleBase + static_cast<int>(CodeToken::Comment);
        }

        bool IsLinkStyle(int style)
        {
            return style == S::Link || style == S::WikiLink;
        }

        int Utf8Length(unsigned char lead)
        {
            if (lead < 0x80)
            {
                return 1;
            }
            if ((lead >> 5) == 0x6)
            {
                return 2;
            }
            if ((lead >> 4) == 0xE)
            {
                return 3;
            }
            if ((lead >> 3) == 0x1E)
            {
                return 4;
            }
            return 1;
        }

        float TextWidth(float size, const char* begin, const char* end)
        {
            return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end).x;
        }

        bool IsMarkerStyle(int style)
        {
            return style == S::Bullet || style == S::Task || style == S::TaskDone || style == S::QuoteMarker ||
                   style == S::Markup || style == S::Hidden;
        }
    }

    float HeadingFontSize(int level, float baseSize, bool headingScale)
    {
        if (!headingScale || level <= 0)
        {
            return baseSize;
        }
        static constexpr float Scales[] = {1.0f, 1.7f, 1.42f, 1.22f, 1.1f, 1.0f, 1.0f};
        return std::round(baseSize * Scales[std::clamp(level, 0, 6)]);
    }

    ImU32 StyleColor(int style, float alpha)
    {
        using namespace Software::Slate::UI;
        if (style >= CodeStyleBase)
        {
            switch (static_cast<CodeToken>(style - CodeStyleBase))
            {
            case CodeToken::Keyword: return ToU32(Amber, alpha);
            case CodeToken::Type: return ToU32(Cyan, alpha);
            case CodeToken::String: return ToU32(Green, alpha);
            case CodeToken::Number: return ToU32(MarkdownImage, alpha);
            case CodeToken::Comment: return ToU32(Muted, alpha);
            case CodeToken::Function: return ToU32(MarkdownHeading1, alpha);
            case CodeToken::Constant: return ToU32(MarkdownImage, alpha);
            case CodeToken::Punctuation: return ToU32(Mix(MarkdownCode, Muted, 0.45f), alpha);
            case CodeToken::Meta: return ToU32(MarkdownItalic, alpha);
            case CodeToken::Property: return ToU32(MarkdownHeading2, alpha);
            case CodeToken::Tag: return ToU32(MarkdownHeading3, alpha);
            case CodeToken::Text:
            default: return ToU32(MarkdownCode, alpha);
            }
        }

        switch (style)
        {
        case S::Hidden: return 0;
        case S::Markup: return ToU32(Mix(Muted, Background, 0.2f), alpha);
        case S::Heading1: return ToU32(MarkdownHeading1, alpha);
        case S::Heading2: return ToU32(MarkdownHeading2, alpha);
        case S::Heading3: return ToU32(MarkdownHeading3, alpha);
        case S::Heading4:
        case S::Heading5: return ToU32(Mix(MarkdownHeading3, Primary, 0.35f), alpha);
        case S::Heading6: return ToU32(Mix(MarkdownHeading3, Muted, 0.4f), alpha);
        case S::Bold:
        case S::BoldItalic: return ToU32(MarkdownBold, alpha);
        case S::Italic: return ToU32(MarkdownItalic, alpha);
        case S::Code:
        case S::CodeBlock: return ToU32(MarkdownCode, alpha);
        case S::Fence: return ToU32(Mix(Muted, Background, 0.15f), alpha);
        case S::Link:
        case S::WikiLink: return ToU32(UI::MarkdownLink, alpha);
        case S::Url: return ToU32(Mix(UI::MarkdownLink, Muted, 0.55f), alpha);
        case S::Bullet: return ToU32(MarkdownBullet, alpha);
        case S::Task: return ToU32(MarkdownCheckbox, alpha);
        case S::TaskDone: return ToU32(MarkdownCheckboxDone, alpha);
        case S::DoneText: return ToU32(Muted, alpha);
        case S::Quote: return ToU32(MarkdownQuote, alpha);
        case S::QuoteMarker: return ToU32(Mix(MarkdownQuote, Background, 0.4f), alpha);
        case S::Rule: return ToU32(Mix(Muted, Background, 0.35f), alpha);
        case S::Table: return ToU32(MarkdownTable, alpha);
        case S::Frontmatter: return ToU32(Muted, alpha);
        case S::Image: return ToU32(MarkdownImage, alpha);
        case S::Tag: return ToU32(Cyan, alpha);
        case S::Strike: return ToU32(Muted, alpha);
        case S::Callout: return ToU32(Cyan, alpha);
        case S::Comment: return ToU32(Mix(Muted, Background, 0.2f), alpha);
        case S::Highlight:
        case S::Default:
        default: return ToU32(Primary, alpha);
        }
    }

    void DrawStyledText(ImDrawList* drawList, float size, ImVec2 position, ImU32 color, const char* begin,
                        const char* end, bool bold, bool italic)
    {
        ImFont* font = ImGui::GetFont();
        const int vertexStart = drawList->VtxBuffer.Size;
        drawList->AddText(font, size, position, color, begin, end);
        if (bold)
        {
            // Faux bold keeps the single monospace font family
            drawList->AddText(font, size, ImVec2(position.x + std::max(0.6f, size * 0.045f), position.y), color, begin, end);
        }
        if (italic)
        {
            const float baseline = position.y + size * 0.8f;
            for (int i = vertexStart; i < drawList->VtxBuffer.Size; ++i)
            {
                ImDrawVert& vertex = drawList->VtxBuffer[i];
                vertex.pos.x += (baseline - vertex.pos.y) * 0.18f;
            }
        }
    }

    InlineLayout LayoutInline(std::string_view text, const InlineOptions& options)
    {
        InlineLayout layout;
        layout.sourceStart = options.offsetBase;
        layout.sourceEnd = options.offsetBase + static_cast<std::uint32_t>(text.size());

        std::vector<std::uint8_t> styles(text.size(), static_cast<std::uint8_t>(S::Default));
        if (options.code)
        {
            int state = options.codeState;
            const auto tokens = HighlightCodeLine(options.language, text, &state);
            for (std::size_t i = 0; i < tokens.size(); ++i)
            {
                styles[i] = static_cast<std::uint8_t>(CodeStyleBase + static_cast<int>(tokens[i]));
            }
            layout.codeStateOut = state;
        }
        else
        {
            const auto styled = StyleLivePreviewLine(text, false, 0, options.reveal);
            for (std::size_t i = 0; i < styled.styles.size(); ++i)
            {
                styles[i] = static_cast<std::uint8_t>(styled.styles[i]);
            }
            for (const auto& [start, end] : styled.strikes)
            {
                layout.strikes.emplace_back(options.offsetBase + static_cast<std::uint32_t>(start),
                                            options.offsetBase + static_cast<std::uint32_t>(end));
            }
        }

        bool isTask = false;
        if (!options.code)
        {
            for (const auto style : styles)
            {
                if (style >= S::Heading1 && style <= S::Heading6 && layout.headingLevel == 0)
                {
                    layout.headingLevel = style - S::Heading1 + 1;
                }
                if (style == S::Task || style == S::TaskDone)
                {
                    isTask = true;
                }
                if (style == S::QuoteMarker)
                {
                    ++layout.quoteDepth;
                }
            }
            layout.isRule = !text.empty() && !options.reveal &&
                            std::all_of(styles.begin(), styles.end(), [](std::uint8_t style) { return style == S::Rule; });
        }

        const float fontSize = HeadingFontSize(layout.headingLevel, options.fontSize, options.headingScale);
        const float spaceWidth = TextWidth(fontSize, " ", " " + 1);
        const bool render = !options.code && !options.reveal;

        // Pass 1: atoms with unwrapped widths
        std::vector<Atom>& atoms = layout.atoms;
        atoms.reserve(text.size());
        if (layout.isRule)
        {
            Atom atom;
            atom.start = options.offsetBase;
            atom.end = options.offsetBase + static_cast<std::uint32_t>(text.size());
            atom.kind = AtomKind::Rule;
            atom.style = S::Rule;
            atom.width = options.wrapWidth;
            atoms.push_back(atom);
        }
        else
        {
            std::size_t i = 0;
            while (i < text.size())
            {
                const int style = styles[i];
                const std::size_t length = std::min<std::size_t>(Utf8Length(static_cast<unsigned char>(text[i])), text.size() - i);
                Atom atom;
                atom.start = options.offsetBase + static_cast<std::uint32_t>(i);
                atom.end = atom.start + static_cast<std::uint32_t>(length);
                atom.style = static_cast<std::uint8_t>(style);

                if (render && style == S::Hidden)
                {
                    i += length;
                    continue;
                }
                if (render && style == S::Bullet && (text[i] == '-' || text[i] == '*' || text[i] == '+'))
                {
                    if (isTask)
                    {
                        // Tasks show only their checkbox, and the space after the bullet goes with it
                        i += length;
                        if (i < text.size() && text[i] == ' ')
                        {
                            ++i;
                        }
                        continue;
                    }
                    atom.kind = AtomKind::Bullet;
                    atom.width = TextWidth(fontSize, "-", "-" + 1);
                    atoms.push_back(atom);
                    i += length;
                    continue;
                }
                if (render && (style == S::Task || style == S::TaskDone) && text[i] == '[' && i + 2 < text.size())
                {
                    atom.kind = AtomKind::Checkbox;
                    atom.end = atom.start + 3;
                    atom.checked = style == S::TaskDone;
                    atom.width = fontSize * 1.15f;
                    atoms.push_back(atom);
                    i += 3;
                    continue;
                }
                if (render && style == S::QuoteMarker && text[i] == '>')
                {
                    atom.kind = AtomKind::QuoteBar;
                    atom.width = fontSize * 0.8f;
                    atoms.push_back(atom);
                    i += length;
                    continue;
                }

                if (text[i] == '\t')
                {
                    atom.width = spaceWidth * 4.0f;
                }
                else
                {
                    atom.width = TextWidth(fontSize, text.data() + i, text.data() + i + length);
                }
                atoms.push_back(atom);
                i += length;
            }
        }

        // Wrapped rows keep the hanging indent of list, task, and quote markers
        float indent = 0.0f;
        if (!options.code)
        {
            float x = 0.0f;
            for (const auto& atom : atoms)
            {
                const std::size_t local = atom.start - options.offsetBase;
                const bool marker = atom.kind != AtomKind::Text || IsMarkerStyle(atom.style) ||
                                    (local < text.size() && (text[local] == ' ' || text[local] == '\t'));
                if (!marker)
                {
                    break;
                }
                x += atom.width;
            }
            indent = std::min(x, options.wrapWidth * 0.5f);
        }

        // Pass 2: wrap into rows
        const float wrapWidth = std::max(options.wrapWidth, fontSize * 4.0f);
        std::vector<std::size_t> rowStarts{0};
        {
            float x = 0.0f;
            std::size_t rowStart = 0;
            std::size_t breakAfter = 0;
            for (std::size_t k = 0; k < atoms.size(); ++k)
            {
                const float rowIndent = rowStarts.size() > 1 ? indent : 0.0f;
                if (k > rowStart && rowIndent + x + atoms[k].width > wrapWidth)
                {
                    std::size_t next = k;
                    if (!options.code && breakAfter > rowStart && breakAfter <= k)
                    {
                        next = breakAfter;
                    }
                    rowStarts.push_back(next);
                    rowStart = next;
                    breakAfter = rowStart;
                    x = 0.0f;
                    for (std::size_t j = next; j < k; ++j)
                    {
                        x += atoms[j].width;
                    }
                }
                x += atoms[k].width;
                const std::size_t local = atoms[k].start - options.offsetBase;
                if (atoms[k].kind == AtomKind::Text && local < text.size() && text[local] == ' ')
                {
                    breakAfter = k + 1;
                }
            }
        }

        const float rowTextHeight = fontSize * 1.18f;
        const float headingGap = layout.headingLevel > 0 && layout.headingLevel <= 3 && options.headingScale
                                     ? fontSize * 0.35f
                                     : 0.0f;
        float y = headingGap;
        for (std::size_t r = 0; r < rowStarts.size(); ++r)
        {
            Row row;
            row.firstAtom = static_cast<std::uint32_t>(rowStarts[r]);
            const std::size_t end = r + 1 < rowStarts.size() ? rowStarts[r + 1] : atoms.size();
            row.atomCount = static_cast<std::uint32_t>(end - rowStarts[r]);
            row.y = y;
            row.textHeight = rowTextHeight;
            row.height = rowTextHeight + options.lineGap;
            float x = r > 0 ? indent : 0.0f;
            for (std::size_t k = rowStarts[r]; k < end; ++k)
            {
                atoms[k].x = x;
                atoms[k].row = static_cast<std::uint32_t>(r);
                x += atoms[k].width;
            }
            layout.width = std::max(layout.width, x);
            row.startOffset = r == 0 ? options.offsetBase : (row.atomCount > 0 ? atoms[rowStarts[r]].start : options.offsetBase);
            row.endOffset = r + 1 < rowStarts.size() ? atoms[rowStarts[r + 1]].start : layout.sourceEnd;
            layout.rows.push_back(row);
            y += row.height;
        }
        layout.height = y;
        return layout;
    }

    void DrawInline(ImDrawList* drawList, const InlineLayout& layout, std::string_view text, ImVec2 origin,
                    const InlineOptions& options, float alpha)
    {
        using namespace Software::Slate::UI;
        const float fontSize = HeadingFontSize(layout.headingLevel, options.fontSize, options.headingScale);
        const auto local = [&](const Atom& atom) { return atom.start - layout.sourceStart; };
        const auto textTop = [&](const Row& row) { return origin.y + row.y + (row.textHeight - fontSize) * 0.5f; };

        // Backgrounds: inline code, highlights, and tag pills merge across neighbouring atoms
        for (const auto& row : layout.rows)
        {
            std::size_t k = row.firstAtom;
            const std::size_t end = row.firstAtom + row.atomCount;
            while (k < end)
            {
                const int style = layout.atoms[k].style;
                if (layout.atoms[k].kind != AtomKind::Text || (style != S::Code && style != S::Highlight && style != S::Tag))
                {
                    ++k;
                    continue;
                }
                std::size_t j = k;
                while (j + 1 < end && layout.atoms[j + 1].style == style && layout.atoms[j + 1].kind == AtomKind::Text)
                {
                    ++j;
                }
                const float pad = fontSize * 0.22f;
                const ImVec2 min(origin.x + layout.atoms[k].x - pad, textTop(row) - fontSize * 0.06f);
                const ImVec2 max(origin.x + layout.atoms[j].x + layout.atoms[j].width + pad, textTop(row) + fontSize * 1.12f);
                ImVec4 fill = style == S::Code ? Panel : (style == S::Highlight ? Amber : Cyan);
                fill.w = style == S::Code ? 1.0f : 0.16f;
                if (style == S::Code)
                {
                    fill = ImVec4(Panel.x + (Muted.x - Panel.x) * 0.12f, Panel.y + (Muted.y - Panel.y) * 0.12f,
                                  Panel.z + (Muted.z - Panel.z) * 0.12f, 1.0f);
                }
                drawList->AddRectFilled(min, max, ToU32(fill, alpha), style == S::Tag ? fontSize * 0.6f : 4.0f);
                k = j + 1;
            }
        }

        // Quote bars run the full height of every row of the line
        if (layout.quoteDepth > 0)
        {
            int depth = 0;
            for (const auto& atom : layout.atoms)
            {
                if (atom.kind != AtomKind::QuoteBar)
                {
                    continue;
                }
                const float barX = origin.x + atom.x + 2.0f;
                drawList->AddRectFilled(ImVec2(barX, origin.y), ImVec2(barX + 3.0f, origin.y + layout.height),
                                        StyleColor(S::QuoteMarker, alpha), 1.5f);
                ++depth;
            }
        }

        for (const auto& row : layout.rows)
        {
            std::size_t k = row.firstAtom;
            const std::size_t end = row.firstAtom + row.atomCount;
            while (k < end)
            {
                const Atom& atom = layout.atoms[k];
                const float top = textTop(row);
                switch (atom.kind)
                {
                case AtomKind::Bullet:
                {
                    const ImVec2 center(origin.x + atom.x + atom.width * 0.5f, top + fontSize * 0.58f);
                    drawList->AddCircleFilled(center, std::max(2.0f, fontSize * 0.17f), StyleColor(S::Bullet, alpha), 12);
                    ++k;
                    continue;
                }
                case AtomKind::Checkbox:
                {
                    const float box = fontSize * 0.78f;
                    const ImVec2 min(origin.x + atom.x + 1.0f, top + (fontSize * 1.16f - box) * 0.5f);
                    const ImVec2 max(min.x + box, min.y + box);
                    if (atom.checked)
                    {
                        drawList->AddRectFilled(min, max, StyleColor(S::TaskDone, alpha), 3.0f);
                        const ImU32 mark = ToU32(Background, alpha);
                        drawList->AddLine(ImVec2(min.x + box * 0.22f, min.y + box * 0.52f),
                                          ImVec2(min.x + box * 0.43f, min.y + box * 0.72f), mark, 2.0f);
                        drawList->AddLine(ImVec2(min.x + box * 0.43f, min.y + box * 0.72f),
                                          ImVec2(min.x + box * 0.8f, min.y + box * 0.28f), mark, 2.0f);
                    }
                    else
                    {
                        drawList->AddRect(min, max, StyleColor(S::Task, alpha), 3.0f, 0, 1.6f);
                    }
                    ++k;
                    continue;
                }
                case AtomKind::QuoteBar:
                    ++k;
                    continue;
                case AtomKind::Rule:
                {
                    const float midY = origin.y + row.y + row.height * 0.5f;
                    drawList->AddLine(ImVec2(origin.x, midY), ImVec2(origin.x + options.wrapWidth, midY),
                                      StyleColor(S::Rule, alpha), 1.0f);
                    ++k;
                    continue;
                }
                case AtomKind::Text:
                    break;
                }

                // Batch neighbouring text atoms with the same style into one draw call
                std::size_t j = k;
                while (j + 1 < end && layout.atoms[j + 1].kind == AtomKind::Text && layout.atoms[j + 1].style == atom.style &&
                       layout.atoms[j + 1].start == layout.atoms[j].end)
                {
                    ++j;
                }
                const char* begin = text.data() + local(atom);
                const char* stop = text.data() + local(layout.atoms[j]) + (layout.atoms[j].end - layout.atoms[j].start);
                const int style = atom.style;
                // Tabs draw as spaces so they never print a missing glyph
                std::string spaced;
                if (std::find(begin, stop, '\t') != stop)
                {
                    spaced.assign(begin, stop);
                    std::replace(spaced.begin(), spaced.end(), '\t', ' ');
                    begin = spaced.data();
                    stop = spaced.data() + spaced.size();
                }
                const ImVec2 position(origin.x + atom.x, top);
                if (std::any_of(begin, stop, [](char ch) { return ch != ' '; }))
                {
                    DrawStyledText(drawList, fontSize, position, StyleColor(style, alpha), begin, stop, IsBold(style),
                                   IsItalic(style));
                }
                if (IsLinkStyle(style))
                {
                    const float underlineY = top + fontSize * 1.04f;
                    drawList->AddLine(ImVec2(position.x, underlineY),
                                      ImVec2(origin.x + layout.atoms[j].x + layout.atoms[j].width, underlineY),
                                      StyleColor(style, alpha * 0.55f), 1.0f);
                }
                k = j + 1;
            }
        }

        // Strikethrough over completed tasks and ~~text~~
        for (const auto& [start, end] : layout.strikes)
        {
            for (const auto& row : layout.rows)
            {
                float x0 = FLT_MAX;
                float x1 = -FLT_MAX;
                for (std::size_t k = row.firstAtom; k < row.firstAtom + row.atomCount; ++k)
                {
                    const Atom& atom = layout.atoms[k];
                    if (atom.start >= start && atom.end <= end && atom.kind == AtomKind::Text)
                    {
                        x0 = std::min(x0, atom.x);
                        x1 = std::max(x1, atom.x + atom.width);
                    }
                }
                if (x1 > x0)
                {
                    const float midY = textTop(row) + fontSize * 0.6f;
                    drawList->AddLine(ImVec2(origin.x + x0, midY), ImVec2(origin.x + x1, midY), StyleColor(S::Strike, alpha),
                                      1.2f);
                }
            }
        }
    }

    float OffsetX(const InlineLayout& layout, const Row& row, std::uint32_t offset)
    {
        float x = row.atomCount > 0 ? layout.atoms[row.firstAtom].x : 0.0f;
        for (std::size_t k = row.firstAtom; k < row.firstAtom + row.atomCount; ++k)
        {
            const Atom& atom = layout.atoms[k];
            if (offset <= atom.start)
            {
                return atom.x;
            }
            if (offset < atom.end)
            {
                return atom.x;
            }
            x = atom.x + atom.width;
        }
        return x;
    }

    std::size_t RowOfOffset(const InlineLayout& layout, std::uint32_t offset)
    {
        for (std::size_t r = 0; r < layout.rows.size(); ++r)
        {
            if (offset < layout.rows[r].endOffset || r + 1 == layout.rows.size())
            {
                return r;
            }
        }
        return 0;
    }

    std::uint32_t OffsetAtPoint(const InlineLayout& layout, ImVec2 local)
    {
        if (layout.rows.empty())
        {
            return layout.sourceStart;
        }
        std::size_t r = 0;
        while (r + 1 < layout.rows.size() && local.y >= layout.rows[r + 1].y)
        {
            ++r;
        }
        const Row& row = layout.rows[r];
        for (std::size_t k = row.firstAtom; k < row.firstAtom + row.atomCount; ++k)
        {
            const Atom& atom = layout.atoms[k];
            if (local.x < atom.x + atom.width * 0.5f)
            {
                return atom.start;
            }
        }
        if (row.atomCount == 0)
        {
            return r + 1 < layout.rows.size() ? row.startOffset : layout.sourceEnd;
        }
        const Atom& last = layout.atoms[row.firstAtom + row.atomCount - 1];
        // Clicking past a wrapped row keeps the caret on that row, before the wrap space
        if (r + 1 < layout.rows.size())
        {
            return last.start;
        }
        return layout.sourceEnd;
    }

    const Atom* AtomAtPoint(const InlineLayout& layout, ImVec2 local)
    {
        for (const auto& row : layout.rows)
        {
            if (local.y < row.y || local.y >= row.y + row.height)
            {
                continue;
            }
            for (std::size_t k = row.firstAtom; k < row.firstAtom + row.atomCount; ++k)
            {
                const Atom& atom = layout.atoms[k];
                if (local.x >= atom.x && local.x < atom.x + atom.width)
                {
                    return &atom;
                }
            }
        }
        return nullptr;
    }
}
