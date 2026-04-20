#pragma once

#include "App/Slate/WorkspaceTree.h"

#include "imgui.h"

#include <string>

namespace Software::Slate::UI
{
    inline const ImVec4 Background{0.055f, 0.055f, 0.052f, 1.0f};
    inline const ImVec4 Primary{0.86f, 0.84f, 0.77f, 1.0f};
    inline const ImVec4 Muted{0.48f, 0.48f, 0.43f, 1.0f};
    inline const ImVec4 Cyan{0.50f, 0.78f, 0.86f, 1.0f};
    inline const ImVec4 Amber{0.93f, 0.70f, 0.35f, 1.0f};
    inline const ImVec4 Green{0.58f, 0.82f, 0.56f, 1.0f};
    inline const ImVec4 Red{0.92f, 0.38f, 0.36f, 1.0f};
    inline const ImVec4 Panel{0.085f, 0.085f, 0.08f, 1.0f};
    inline const ImVec4 Code{0.70f, 0.75f, 0.78f, 1.0f};

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
