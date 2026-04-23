#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Software::Slate
{
    namespace fs = std::filesystem;

    // Describes one scanned path inside the workspace
    struct WorkspaceEntry
    {
        // Stores the absolute path on disk
        fs::path path;
        // Stores the path relative to the workspace root
        fs::path relativePath;
        // Marks directory rows so the browser can branch correctly
        bool isDirectory = false;
        // Stores how deep the row sits in the tree
        int depth = 0;
    };

    // Describes one markdown heading
    struct Heading
    {
        // Stores the markdown heading level
        int level = 1;
        // Stores the one based source line
        std::size_t line = 0;
        // Stores the visible heading text
        std::string title;
    };

    // Describes one parsed markdown link
    struct MarkdownLink
    {
        // Stores the one based source line
        std::size_t line = 0;
        // Stores the byte offset inside the document
        std::size_t start = 0;
        // Stores the byte length inside the document
        std::size_t length = 0;
        // Stores the visible label text
        std::string label;
        // Stores the target text
        std::string target;
        // Marks wiki links like double bracket links
        bool isWiki = false;
        // Marks image links
        bool isImage = false;
    };

    // Describes one parsed markdown tag
    struct MarkdownTag
    {
        // Stores the one based source line
        std::size_t line = 0;
        // Stores the byte offset inside the document
        std::size_t start = 0;
        // Stores the byte length inside the document
        std::size_t length = 0;
        // Stores the tag text without the hash
        std::string name;
    };

    // Lists the todo states used by the editor
    enum class TodoState
    {
        Open,
        Research,
        Doing,
        Done
    };

    // Describes one parsed todo ticket
    struct TodoTicket
    {
        // Stores the document that owns the todo
        fs::path relativePath;
        // Stores the first line of the todo block
        std::size_t line = 0;
        // Stores the last line of the todo block
        std::size_t endLine = 0;
        // Stores the todo title text
        std::string title;
        // Stores the todo description text
        std::string description;
        // Stores the current todo state
        TodoState state = TodoState::Open;
        // Mirrors done style tickets from legacy checkbox format
        bool done = false;
        // Stores tags found in the todo block
        std::vector<std::string> tags;
    };

    // Collects parsed markdown details for one document
    struct MarkdownSummary
    {
        // Marks whether frontmatter exists
        bool hasFrontmatter = false;
        // Stores the last frontmatter line when present
        std::size_t frontmatterEndLine = 0;
        // Stores parsed headings
        std::vector<Heading> headings;
        // Stores parsed links
        std::vector<MarkdownLink> links;
        // Stores parsed tags
        std::vector<MarkdownTag> tags;
        // Stores parsed todo blocks
        std::vector<TodoTicket> todos;
    };

    // Describes one search result row
    struct SearchResult
    {
        // Stores the resolved path for the result
        fs::path path;
        // Stores the workspace relative path
        fs::path relativePath;
        // Stores the one based source line
        std::size_t line = 0;
        // Stores the one based source column
        std::size_t column = 0;
        // Stores the snippet shown in the list
        std::string snippet;
        // Stores the rank score used for sorting
        int score = 0;
    };

    // Lists the available search scopes
    enum class SearchMode
    {
        FileNames,
        FullText,
        Recent
    };

    // Describes one styled inline markdown span
    struct MarkdownInlineSpan
    {
        // Stores the visible text for this span
        std::string text;
        // Marks bold spans
        bool bold = false;
        // Marks italic spans
        bool italic = false;
        // Marks inline code spans
        bool code = false;
        // Marks link spans
        bool link = false;
        // Marks image spans
        bool image = false;
    };

    // Stores one path rewrite result
    struct RewriteChange
    {
        // Stores the absolute file path
        fs::path path;
        // Stores the workspace relative file path
        fs::path relativePath;
        // Stores the file text before rewriting
        std::string originalText;
        // Stores the file text after rewriting
        std::string updatedText;
        // Stores how many replacements were applied
        int replacementCount = 0;
    };

    // Stores the full rename rewrite plan
    struct RewritePlan
    {
        // Stores the original relative path
        fs::path oldRelativePath;
        // Stores the new relative path
        fs::path newRelativePath;
        // Stores the file edits needed for the rename
        std::vector<RewriteChange> changes;

        // Counts the total replacements in the plan
        int TotalReplacements() const
        {
            int total = 0;
            for (const auto& change : changes)
            {
                total += change.replacementCount;
            }
            return total;
        }
    };

    // Describes one workspace entry in the vault registry
    struct WorkspaceVault
    {
        // Stores the stable vault id
        std::string id;
        // Stores the short emoji label
        std::string emoji;
        // Stores the display title
        std::string title;
        // Stores the root path on disk
        fs::path path;
    };
}
