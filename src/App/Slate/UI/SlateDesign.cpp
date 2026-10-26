#include "App/Slate/UI/SlateDesign.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

namespace Software::Slate::UI
{
    namespace
    {
        float SmallFontSize()
        {
            return std::round(ImGui::GetFontSize() * 0.82f);
        }

        bool g_cursorBlink = true;

        // Five-row bitmaps for the block logo; '#' is a filled cell
        struct BlockGlyph
        {
            int width;
            const char* rows[5];
        };

        constexpr BlockGlyph LogoGlyphs[] = {
            {4, {".###", "#...", ".##.", "...#", "###."}},
            {3, {"##.", ".#.", ".#.", ".#.", "###"}},
            {4, {".##.", "...#", ".###", "#..#", ".###"}},
            {4, {".#..", "####", ".#..", ".#..", "..##"}},
            {4, {".##.", "#..#", "####", "#...", ".###"}},
        };
        constexpr int LogoGap = 1;
        constexpr int LogoCursorGap = 1;
    }

    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
    {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w};
    }

    ImVec4 Alpha(ImVec4 color, float alpha)
    {
        color.w = alpha;
        return color;
    }

    ImU32 U32(const ImVec4& color)
    {
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    ImVec4 Surface(int level)
    {
        switch (level)
        {
        case 0: return Background;
        case 1: return Mix(Background, Panel, 0.55f);
        case 2: return Panel;
        default: return Mix(Panel, Muted, 0.12f);
        }
    }

    ImVec4 Border(float strength)
    {
        return Alpha(Mix(Panel, Muted, 0.55f), std::clamp(0.55f * strength, 0.0f, 1.0f));
    }

    ImVec4 TextSecondary()
    {
        return Muted;
    }

    ImVec4 TextFaint()
    {
        return Alpha(Muted, 0.55f);
    }

    void ApplyImGuiTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = Tokens::RadiusM;
        style.FrameRounding = 6.0f;
        style.PopupRounding = Tokens::RadiusM;
        style.GrabRounding = Tokens::RadiusS;
        style.TabRounding = Tokens::RadiusS;
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 5.0f);
        style.PopupBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.WindowBorderSize = 0.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = Primary;
        colors[ImGuiCol_TextDisabled] = TextFaint();
        colors[ImGuiCol_WindowBg] = Background;
        colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_PopupBg] = Alpha(Surface(2), 0.98f);
        colors[ImGuiCol_Border] = Border();
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_FrameBg] = Surface(3);
        colors[ImGuiCol_FrameBgHovered] = Mix(Surface(3), Muted, 0.1f);
        colors[ImGuiCol_FrameBgActive] = Mix(Surface(3), Cyan, 0.08f);
        colors[ImGuiCol_TitleBg] = Surface(1);
        colors[ImGuiCol_TitleBgActive] = Surface(1);
        colors[ImGuiCol_MenuBarBg] = Surface(1);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = Alpha(Muted, 0.28f);
        colors[ImGuiCol_ScrollbarGrabHovered] = Alpha(Muted, 0.5f);
        colors[ImGuiCol_ScrollbarGrabActive] = Alpha(Cyan, 0.7f);
        colors[ImGuiCol_CheckMark] = Background;
        colors[ImGuiCol_SliderGrab] = Cyan;
        colors[ImGuiCol_SliderGrabActive] = Amber;
        colors[ImGuiCol_Button] = Alpha(Cyan, 0.12f);
        colors[ImGuiCol_ButtonHovered] = Alpha(Cyan, 0.22f);
        colors[ImGuiCol_ButtonActive] = Alpha(Cyan, 0.32f);
        colors[ImGuiCol_Header] = Alpha(Cyan, 0.12f);
        colors[ImGuiCol_HeaderHovered] = Alpha(Cyan, 0.18f);
        colors[ImGuiCol_HeaderActive] = Alpha(Cyan, 0.26f);
        colors[ImGuiCol_Separator] = Border();
        colors[ImGuiCol_SeparatorHovered] = Alpha(Amber, 0.42f);
        colors[ImGuiCol_SeparatorActive] = Alpha(Green, 0.5f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TextSelectedBg] = Alpha(Cyan, 0.3f);
        colors[ImGuiCol_DragDropTarget] = Cyan;
        colors[ImGuiCol_NavCursor] = Alpha(Cyan, 0.6f);
        colors[ImGuiCol_ModalWindowDimBg] = Alpha(Background, 0.6f);
        colors[ImGuiCol_TableHeaderBg] = Surface(2);
        colors[ImGuiCol_TableBorderStrong] = Border();
        colors[ImGuiCol_TableBorderLight] = Border(0.5f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TableRowBgAlt] = Alpha(Panel, 0.4f);
    }

    float KeycapHeight()
    {
        return SmallFontSize() + 6.0f;
    }

    float KeycapWidth(std::string_view label)
    {
        const float size = SmallFontSize();
        const float textWidth = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, label.data(), label.data() + label.size()).x;
        return std::max(textWidth + 12.0f, KeycapHeight());
    }

    void DrawKeycap(ImDrawList* drawList, ImVec2 position, std::string_view label, const ImVec4& accent)
    {
        const float size = SmallFontSize();
        const float width = KeycapWidth(label);
        const float height = KeycapHeight();
        const ImVec2 max(position.x + width, position.y + height);
        // A darker lip under the key gives it a little depth
        drawList->AddRectFilled(ImVec2(position.x, position.y + 1.5f), ImVec2(max.x, max.y + 1.5f),
                                U32(Alpha(Mix(Background, accent, 0.18f), 1.0f)), Tokens::RadiusS);
        drawList->AddRectFilled(position, max, U32(Mix(Surface(2), accent, 0.10f)), Tokens::RadiusS);
        drawList->AddRect(position, max, U32(Alpha(accent, 0.38f)), Tokens::RadiusS);
        const float textWidth = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, label.data(), label.data() + label.size()).x;
        drawList->AddText(ImGui::GetFont(), size, ImVec2(position.x + (width - textWidth) * 0.5f, position.y + 3.0f), U32(accent),
                          label.data(), label.data() + label.size());
    }

    float KeyHintWidth(std::string_view key, std::string_view label)
    {
        return KeycapWidth(key) + 6.0f + ImGui::CalcTextSize(label.data(), label.data() + label.size()).x;
    }

    void KeyHint(std::string_view key, std::string_view label, const ImVec4& accent)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float capY = start.y + (lineHeight - KeycapHeight()) * 0.5f;
        DrawKeycap(ImGui::GetWindowDrawList(), ImVec2(start.x, capY), key, accent);
        const float labelX = start.x + KeycapWidth(key) + 6.0f;
        ImGui::GetWindowDrawList()->AddText(ImVec2(labelX, start.y), U32(Primary), label.data(), label.data() + label.size());
        ImGui::Dummy(ImVec2(KeyHintWidth(key, label), lineHeight));
    }

    float PillWidth(std::string_view text)
    {
        const float size = SmallFontSize();
        return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text.data(), text.data() + text.size()).x + 14.0f;
    }

    float PillHeight()
    {
        return SmallFontSize() + 6.0f;
    }

    void DrawPill(ImDrawList* drawList, ImVec2 position, std::string_view text, const ImVec4& color, bool solid)
    {
        const float size = SmallFontSize();
        const ImVec2 max(position.x + PillWidth(text), position.y + size + 6.0f);
        drawList->AddRectFilled(position, max, U32(solid ? color : Alpha(color, 0.16f)), Tokens::RadiusS);
        drawList->AddText(ImGui::GetFont(), size, ImVec2(position.x + 7.0f, position.y + 3.0f), U32(solid ? Background : color),
                          text.data(), text.data() + text.size());
    }

    void Pill(std::string_view text, const ImVec4& color, bool solid)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float lineHeight = ImGui::GetTextLineHeight();
        DrawPill(ImGui::GetWindowDrawList(), ImVec2(start.x, start.y + (lineHeight - SmallFontSize() - 6.0f) * 0.5f), text, color, solid);
        ImGui::Dummy(ImVec2(PillWidth(text), lineHeight));
    }

    void SectionLabel(const char* text, const ImVec4& color, bool rule)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float available = ImGui::GetContentRegionAvail().x;
        auto* drawList = ImGui::GetWindowDrawList();
        const float mid = start.y + lineHeight * 0.55f;
        drawList->AddRectFilled(ImVec2(start.x, mid - 3.0f), ImVec2(start.x + 6.0f, mid + 3.0f), U32(color));
        drawList->AddText(ImVec2(start.x + 13.0f, start.y), U32(Alpha(color, 0.9f)), text);
        const float textEnd = start.x + 13.0f + ImGui::CalcTextSize(text).x;
        if (rule && available > textEnd - start.x + 40.0f)
        {
            drawList->AddLine(ImVec2(textEnd + 10.0f, mid), ImVec2(start.x + available, mid), U32(Border(0.7f)), 1.0f);
        }
        ImGui::Dummy(ImVec2(textEnd - start.x, lineHeight));
    }

    bool ListRow(const char* id, bool selected, float height, const ImVec4& accent, ImVec2* rowMin, ImVec2* rowMax, float width)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float rowWidth = width > 0.0f ? width : std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(rowWidth, height));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 max(start.x + rowWidth, start.y + height);
        auto* drawList = ImGui::GetWindowDrawList();
        if (selected)
        {
            drawList->AddRectFilled(start, max, U32(Alpha(accent, 0.13f)), 6.0f);
            drawList->AddRectFilled(ImVec2(start.x, start.y + 4.0f), ImVec2(start.x + 3.0f, max.y - 4.0f), U32(accent), 1.5f);
        }
        else if (hovered)
        {
            drawList->AddRectFilled(start, max, U32(Alpha(Muted, 0.10f)), 6.0f);
        }
        if (hovered)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if (rowMin)
        {
            *rowMin = start;
        }
        if (rowMax)
        {
            *rowMax = max;
        }
        return clicked;
    }

    void BeginCard(const char* id, ImVec2 size, const ImVec4& accent, float padding)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Alpha(Surface(2), 0.985f));
        ImGui::PushStyleColor(ImGuiCol_Border, Mix(Border(1.4f), Alpha(accent, 0.6f), 0.25f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Tokens::RadiusL);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
        ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    }

    void EndCard()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void CardTitle(const char* title, const ImVec4& accent, const char* subtitle)
    {
        // Drawn on the card's own top border, outside its clip rect, like a tmux pane title
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const float windowWidth = ImGui::GetWindowWidth();
        auto* drawList = ImGui::GetWindowDrawList();
        const float size = SmallFontSize();
        ImFont* font = ImGui::GetFont();
        const float y = windowPos.y;
        drawList->PushClipRect(ImVec2(windowPos.x - 2.0f, y - size), ImVec2(windowPos.x + windowWidth + 2.0f, y + size), false);
        const ImVec4 knockout = Alpha(Surface(2), 1.0f);

        // Accent stroke along the top-left edge marks the pane as live
        drawList->AddLine(ImVec2(windowPos.x + Tokens::RadiusL, y), ImVec2(windowPos.x + 18.0f, y), U32(accent), 2.0f);
        const std::string label = std::string(" ") + title + " ";
        const float labelWidth = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label.c_str()).x;
        const float labelX = windowPos.x + 20.0f;
        drawList->AddRectFilled(ImVec2(labelX, y - size * 0.5f - 1.0f), ImVec2(labelX + labelWidth, y + size * 0.5f + 1.0f), U32(knockout));
        drawList->AddText(font, size, ImVec2(labelX, y - size * 0.5f - 1.0f), U32(accent), label.c_str());
        if (subtitle && *subtitle)
        {
            const std::string right = std::string(" ") + subtitle + " ";
            const float rightWidth = font->CalcTextSizeA(size, FLT_MAX, 0.0f, right.c_str()).x;
            const float rightX = windowPos.x + windowWidth - 18.0f - rightWidth;
            if (rightX > labelX + labelWidth + 12.0f)
            {
                drawList->AddRectFilled(ImVec2(rightX, y - size * 0.5f - 1.0f), ImVec2(rightX + rightWidth, y + size * 0.5f + 1.0f),
                                        U32(knockout));
                drawList->AddText(font, size, ImVec2(rightX, y - size * 0.5f - 1.0f), U32(TextSecondary()), right.c_str());
            }
        }
        drawList->PopClipRect();
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceXS));
    }

    void ScreenHeader(const char* title, const char* subtitle, const char* right, const ImVec4& accent)
    {
        SectionLabel(title, accent);
        if (subtitle && *subtitle)
        {
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(TextSecondary(), "%s", subtitle);
        }
        if (right && *right)
        {
            const float width = ImGui::CalcTextSize(right).x;
            ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 16.0f, ImGui::GetWindowContentRegionMax().x - width));
            ImGui::TextColored(TextFaint(), "%s", right);
        }
        const ImVec2 rule = ImGui::GetCursorScreenPos();
        const float left = ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(left, rule.y + 4.0f),
                                            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x, rule.y + 4.0f),
                                            U32(Border(0.8f)), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, 12.0f));
    }

    void DrawLogoMark(ImDrawList* drawList, ImVec2 center, float size, float alpha)
    {
        // A slate tile with three written lines; the last line is shorter, like a paragraph end
        const float half = size * 0.5f;
        const ImVec2 min(center.x - half, center.y - half);
        const ImVec2 max(center.x + half, center.y + half);
        drawList->AddRectFilled(min, max, U32(Alpha(Mix(Panel, Cyan, 0.18f), alpha)), size * 0.24f);
        drawList->AddRect(min, max, U32(Alpha(Cyan, 0.75f * alpha)), size * 0.24f, 0, std::max(1.0f, size * 0.06f));
        const float inset = size * 0.24f;
        const float thickness = std::max(1.5f, size * 0.09f);
        const float lines[3] = {1.0f, 0.78f, 0.5f};
        for (int i = 0; i < 3; ++i)
        {
            const float y = min.y + size * (0.32f + 0.18f * static_cast<float>(i));
            const ImVec4 color = i == 2 ? Amber : Primary;
            drawList->AddLine(ImVec2(min.x + inset, y), ImVec2(min.x + inset + (size - inset * 2.0f) * lines[i], y),
                              U32(Alpha(color, (i == 2 ? 0.95f : 0.8f) * alpha)), thickness);
        }
    }

    void Wordmark(float size, double timeSeconds)
    {
        const char* word = "slate";
        ImGui::PushFont(nullptr, size);
        const float wordWidth = ImGui::CalcTextSize(word).x;
        const float cursorWidth = size * 0.3f;
        const float markSize = size * 0.82f;
        const float gap = size * 0.32f;
        const float total = markSize + gap + wordWidth + size * 0.1f + cursorWidth;
        const float x = (ImGui::GetWindowWidth() - total) * 0.5f;
        ImGui::SetCursorPosX(x + markSize + gap);
        const ImVec2 textPos = ImGui::GetCursorScreenPos();
        ImGui::TextColored(Primary, "%s", word);
        ImGui::PopFont();

        auto* drawList = ImGui::GetWindowDrawList();
        DrawLogoMark(drawList, ImVec2(textPos.x - gap - markSize * 0.5f, textPos.y + size * 0.58f), markSize);
        if (CursorVisible(timeSeconds))
        {
            const float cursorX = textPos.x + wordWidth + size * 0.1f;
            drawList->AddRectFilled(ImVec2(cursorX, textPos.y + size * 0.2f), ImVec2(cursorX + cursorWidth, textPos.y + size * 1.02f),
                                    U32(Alpha(Amber, 0.9f)), 2.0f);
        }
    }

    void SetCursorBlink(bool enabled)
    {
        g_cursorBlink = enabled;
    }

    bool CursorVisible(double timeSeconds)
    {
        return !g_cursorBlink || std::fmod(timeSeconds, 1.1) < 0.65;
    }

    ImVec2 BlockLogoSize(float cell)
    {
        int columns = 0;
        for (const auto& glyph : LogoGlyphs)
        {
            columns += glyph.width + LogoGap;
        }
        // The cursor block is two cells wide and starts right after the last letter's gap
        columns += LogoCursorGap + 1;
        // Half a cell extra on each axis for the shadow
        return ImVec2(cell * (static_cast<float>(columns) + 0.5f), cell * 5.5f);
    }

    void DrawBlockLogo(ImDrawList* drawList, ImVec2 position, float cell, double timeSeconds, float alpha)
    {
        const float shadow = cell * 0.45f;
        const float pad = std::max(0.0f, cell * 0.08f);
        // Two passes: the shadow first, then the letters, so every shadow sits behind every letter
        for (int pass = 0; pass < 2; ++pass)
        {
            float x = position.x;
            for (const auto& glyph : LogoGlyphs)
            {
                for (int row = 0; row < 5; ++row)
                {
                    // Phosphor gradient: cyan at the top fading to green at the bottom
                    const ImVec4 color = pass == 0 ? Alpha(Mix(Background, Muted, 0.45f), 0.9f * alpha)
                                                   : Alpha(Mix(Cyan, Green, static_cast<float>(row) / 4.0f), alpha);
                    for (int column = 0; column < glyph.width; ++column)
                    {
                        if (glyph.rows[row][column] != '#')
                        {
                            continue;
                        }
                        const float offset = pass == 0 ? shadow : 0.0f;
                        const ImVec2 min(x + cell * static_cast<float>(column) + offset, position.y + cell * static_cast<float>(row) + offset);
                        drawList->AddRectFilled(ImVec2(min.x + pad, min.y + pad), ImVec2(min.x + cell - pad, min.y + cell - pad), U32(color));
                    }
                }
                x += cell * static_cast<float>(glyph.width + LogoGap);
            }
            if (pass == 1 && CursorVisible(timeSeconds))
            {
                // The terminal cursor: a full-height amber block after the word
                const float cursorX = x + cell * static_cast<float>(LogoCursorGap - 1);
                drawList->AddRectFilled(ImVec2(cursorX + pad, position.y + cell + pad), ImVec2(cursorX + cell * 2.0f - pad, position.y + cell * 5.0f - pad),
                                        U32(Alpha(Amber, 0.92f * alpha)));
            }
        }
    }

    void BlockLogo(float cell, double timeSeconds)
    {
        const ImVec2 size = BlockLogoSize(cell);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - size.x) * 0.5f);
        const ImVec2 position = ImGui::GetCursorScreenPos();
        DrawBlockLogo(ImGui::GetWindowDrawList(), position, cell, timeSeconds);
        ImGui::Dummy(size);
    }

    float PowerlineWidth(std::string_view text, float height)
    {
        return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x + 20.0f + height * 0.42f;
    }

    float PowerlineSegment(ImDrawList* drawList, float x, float top, float height, std::string_view text, const ImVec4& background,
                           const ImVec4& foreground, bool pointRight)
    {
        const float arrow = height * 0.42f;
        const float textWidth = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        const float width = textWidth + 20.0f;
        const float bottom = top + height;
        const ImU32 fill = U32(background);
        if (pointRight)
        {
            drawList->AddRectFilled(ImVec2(x, top), ImVec2(x + width, bottom), fill);
            drawList->AddTriangleFilled(ImVec2(x + width, top), ImVec2(x + width + arrow, top + height * 0.5f), ImVec2(x + width, bottom), fill);
            drawList->AddText(ImVec2(x + 10.0f, top + (height - ImGui::GetTextLineHeight()) * 0.5f), U32(foreground), text.data(),
                              text.data() + text.size());
            return x + width + arrow;
        }
        // Left-pointing segments are laid out from the right edge: x is where the segment ends
        const float start = x - width;
        drawList->AddRectFilled(ImVec2(start, top), ImVec2(x, bottom), fill);
        drawList->AddTriangleFilled(ImVec2(start, top), ImVec2(start, bottom), ImVec2(start - arrow, top + height * 0.5f), fill);
        drawList->AddText(ImVec2(start + 10.0f, top + (height - ImGui::GetTextLineHeight()) * 0.5f), U32(foreground), text.data(),
                          text.data() + text.size());
        return start - arrow;
    }

    std::string Ellipsize(std::string_view text, float maxWidth)
    {
        const std::string full(text);
        if (ImGui::CalcTextSize(full.c_str()).x <= maxWidth)
        {
            return full;
        }
        const float dots = ImGui::CalcTextSize("...").x;
        std::size_t length = full.size();
        while (length > 0)
        {
            --length;
            // Never cut through a UTF-8 sequence
            while (length > 0 && (static_cast<unsigned char>(full[length]) & 0xC0) == 0x80)
            {
                --length;
            }
            if (ImGui::CalcTextSize(full.data(), full.data() + length).x + dots <= maxWidth)
            {
                break;
            }
        }
        return full.substr(0, length) + "...";
    }

    std::string FriendlyPath(const std::string& path)
    {
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');
#if defined(_WIN32)
        char* profile = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&profile, &length, "USERPROFILE") == 0 && profile)
        {
            std::string home = profile;
            std::free(profile);
            std::replace(home.begin(), home.end(), '\\', '/');
            if (!home.empty() && result.size() >= home.size() && _strnicmp(result.c_str(), home.c_str(), home.size()) == 0)
            {
                result = "~" + result.substr(home.size());
            }
        }
