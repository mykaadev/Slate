#include "App/Slate/UI/SlateUi.h"

#include "App/Slate/MarkdownService.h"
#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <gdiplus.h>
#include <GLFW/glfw3.h>
#endif

namespace Software::Slate::UI
{
    namespace
    {
#if defined(_WIN32)
        struct GdiplusSession
        {
            GdiplusSession()
            {
                Gdiplus::GdiplusStartupInput input;
                ok = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
            }

            ~GdiplusSession()
            {
                if (ok)
                {
                    Gdiplus::GdiplusShutdown(token);
                }
            }

            ULONG_PTR token = 0;
            bool ok = false;
        };

        struct CachedImageTexture
        {
            unsigned int texture = 0;
            int width = 0;
            int height = 0;
            bool failed = false;
        };

        std::unordered_map<std::string, CachedImageTexture>& ImageCache()
        {
            static std::unordered_map<std::string, CachedImageTexture> cache;
            return cache;
        }

        bool LoadTextureFromFile(const fs::path& absolutePath, CachedImageTexture* out)
        {
            static GdiplusSession gdiplus;
            if (!out || !gdiplus.ok || !fs::exists(absolutePath))
            {
                return false;
            }

            Gdiplus::Bitmap bitmap(absolutePath.wstring().c_str(), false);
            if (bitmap.GetLastStatus() != Gdiplus::Ok || bitmap.GetWidth() == 0 || bitmap.GetHeight() == 0)
            {
                return false;
            }

            const int width = static_cast<int>(bitmap.GetWidth());
            const int height = static_cast<int>(bitmap.GetHeight());
            Gdiplus::Rect rect(0, 0, width, height);
            Gdiplus::BitmapData data{};
            if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
            {
                return false;
            }

            std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
            const auto* source = static_cast<const unsigned char*>(data.Scan0);
            const int stride = data.Stride;
            for (int y = 0; y < height; ++y)
            {
                const unsigned char* sourceRow = stride >= 0
                                                     ? source + y * stride
                                                     : source + (height - 1 - y) * (-stride);
                for (int x = 0; x < width; ++x)
                {
                    const std::size_t dest = (static_cast<std::size_t>(y) * width + x) * 4;
                    const unsigned char blue = sourceRow[x * 4 + 0];
                    const unsigned char green = sourceRow[x * 4 + 1];
                    const unsigned char red = sourceRow[x * 4 + 2];
                    const unsigned char alpha = sourceRow[x * 4 + 3];
                    pixels[dest + 0] = red;
                    pixels[dest + 1] = green;
                    pixels[dest + 2] = blue;
                    pixels[dest + 3] = alpha;
                }
            }
            bitmap.UnlockBits(&data);

            unsigned int texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         width,
                         height,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         pixels.data());

            out->texture = texture;
            out->width = width;
            out->height = height;
            out->failed = false;
            return texture != 0;
        }
#endif

        template <typename Callback>
        void ForEachShortcutSegment(std::string_view text, Callback&& callback)
        {
            std::size_t cursor = 0;
            while (cursor < text.size())
            {
                if (text[cursor] == '(')
                {
                    const std::size_t close = text.find(')', cursor + 1);
                    if (close != std::string_view::npos)
                    {
                        callback(text.substr(cursor, close - cursor + 1), true);
                        cursor = close + 1;
                        continue;
                    }
                }

                const std::size_t next =
                    text.find('(', text[cursor] == '(' ? cursor + 1 : cursor);
                const std::size_t end = next == std::string_view::npos ? text.size() : next;
                callback(text.substr(cursor, end - cursor), false);
                if (next == std::string_view::npos)
                {
                    break;
                }
                cursor = next;
            }
        }

        std::size_t LeadingSpaceCount(const std::string& line)
        {
            std::size_t count = 0;
            while (count < line.size() && line[count] == ' ')
            {
                ++count;
            }
            return count;
        }

        std::string VisibleWhitespace(std::string_view text)
        {
            std::string out;
            out.reserve(text.size());
            for (const char ch : text)
            {
                if (ch == ' ')
                {
                    out += "\xC2\xB7";
                }
                else if (ch == '\t')
                {
                    out += "\xE2\x86\x92";
                }
                else
                {
                    out.push_back(ch);
                }
            }
            return out;
        }

