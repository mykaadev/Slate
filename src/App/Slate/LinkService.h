#pragma once

#include "App/Slate/HistoryService.h"
#include "App/Slate/MarkdownService.h"
#include "App/Slate/WorkspaceService.h"

#include <string>

namespace Software::Slate
{
    class LinkService
    {
    public:
        RewritePlan BuildRenamePlan(const WorkspaceService& workspace, const fs::path& oldRelativePath,
                                    const fs::path& newRelativePath) const;
        bool ApplyRewritePlan(const WorkspaceService& workspace, const RewritePlan& plan, std::string* error = nullptr) const;

    private:
        static bool IsExternalTarget(const std::string& target);
        static std::string StripAnchor(const std::string& target, std::string* anchor);
        static fs::path ResolveMarkdownTarget(const fs::path& sourceRelativePath, const std::string& target);
        static bool WikiTargetMatches(const std::string& target, const fs::path& oldRelativePath);
        static std::string WikiTargetFor(const fs::path& newRelativePath);
        static std::string MarkdownTargetFor(const fs::path& sourceRelativePath, const fs::path& newRelativePath,
                                             const std::string& anchor);
    };
}
