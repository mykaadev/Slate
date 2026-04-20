#include "App/Slate/UI/SlateUi.h"

#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <cstring>

namespace Software::Slate::UI
{
    namespace
    {
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
}
