#include "App/Slate/Documents/LinkService.h"

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Core/PathUtils.h"

#include <algorithm>

namespace Software::Slate
{
    RewritePlan LinkService::BuildRenamePlan(const WorkspaceService& workspace, const fs::path& oldRelativePath,
                                             const fs::path& newRelativePath) const
    {
        RewritePlan plan;
        plan.oldRelativePath = PathUtils::NormalizeRelative(oldRelativePath);
        plan.newRelativePath = PathUtils::NormalizeRelative(newRelativePath);

        MarkdownService markdown;
        for (const auto& relativePath : workspace.MarkdownFiles())
        {
            std::string text;
            if (!DocumentService::ReadTextFile(workspace.Resolve(relativePath), &text, nullptr))
            {
                continue;
            }

            const MarkdownSummary summary = markdown.Parse(text);
            std::string updated = text;
            int replacements = 0;

            std::vector<MarkdownLink> links = summary.links;
            std::sort(links.begin(), links.end(), [](const MarkdownLink& a, const MarkdownLink& b) {
                return a.start > b.start;
            });

            for (const auto& link : links)
            {
                if (link.isImage || IsExternalTarget(link.target))
                {
                    continue;
                }

                std::string replacement;
                if (link.isWiki)
                {
                    if (!WikiTargetMatches(link.target, plan.oldRelativePath))
                    {
                        continue;
                    }

                    const std::string newTarget = WikiTargetFor(plan.newRelativePath);
                    replacement = "[[" + newTarget;
                    if (!link.label.empty() && link.label != link.target)
                    {
                        replacement += "|" + link.label;
                    }
                    replacement += "]]";
                }
                else
                {
                    std::string anchor;
                    const std::string targetWithoutAnchor = StripAnchor(link.target, &anchor);
                    const fs::path resolved = ResolveMarkdownTarget(relativePath, targetWithoutAnchor);
                    if (PathUtils::NormalizeRelative(resolved) != plan.oldRelativePath)
                    {
                        continue;
                    }

                    replacement = "[" + link.label + "](" +
                                  MarkdownTargetFor(relativePath, plan.newRelativePath, anchor) + ")";
                }

                updated.replace(link.start, link.length, replacement);
                ++replacements;
            }

            if (replacements > 0)
            {
                RewriteChange change;
                change.path = workspace.Resolve(relativePath);
                change.relativePath = relativePath;
                change.originalText = std::move(text);
                change.updatedText = std::move(updated);
                change.replacementCount = replacements;
                plan.changes.push_back(std::move(change));
            }
        }

        return plan;
    }

    bool LinkService::ApplyRewritePlan(const WorkspaceService& workspace, const RewritePlan& plan, std::string* error) const
    {
        HistoryService history(workspace.Root());
        for (const auto& change : plan.changes)
        {
            if (!history.SnapshotText(change.relativePath, change.originalText, error))
            {
                return false;
            }
            if (!DocumentService::AtomicWriteText(change.path, change.updatedText, error))
            {
                return false;
            }
        }
        return true;
    }

    bool LinkService::IsExternalTarget(const std::string& target)
    {
        const std::string lower = PathUtils::ToLower(target);
        return lower.empty() || lower[0] == '#' || lower.find("://") != std::string::npos ||
               lower.rfind("mailto:", 0) == 0;
    }

    std::string LinkService::StripAnchor(const std::string& target, std::string* anchor)
    {
        const std::size_t hash = target.find('#');
        if (hash == std::string::npos)
        {
            if (anchor)
            {
                anchor->clear();
            }
            return target;
        }

        if (anchor)
        {
            *anchor = target.substr(hash);
        }
        return target.substr(0, hash);
    }

    fs::path LinkService::ResolveMarkdownTarget(const fs::path& sourceRelativePath, const std::string& target)
    {
        return PathUtils::NormalizeRelative(sourceRelativePath.parent_path() / fs::path(target));
    }

    bool LinkService::WikiTargetMatches(const std::string& target, const fs::path& oldRelativePath)
    {
        std::string targetNoAnchor;
        std::string ignored;
        targetNoAnchor = StripAnchor(target, &ignored);
        fs::path targetPath = PathUtils::NormalizeRelative(fs::path(targetNoAnchor));
        if (!PathUtils::IsMarkdownFile(targetPath))
        {
            targetPath += ".md";
        }

        const fs::path oldNormalized = PathUtils::NormalizeRelative(oldRelativePath);
        if (PathUtils::ToLower(targetPath.generic_string()) == PathUtils::ToLower(oldNormalized.generic_string()))
        {
            return true;
        }

        return PathUtils::ToLower(fs::path(targetNoAnchor).stem().string()) ==
               PathUtils::ToLower(oldNormalized.stem().string());
    }

    std::string LinkService::WikiTargetFor(const fs::path& newRelativePath)
    {
        fs::path withoutExtension = newRelativePath;
        withoutExtension.replace_extension();
        return withoutExtension.generic_string();
    }

    std::string LinkService::MarkdownTargetFor(const fs::path& sourceRelativePath, const fs::path& newRelativePath,
                                               const std::string& anchor)
    {
        std::error_code ec;
        fs::path relative = fs::relative(newRelativePath, sourceRelativePath.parent_path(), ec);
        if (ec)
        {
            relative = newRelativePath;
        }
        return PathUtils::NormalizeRelative(relative).generic_string() + anchor;
    }
}
