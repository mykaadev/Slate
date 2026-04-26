#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    // Lists editor slash commands that can expand into richer Markdown blocks.
    enum class SlashCommandKind
    {
        Todo,
        Table,
        Callout,
        Image,
        CodeBlock,
        Checklist,
        Quote,
        Divider,
        Link
    };

    // Describes one slash command entry displayed in the inline editor palette.
    struct SlashCommandDefinition
    {
        // Stable command id typed after the slash, for example "todo".
        std::string_view id;
        // Short label shown in the palette.
        std::string_view label;
        // One-line explanation shown under the label.
        std::string_view description;
        // Command behavior used when applying the selected entry.
        SlashCommandKind kind;
    };

    // Provides filtering and template expansion for inline editor slash commands.
    class SlashCommandService
    {
    public:
        // Returns every built-in editor slash command.
        static const std::vector<SlashCommandDefinition>& Commands();
        // Extracts the active slash query token from a line. Returns false when the line does not end in a slash-command token.
        static bool QueryFromLine(std::string_view line, std::string* query = nullptr,
                                  std::size_t* tokenStart = nullptr, std::size_t* tokenEnd = nullptr);
        // Extracts the active slash query token immediately before the caret.
        static bool QueryFromLineAtCaret(std::string_view line,
                                         std::size_t caretColumn,
                                         std::string* query = nullptr,
                                         std::size_t* tokenStart = nullptr,
                                         std::size_t* tokenEnd = nullptr);
        // Returns commands matching a typed slash query.
        static std::vector<SlashCommandDefinition> Filter(std::string_view query);
        // Builds the Markdown replacement block for simple insert commands.
        static std::string BuildMarkdownTemplate(SlashCommandKind kind, std::string_view lineEnding);
    };
}
