#pragma once

#include "App/Slate/WorkspaceTree.h"

#include "imgui.h"

#include <string>

namespace Software::Slate::UI
{
    inline ImVec4 Background{0.055f, 0.055f, 0.052f, 1.0f};
    inline ImVec4 Primary{0.86f, 0.84f, 0.77f, 1.0f};
    inline ImVec4 Muted{0.48f, 0.48f, 0.43f, 1.0f};
    inline ImVec4 Cyan{0.50f, 0.78f, 0.86f, 1.0f};
    inline ImVec4 Amber{0.93f, 0.70f, 0.35f, 1.0f};
    inline ImVec4 Green{0.58f, 0.82f, 0.56f, 1.0f};
    inline ImVec4 Red{0.92f, 0.38f, 0.36f, 1.0f};
    inline ImVec4 Panel{0.085f, 0.085f, 0.08f, 1.0f};
    inline ImVec4 Code{0.70f, 0.75f, 0.78f, 1.0f};

    inline ImVec4 MarkdownHeading1{0.56f, 0.84f, 0.90f, 1.0f};
    inline ImVec4 MarkdownHeading2{0.70f, 0.88f, 0.68f, 1.0f};
    inline ImVec4 MarkdownHeading3{0.94f, 0.73f, 0.42f, 1.0f};
    inline ImVec4 MarkdownBullet{0.84f, 0.69f, 0.42f, 1.0f};
    inline ImVec4 MarkdownCheckbox{0.88f, 0.74f, 0.45f, 1.0f};
    inline ImVec4 MarkdownCheckboxDone{0.57f, 0.86f, 0.60f, 1.0f};
    inline ImVec4 MarkdownQuote{0.63f, 0.72f, 0.78f, 1.0f};
    inline ImVec4 MarkdownTable{0.78f, 0.74f, 0.56f, 1.0f};
    inline ImVec4 MarkdownLink{0.50f, 0.80f, 0.96f, 1.0f};
    inline ImVec4 MarkdownImage{0.92f, 0.62f, 0.51f, 1.0f};
    inline ImVec4 MarkdownBold{0.96f, 0.92f, 0.84f, 1.0f};
    inline ImVec4 MarkdownItalic{0.76f, 0.80f, 0.67f, 1.0f};
    inline ImVec4 MarkdownCode{0.78f, 0.83f, 0.86f, 1.0f};

    struct TextInputResult
    {
        bool edited = false;
        bool submitted = false;
    };

    TextInputResult InputTextString(const char* label, std::string& text, ImGuiInputTextFlags flags,
                                    int* cursorPos = nullptr, int requestedCursorPos = -1);
    TextInputResult InputTextMultilineString(const char* label, std::string& text, const ImVec2& size,
                                             ImGuiInputTextFlags flags, int* cursorPos = nullptr);
    void TextCentered(const char* text, const ImVec4& color = Primary);
    void TextLine(const char* key, const char* label);
    bool ContainsFilter(const std::string& value, const char* filter);
    bool IsShiftQuestion();
    std::string DisplayNameForTreeRow(const TreeViewRow& row);
}
