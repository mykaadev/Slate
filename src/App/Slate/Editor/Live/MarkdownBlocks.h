#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate::Live
{
    // Multi-line structures render as one widget; everything else renders line by line
    enum class BlockKind
    {
        Line,
        Code,
        Table,
        Callout,
        Frontmatter,
        Math
    };

    struct Block
    {
        BlockKind kind = BlockKind::Line;
        std::size_t firstLine = 0;
        std::size_t lastLine = 0;
        // Code fence info string, for example "lua"
        std::string language;
        // Callout type in lower case, for example "warning"
        std::string calloutType;
        // Callout title text, or empty for the default title
        std::string calloutTitle;
        // '+' or '-' when the callout is foldable, otherwise 0
        char calloutFold = 0;
        // True when a code or math fence never closes
        bool unclosed = false;
    };

    enum class TableAlign
    {
        Left,
        Center,
        Right
    };

    // One table cell with its position inside the source line
    struct TableCell
    {
        std::string_view text;
        std::size_t offset = 0;
    };

    // Splits a Markdown document into blocks; lines are given without line endings
    std::vector<Block> ParseBlocks(const std::vector<std::string_view>& lines);

    // Splits a table row into trimmed cells, honoring escaped pipes
    std::vector<TableCell> SplitTableRow(std::string_view line);
    // Reports whether a line is a table delimiter row such as | --- | :-: |
    bool IsTableDelimiter(std::string_view line);
    // Reads the column alignment from a delimiter row
    std::vector<TableAlign> TableAlignments(std::string_view delimiter);

    // Removes the callout or quote markers ("> ", ">> ") from a line and returns the content offset
    std::size_t QuoteContentOffset(std::string_view line);
    // Default title for a callout type: "note" becomes "Note"
    std::string CalloutDefaultTitle(std::string_view type);
}
