#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace Software::Slate::Live
{
    // Token classes for fenced code; colors come from the active theme
    enum class CodeToken : std::uint8_t
    {
        Text,
        Keyword,
        Type,
        String,
        Number,
        Comment,
        Function,
        Constant,
        Punctuation,
        Meta,
        Property,
        Tag
    };

    // Reports whether a fence info string names a language the highlighter knows
    bool IsKnownCodeLanguage(std::string_view language);

    // Classifies every byte of one code line. state carries block comments and
    // multi-line strings between lines; start each block with zero.
    std::vector<CodeToken> HighlightCodeLine(std::string_view language, std::string_view line, int* state);
}