#endif
        return result;
    }

    void DrawScanlines(int level)
    {
        if (level <= 0)
        {
            return;
        }
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        auto* drawList = ImGui::GetForegroundDrawList(viewport);
        const ImVec2 min = viewport->Pos;
        const ImVec2 max(min.x + viewport->Size.x, min.y + viewport->Size.y);
        const ImU32 line = U32(ImVec4(0.0f, 0.0f, 0.0f, level == 1 ? 0.18f : 0.34f));
        for (float y = min.y + 1.0f; y < max.y; y += 3.0f)
        {
            drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), line, 1.0f);
        }
        // A soft vignette darkens the edges like a curved tube
        const ImU32 clear = U32(ImVec4(0, 0, 0, 0));
        const ImU32 edge = U32(ImVec4(0, 0, 0, level == 1 ? 0.25f : 0.45f));
        const float band = std::min(viewport->Size.x, viewport->Size.y) * 0.12f;
        drawList->AddRectFilledMultiColor(min, ImVec2(max.x, min.y + band), edge, edge, clear, clear);
        drawList->AddRectFilledMultiColor(ImVec2(min.x, max.y - band), max, clear, clear, edge, edge);
        drawList->AddRectFilledMultiColor(min, ImVec2(min.x + band, max.y), edge, clear, clear, edge);
        drawList->AddRectFilledMultiColor(ImVec2(max.x - band, min.y), max, clear, edge, edge, clear);
    }
}