        std::string DisplayText(std::string_view text, bool showWhitespace)
        {
            return showWhitespace ? VisibleWhitespace(text) : std::string(text);
        }

        struct TextInputPayload
        {
            std::string* text = nullptr;
            int* cursorPos = nullptr;
            int requestedCursorPos = -1;
            bool edited = false;
        };

        int TextInputCallback(ImGuiInputTextCallbackData* data)
        {
            auto* payload = static_cast<TextInputPayload*>(data->UserData);
            if (!payload || !payload->text)
            {
                return 0;
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                payload->text->resize(static_cast<std::size_t>(data->BufTextLen));
                data->Buf = payload->text->data();
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && payload->cursorPos)
            {
                if (payload->requestedCursorPos >= 0)
                {
                    const int cursor = std::clamp(payload->requestedCursorPos, 0, data->BufTextLen);
                    data->CursorPos = cursor;
                    data->SelectionStart = cursor;
                    data->SelectionEnd = cursor;
                }
                *payload->cursorPos = data->CursorPos;
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
            {
                payload->edited = true;
            }

            return 0;
        }
    }

    TextInputResult InputTextString(const char* label, std::string& text, ImGuiInputTextFlags flags, int* cursorPos,
                                    int requestedCursorPos)
    {
        if (text.capacity() < text.size() + 1024)
        {
            text.reserve(text.size() + 1024);
        }

        TextInputPayload payload;
        payload.text = &text;
        payload.cursorPos = cursorPos;
        payload.requestedCursorPos = requestedCursorPos;
        flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways |
                 ImGuiInputTextFlags_CallbackEdit;
        const bool submitted =
            ImGui::InputText(label, text.data(), text.capacity() + 1, flags, TextInputCallback, &payload);
        text.resize(std::strlen(text.c_str()));
        return {payload.edited, submitted};
    }

    TextInputResult InputTextMultilineString(const char* label, std::string& text, const ImVec2& size,
                                             ImGuiInputTextFlags flags, int* cursorPos)
    {
        if (text.capacity() < text.size() + 4096)
        {
            text.reserve(text.size() + 4096);
        }

        TextInputPayload payload;
        payload.text = &text;
        payload.cursorPos = cursorPos;
        flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways |
                 ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AllowTabInput;
        const bool submitted = ImGui::InputTextMultiline(label, text.data(), text.capacity() + 1, size, flags,
                                                         TextInputCallback, &payload);
        text.resize(std::strlen(text.c_str()));
        return {payload.edited, submitted};
    }

    float CenteredColumnWidth(float maxWidth, float minMargin)
    {
        const float available = ImGui::GetContentRegionAvail().x;
        return std::max(1.0f, std::min(maxWidth, std::max(1.0f, available - minMargin * 2.0f)));
    }

