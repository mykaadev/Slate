#include "App/Slate/UI/SlateUi.h"

#include "App/Slate/MarkdownService.h"
#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>

namespace Software::Slate::UI
{
    namespace
    {
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

    void DrawInlineSpans(const std::vector<MarkdownInlineSpan>& spans, const ImVec4& baseColor)
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

            ImGui::TextColored(color, "%s", span.text.c_str());
            if (span.bold)
            {
                const ImVec2 min = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 0.7f, min.y),
                                                    ImGui::ColorConvertFloat4ToU32(color),
                                                    span.text.c_str());
            }
            first = false;
        }

        if (first)
        {
            ImGui::TextUnformatted("");
        }
    }

    void DrawMarkdownLine(const std::string& line, bool inCodeFence, bool inFrontmatter)
    {
        const std::string trimmed = PathUtils::Trim(line);

        if (trimmed.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, ImGui::GetTextLineHeightWithSpacing()));
            return;
        }

        const float indentWidth = static_cast<float>(LeadingSpaceCount(line)) * 7.0f;
        if (indentWidth > 0.0f)
        {
            ImGui::Indent(indentWidth);
        }

        if (inFrontmatter)
        {
            ImGui::TextColored(Muted, "%s", trimmed.c_str());
        }
        else if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0)
        {
            ImGui::TextColored(MarkdownCode, "code");
        }
        else if (inCodeFence)
        {
            ImGui::TextColored(MarkdownCode, "%s", line.c_str());
        }
        else if (trimmed[0] == '#')
        {
            std::size_t hashes = 0;
            while (hashes < trimmed.size() && trimmed[hashes] == '#')
            {
                ++hashes;
            }
            const std::string title = PathUtils::Trim(std::string_view(trimmed).substr(hashes));
            const float scale = hashes == 1 ? 1.42f : (hashes == 2 ? 1.25f : 1.10f);
            const ImVec4 headingColor = hashes == 1 ? MarkdownHeading1 : (hashes == 2 ? MarkdownHeading2 : MarkdownHeading3);
            ImGui::SetWindowFontScale(scale);
            DrawInlineSpans(MarkdownService::ParseInlineSpans(title.empty() ? "heading" : title), headingColor);
            ImGui::SetWindowFontScale(1.0f);
        }
        else if (trimmed.rfind("- [ ]", 0) == 0 || trimmed.rfind("* [ ]", 0) == 0)
        {
            ImGui::TextColored(MarkdownCheckbox, "[ ]");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(PathUtils::Trim(trimmed.substr(5))), Primary);
        }
        else if (trimmed.rfind("- [x]", 0) == 0 || trimmed.rfind("- [X]", 0) == 0 || trimmed.rfind("* [x]", 0) == 0 ||
                 trimmed.rfind("* [X]", 0) == 0)
        {
            ImGui::TextColored(MarkdownCheckboxDone, "[x]");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(PathUtils::Trim(trimmed.substr(5))), Primary);
        }
        else if (trimmed[0] == '>')
        {
            ImGui::TextColored(MarkdownQuote, "|");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(PathUtils::Trim(trimmed.substr(1))), MarkdownQuote);
        }
        else if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0 || trimmed.rfind("+ ", 0) == 0)
        {
            ImGui::TextColored(MarkdownBullet, "*");
            ImGui::SameLine();
            DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed.substr(2)), Primary);
        }
        else if (!trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0])))
        {
            const std::size_t dot = trimmed.find(". ");
            if (dot != std::string::npos)
            {
                ImGui::TextColored(MarkdownBullet, "%s", trimmed.substr(0, dot + 1).c_str());
                ImGui::SameLine();
                DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed.substr(dot + 2)), Primary);
            }
            else
            {
                DrawInlineSpans(MarkdownService::ParseInlineSpans(line), Primary);
            }
        }
        else if (trimmed.find("![](") != std::string::npos || trimmed.find("![") != std::string::npos)
        {
            DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed), MarkdownImage);
        }
        else if (trimmed.find("](") != std::string::npos || trimmed.find("[[") != std::string::npos)
        {
            DrawInlineSpans(MarkdownService::ParseInlineSpans(trimmed), Primary);
        }
        else if (trimmed.find('|') != std::string::npos)
        {
            ImGui::TextColored(MarkdownTable, "%s", trimmed.c_str());
        }
        else
        {
            DrawInlineSpans(MarkdownService::ParseInlineSpans(line), Primary);
        }

        if (indentWidth > 0.0f)
        {
            ImGui::Unindent(indentWidth);
        }
    }
}