    void SetCursorCenteredForWidth(float width)
    {
        const float x = ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f);
        ImGui::SetCursorPosX(x);
    }

    void PushSlateScrollbarStyle(int style)
    {
        style = std::clamp(style, 0, 2);
        const float size = style == 0 ? 7.0f : (style == 2 ? 12.0f : 9.0f);
        const float backgroundAlpha = style == 0 ? 0.12f : (style == 2 ? 0.30f : 0.20f);
        const float grabAlpha = style == 0 ? 0.34f : (style == 2 ? 0.58f : 0.42f);
        const float hoverAlpha = style == 0 ? 0.58f : (style == 2 ? 0.78f : 0.66f);
        const float activeAlpha = style == 0 ? 0.76f : (style == 2 ? 0.94f : 0.82f);

        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(Panel.x, Panel.y, Panel.z, backgroundAlpha));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(Cyan.x, Cyan.y, Cyan.z, grabAlpha));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(Cyan.x, Cyan.y, Cyan.z, hoverAlpha));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(Amber.x, Amber.y, Amber.z, activeAlpha));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, size);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, size);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarPadding, style == 2 ? 4.0f : 3.0f);
    }

    void PopSlateScrollbarStyle()
    {
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);
    }

    bool DrawVerticalDocumentScrollbar(const char* id, const ImVec2& size, int firstVisibleLine,
                                       int visibleLineCount, int totalLineCount, int* targetFirstVisibleLine,
                                       int style)
    {
        const int maxFirstLine = std::max(0, totalLineCount - std::max(1, visibleLineCount));
        if (maxFirstLine <= 0 || size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        style = std::clamp(style, 0, 2);
        ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        auto* drawList = ImGui::GetWindowDrawList();

        const float rounding = std::min(size.x * 0.5f, 7.0f);
        const float backgroundAlpha = style == 0 ? 0.14f : (style == 2 ? 0.34f : 0.26f);
        const float grabAlpha = style == 0 ? 0.40f : (style == 2 ? 0.62f : 0.48f);
        const float hoverAlpha = style == 0 ? 0.62f : (style == 2 ? 0.80f : 0.70f);
        const float activeAlpha = style == 0 ? 0.80f : (style == 2 ? 0.96f : 0.86f);
        drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(Panel.x, Panel.y, Panel.z, backgroundAlpha)),
                                rounding);

        const float visibleRatio =
            std::clamp(static_cast<float>(std::max(1, visibleLineCount)) / static_cast<float>(std::max(1, totalLineCount)),
                       0.05f, 1.0f);
        const float grabHeight = std::clamp(size.y * visibleRatio, style == 2 ? 42.0f : 34.0f, size.y);
        const float travel = std::max(1.0f, size.y - grabHeight);
        const float t = static_cast<float>(std::clamp(firstVisibleLine, 0, maxFirstLine)) /
                        static_cast<float>(std::max(1, maxFirstLine));
        const float grabY = min.y + travel * t;
        const ImVec4 grabColor = active ? ImVec4(Amber.x, Amber.y, Amber.z, activeAlpha)
                               : hovered ? ImVec4(Cyan.x, Cyan.y, Cyan.z, hoverAlpha)
                                         : ImVec4(Cyan.x, Cyan.y, Cyan.z, grabAlpha);
        drawList->AddRectFilled(ImVec2(min.x + 1.0f, grabY),
                                ImVec2(max.x - 1.0f, grabY + grabHeight),
                                ImGui::ColorConvertFloat4ToU32(grabColor),
                                rounding);

        if (!active || !targetFirstVisibleLine)
        {
            return false;
        }

        const float mouseY = ImGui::GetIO().MousePos.y;
        const float targetT = std::clamp((mouseY - min.y - grabHeight * 0.5f) / travel, 0.0f, 1.0f);
        *targetFirstVisibleLine = static_cast<int>(std::lround(targetT * static_cast<float>(maxFirstLine)));
        return true;
    }

    void TextCentered(const char* text, const ImVec4& color)
    {
        const float width = ImGui::CalcTextSize(text).x;
        const float x = ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), x));
        ImGui::TextColored(color, "%s", text);
    }

    void TextLine(const char* key, const char* label)
    {
        ImGui::TextColored(Amber, "(%s)", key);
        ImGui::SameLine();
        ImGui::TextColored(Primary, "%s", label);
    }

    float ShortcutTextWidth(std::string_view text)
    {
        float width = 0.0f;
        ForEachShortcutSegment(text, [&](std::string_view segment, bool) {
            if (segment.empty())
            {
                return;
            }
            width += ImGui::CalcTextSize(segment.data(), segment.data() + segment.size()).x;
        });
        return width;
    }

    void DrawShortcutText(std::string_view text, const ImVec4& keyColor, const ImVec4& labelColor)
    {
        bool first = true;
        ForEachShortcutSegment(text, [&](std::string_view segment, bool isKey) {
            if (segment.empty())
            {
                return;
            }

            if (!first)
            {
                ImGui::SameLine(0.0f, 0.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, isKey ? keyColor : labelColor);
            ImGui::TextUnformatted(segment.data(), segment.data() + segment.size());
            ImGui::PopStyleColor();
            first = false;
        });

        if (first)
        {
            ImGui::TextUnformatted("");
        }
    }

    void DrawShortcutTextCentered(std::string_view text, const ImVec4& keyColor, const ImVec4& labelColor)
    {
        const float width = ShortcutTextWidth(text);
        const float x = ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), x));
        DrawShortcutText(text, keyColor, labelColor);
    }

    bool ContainsFilter(const std::string& value, const char* filter)
    {
        const std::string needle = PathUtils::ToLower(PathUtils::Trim(filter));
        return needle.empty() || PathUtils::ToLower(value).find(needle) != std::string::npos;
    }

    bool IsShiftQuestion()
    {
        const ImGuiIO& io = ImGui::GetIO();
        return io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Slash, false);
    }

    std::string DisplayNameForTreeRow(const TreeViewRow& row)
    {
        if (row.relativePath == ".")
        {
            return ".";
        }
        return row.relativePath.filename().string();
    }

    void DrawMarkdownImage(const fs::path& absolutePath, std::string_view alt, std::string_view target,
                           float maxWidth)
    {
        const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        maxWidth = maxWidth > 1.0f ? std::min(maxWidth, availableWidth) : availableWidth;
        const std::string label = !alt.empty() ? std::string(alt)
                                  : !target.empty() ? fs::path(std::string(target)).filename().string()
                                                    : std::string("image");

        const auto drawPlaceholder = [&](std::string_view detail) {
            const float width = std::min(maxWidth, 420.0f);
            const ImVec2 size(width, 76.0f);
            ImGui::InvisibleButton("##MarkdownImagePlaceholder", size);
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(Panel));
            drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(MarkdownImage));
            drawList->AddText(ImVec2(min.x + 12.0f, min.y + 13.0f),
                              ImGui::ColorConvertFloat4ToU32(MarkdownImage),
                              label.c_str());
            if (!detail.empty())
            {
                const std::string detailText(detail);
                drawList->AddText(ImVec2(min.x + 12.0f, min.y + 38.0f),
                                  ImGui::ColorConvertFloat4ToU32(Muted),
                                  detailText.c_str());
            }
        };

        const std::string cacheKey = absolutePath.empty() ? std::string(target)
                                                          : absolutePath.lexically_normal().string();
        ImGui::PushID(cacheKey.c_str());

#if defined(_WIN32)
        auto& cached = ImageCache()[cacheKey];
        if (cached.texture == 0 && !cached.failed)
        {
            cached.failed = !LoadTextureFromFile(absolutePath, &cached);
        }

        if (cached.texture != 0 && cached.width > 0 && cached.height > 0)
        {
            const float widthScale = maxWidth / static_cast<float>(cached.width);
            const float heightScale = 360.0f / static_cast<float>(cached.height);
            const float scale = std::min(1.0f, std::min(widthScale, heightScale));
            const ImVec2 size(static_cast<float>(cached.width) * scale,
                              static_cast<float>(cached.height) * scale);
            ImGui::Image(static_cast<ImTextureID>(cached.texture), size);
        }
        else
#endif
        {
            const std::string detail = target.empty() ? absolutePath.generic_string() : std::string(target);
            drawPlaceholder(detail.empty() ? "missing image" : detail);
        }

        ImGui::PopID();
    }

    void DrawInlineSpans(const std::vector<MarkdownInlineSpan>& spans, const ImVec4& baseColor, bool showWhitespace)
    {
        bool first = true;
        for (const auto& span : spans)
        {
            if (span.text.empty())
            {
                continue;
            }
            if (!first)
            {
                ImGui::SameLine(0.0f, 0.0f);
            }

            ImVec4 color = baseColor;
            if (span.code)
            {
                color = MarkdownCode;
            }
            else if (span.image)
            {
                color = MarkdownImage;
            }
            else if (span.link)
            {
                color = MarkdownLink;
            }
            else if (span.bold)
            {
                color = MarkdownBold;
            }
            else if (span.italic)
            {
                color = MarkdownItalic;
            }

            const std::string displayText = DisplayText(span.text, showWhitespace);
            ImGui::TextColored(color, "%s", displayText.c_str());
            if (span.bold)
            {
                const ImVec2 min = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 0.7f, min.y),
                                                    ImGui::ColorConvertFloat4ToU32(color),
                                                    displayText.c_str());
            }
            first = false;
        }

        if (first)
        {
            ImGui::TextUnformatted("");
        }
    }

    void DrawMarkdownLine(const std::string& line, bool inCodeFence, bool inFrontmatter, bool showWhitespace,
                          float fontScale)
    {
        fontScale = std::max(0.65f, fontScale);
        const std::string trimmed = PathUtils::Trim(line);

        if (trimmed.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, ImGui::GetTextLineHeight()));
            return;
        }

        const float indentWidth = static_cast<float>(LeadingSpaceCount(line)) * 7.0f;
        if (indentWidth > 0.0f)
        {
            ImGui::Indent(indentWidth);
        }

        if (inFrontmatter)
        {
            const std::string displayText = DisplayText(trimmed, showWhitespace);
            ImGui::TextColored(Muted, "%s", displayText.c_str());
        }
        else if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0)
        {
            ImGui::TextColored(MarkdownCode, "code");
        }
        else if (inCodeFence)
        {
            const std::string displayText = DisplayText(line, showWhitespace);
            ImGui::TextColored(MarkdownCode, "%s", displayText.c_str());
        }
        else if (trimmed[0] == '#')
        {
            std::size_t hashes = 0;
            while (hashes < trimmed.size() && trimmed[hashes] == '#')
            {
                ++hashes;
            }
            const std::string title = PathUtils::Trim(std::string_view(trimmed).substr(hashes));
            const float scale = 1.0f;
            const ImVec4 headingColor = hashes == 1 ? MarkdownHeading1 : (hashes == 2 ? MarkdownHeading2 : MarkdownHeading3);
            ImGui::SetWindowFontScale(fontScale * scale);
            DrawInlineSpans(MarkdownService::ParseInlineSpans(title.empty() ? "heading" : title), headingColor,
                            showWhitespace);
            ImGui::SetWindowFontScale(fontScale);
        }
        else if (trimmed.rfind("- [ ]", 0) == 0 || trimmed.rfind("* [ ]", 0) == 0 ||
                 trimmed.rfind("+ [ ]", 0) == 0)
        {
            ImGui::TextColored(MarkdownCheckbox, "[ ]");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(PathUtils::Trim(trimmed.substr(5))), Primary,
                            showWhitespace);
        }
        else if (trimmed.rfind("- [x]", 0) == 0 || trimmed.rfind("- [X]", 0) == 0 || trimmed.rfind("* [x]", 0) == 0 ||
                 trimmed.rfind("* [X]", 0) == 0 || trimmed.rfind("+ [x]", 0) == 0 || trimmed.rfind("+ [X]", 0) == 0)
        {
            ImGui::TextColored(MarkdownCheckboxDone, "[x]");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(PathUtils::Trim(trimmed.substr(5))), Primary,
                            showWhitespace);
        }
        else if (trimmed[0] == '>')
        {
            ImGui::TextColored(MarkdownQuote, "|");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(PathUtils::Trim(trimmed.substr(1))), MarkdownQuote,
                            showWhitespace);
        }
        else if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0 || trimmed.rfind("+ ", 0) == 0)
        {
            ImGui::TextColored(MarkdownBullet, "*");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed.substr(2)), Primary, showWhitespace);
        }
        else if (!trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0])))
        {
            const std::size_t dot = trimmed.find(". ");
            if (dot != std::string::npos)
            {
                ImGui::TextColored(MarkdownBullet, "%s", trimmed.substr(0, dot + 1).c_str());
                ImGui::SameLine();
                DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed.substr(dot + 2)), Primary, showWhitespace);
            }
            else
            {
                DrawInlineSpans(MarkdownService::ParseInlineSpans(line), Primary, showWhitespace);
            }
        }
        else if (trimmed.find("![](") != std::string::npos || trimmed.find("![") != std::string::npos)
        {
            DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed), MarkdownImage, showWhitespace);
        }
        else if (trimmed.find("](") != std::string::npos || trimmed.find("[[") != std::string::npos)
        {
            DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed), Primary, showWhitespace);
        }
        else if (trimmed.find('|') != std::string::npos)
        {
            const std::string displayText = DisplayText(trimmed, showWhitespace);
            ImGui::TextColored(MarkdownTable, "%s", displayText.c_str());
        }
        else
        {
            DrawInlineSpans(MarkdownService::ParseInlineSpans(line), Primary, showWhitespace);
        }

        if (indentWidth > 0.0f)
        {
            ImGui::Unindent(indentWidth);
        }
    }
}
