#include "App/Slate/Overlays/SlateSearchOverlayController.h"

#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
#include "App/Slate/UI/SlateUi.h"
#include "App/Slate/Workspace/SearchService.h"

#include "imgui.h"

#include <algorithm>
#include <utility>

namespace Software::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        bool IsKeyPressed(ImGuiKey key)
        {
            return ImGui::IsKeyPressed(key, false);
        }

        const char* WorkspaceSearchModeLabel(SearchMode mode)
        {
            switch (mode)
            {
            case SearchMode::FileNames:
                return "docs";
            case SearchMode::FullText:
                return "text";
            case SearchMode::Recent:
                return "recent";
            }
            return "docs";
        }

        const ImVec4& WorkspaceSearchModeColor(SearchMode mode)
        {
            switch (mode)
            {
            case SearchMode::FileNames:
                return Cyan;
            case SearchMode::FullText:
                return Amber;
            case SearchMode::Recent:
                return Green;
            }
            return Cyan;
        }

        std::string WorkspaceSearchModeHint(SearchMode mode, const ShortcutService& shortcuts)
        {
            const std::string cycle = shortcuts.Label(ShortcutAction::SearchMode) + " cycles docs / text / recent";
            switch (mode)
            {
            case SearchMode::FileNames:
                return "type to search docs. " + cycle;
            case SearchMode::FullText:
                return "type to search note text. " + cycle;
            case SearchMode::Recent:
                return "recent files. Type to filter. " + cycle;
            }
            return "type to search";
        }

        SearchMode NextWorkspaceSearchMode(SearchMode mode)
        {
            switch (mode)
            {
            case SearchMode::FileNames:
                return SearchMode::FullText;
            case SearchMode::FullText:
                return SearchMode::Recent;
            case SearchMode::Recent:
                return SearchMode::FileNames;
            }
            return SearchMode::FileNames;
        }

        std::vector<SearchResult> QueryRecentFiles(const SlateWorkspaceContext& workspace,
                                                   const std::string& query,
                                                   std::size_t limit)
        {
            std::vector<SearchResult> results;
            for (const auto& recent : workspace.Workspace().RecentFiles())
            {
                if (!query.empty() && !ContainsFilter(recent.generic_string(), query.c_str()))
                {
                    continue;
                }

                SearchResult result;
                result.relativePath = recent;
                result.path = recent;
                result.line = 1;
                result.column = 1;
                result.snippet = "recent";
                result.score = static_cast<int>(limit - std::min(limit, results.size()));
                results.push_back(std::move(result));
                if (results.size() >= limit)
                {
                    break;
                }
            }
            return results;
        }

        std::string NextSearchModeHelper(SearchMode mode, const ShortcutService& shortcuts)
        {
            return shortcuts.Helper(ShortcutAction::SearchMode, WorkspaceSearchModeLabel(NextWorkspaceSearchMode(mode)));
        }
    }

    void SlateSearchOverlayController::Open(bool clearQuery, SearchOverlayScope scope)
    {
        if (clearQuery)
        {
            m_query.clear();
            m_navigation.Reset();
        }
        m_scope = scope;
        if (scope == SearchOverlayScope::Workspace)
        {
            m_mode = SearchMode::FileNames;
        }
        m_open = true;
        m_focusInput = true;
    }

    void SlateSearchOverlayController::Close()
    {
        m_open = false;
    }

    void SlateSearchOverlayController::Reset()
    {
        m_open = false;
        m_focusInput = false;
        m_scope = SearchOverlayScope::Workspace;
        m_mode = SearchMode::FileNames;
        m_query.clear();
        m_results.clear();
        m_navigation.Reset();
    }

    bool SlateSearchOverlayController::IsOpen() const
    {
        return m_open;
    }

    SearchOverlayScope SlateSearchOverlayController::Scope() const
    {
        return m_scope;
    }

    SearchMode SlateSearchOverlayController::Mode() const
    {
        return m_mode;
    }

    const std::string& SlateSearchOverlayController::Query() const
    {
        return m_query;
    }

    void SlateSearchOverlayController::SetQuery(std::string query, bool resetSelection)
    {
        m_query = std::move(query);
        if (resetSelection)
        {
            m_navigation.Reset();
        }
    }

    std::string SlateSearchOverlayController::HelperText(const ShortcutService& shortcuts) const
    {
        const std::string move = "(" + shortcuts.Label(ShortcutAction::NavigateUp) + "/" +
                                 shortcuts.Label(ShortcutAction::NavigateDown) + ") move";
        if (m_scope == SearchOverlayScope::Document)
        {
            return move + "   " + shortcuts.Helper(ShortcutAction::Accept, "jump") + "   " +
                   shortcuts.Helper(ShortcutAction::SearchPrevious, "prev") + "   " +
                   shortcuts.Helper(ShortcutAction::Cancel, "close");
        }
        return move + "   " + shortcuts.Helper(ShortcutAction::Accept, "open") + "   " +
               shortcuts.Helper(ShortcutAction::Cancel, "close") + "   " + NextSearchModeHelper(m_mode, shortcuts);
    }

    void SlateSearchOverlayController::RefreshResults(Software::Core::Runtime::AppContext& context)
    {
        auto workspace = context.services.Resolve<SlateWorkspaceContext>();
        if (!workspace)
        {
            m_results.clear();
            m_navigation.SetCount(0);
            return;
        }

        if (m_scope == SearchOverlayScope::Document)
        {
            const auto* document = workspace->Documents().Active();
            m_results = document ? SearchService::QueryDocument(document->text, m_query, 100)
                                 : std::vector<SearchResult>{};
        }
        else
        {
            m_results = m_mode == SearchMode::Recent
                            ? QueryRecentFiles(*workspace, m_query, 100)
                            : workspace->Search().Query(m_query, m_mode, 100);
        }
        m_navigation.SetCount(m_results.size());
    }

    void SlateSearchOverlayController::ActivateSelected(const SearchOverlayCallbacks& callbacks)
    {
        if (!m_navigation.HasSelection() || m_results.empty() || !callbacks.openSelection)
        {
            return;
        }

        SearchOverlaySelection selection;
        selection.result = m_results[m_navigation.Selected()];
        selection.scope = m_scope;
        selection.mode = m_mode;
        Close();
        callbacks.openSelection(selection);
    }

    void SlateSearchOverlayController::Draw(Software::Core::Runtime::AppContext& context,
                                            const SearchOverlayCallbacks& callbacks)
    {
        if (!m_open)
        {
            return;
        }

        auto workspace = context.services.Resolve<SlateWorkspaceContext>();
        if (!workspace)
        {
            Close();
            return;
        }
        const auto& shortcuts = workspace->Shortcuts();

        RefreshResults(context);

        if (m_scope == SearchOverlayScope::Workspace && shortcuts.Pressed(ShortcutAction::SearchMode))
        {
            m_mode = NextWorkspaceSearchMode(m_mode);
            m_navigation.Reset();
        }
        if (shortcuts.Pressed(ShortcutAction::NavigateDown, true))
        {
            m_navigation.MoveNext();
            m_navigationChanged = true;
        }
        if (shortcuts.Pressed(ShortcutAction::NavigateUp, true))
        {
            m_navigation.MovePrevious();
            m_navigationChanged = true;
        }
        if ((shortcuts.Pressed(ShortcutAction::Accept) || shortcuts.Pressed(ShortcutAction::SearchPrevious)) &&
            m_navigation.HasSelection() && !m_results.empty())
        {
            if (m_scope == SearchOverlayScope::Document && shortcuts.Pressed(ShortcutAction::SearchPrevious))
            {
                m_navigation.MovePrevious();
            }
            ActivateSelected(callbacks);
            return;
        }
        if (shortcuts.Pressed(ShortcutAction::Cancel))
        {
            Close();
            return;
        }

        const bool documentFind = m_scope == SearchOverlayScope::Document;
        const ImVec2 size(std::min(780.0f, std::max(380.0f, ImGui::GetWindowWidth() * 0.56f)),
                          std::min(540.0f, std::max(300.0f, ImGui::GetWindowHeight() * 0.56f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   std::max(40.0f, (ImGui::GetWindowHeight() - Tokens::FooterHeight - size.y) * 0.42f)));
        BeginCard("SearchOverlay", size, Cyan, Tokens::SpaceL);

        // Title row with the search modes as pills; the active mode is solid
        CardTitle(documentFind ? "find in note" : "search", Cyan,
                  m_results.empty() ? nullptr : (std::to_string(m_results.size()) + " results").c_str());
        ImGui::TextColored(TextFaint(), "%s", documentFind ? "this note" : "workspace");
        if (!documentFind)
        {
            float pillX = ImGui::GetWindowContentRegionMax().x;
            std::vector<SearchMode> modes{m_mode};
            for (SearchMode next = NextWorkspaceSearchMode(m_mode); next != m_mode && modes.size() < 6;
                 next = NextWorkspaceSearchMode(next))
            {
                modes.push_back(next);
            }
            std::sort(modes.begin(), modes.end(), [](SearchMode a, SearchMode b) { return static_cast<int>(a) < static_cast<int>(b); });
            for (auto it = modes.rbegin(); it != modes.rend(); ++it)
            {
                pillX -= PillWidth(WorkspaceSearchModeLabel(*it)) + 6.0f;
            }
            ImGui::SameLine(pillX);
            for (const SearchMode mode : modes)
            {
                Pill(WorkspaceSearchModeLabel(mode), WorkspaceSearchModeColor(mode), mode == m_mode);
                ImGui::SameLine(0.0f, 6.0f);
            }
            ImGui::NewLine();
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (m_focusInput)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusInput = false;
        }
        InputTextString("##SearchQuery", m_query, 0);
        ImGui::Dummy(ImVec2(1.0f, 6.0f));

        const bool showQueryHint = m_query.empty() && (documentFind || m_mode != SearchMode::Recent);
        if (showQueryHint)
        {
            const std::string hint = documentFind ? std::string("type to find in this note")
                                                    : WorkspaceSearchModeHint(m_mode, shortcuts);
            ImGui::Dummy(ImVec2(1.0f, 10.0f));
            ImGui::TextColored(TextFaint(), "%s", hint.c_str());
        }
        else if (m_results.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, 10.0f));
            ImGui::TextColored(TextFaint(), "nothing matches \"%s\"", m_query.c_str());
        }
        else
        {
            ImGui::BeginChild("SearchResults", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
            auto* drawList = ImGui::GetWindowDrawList();
            const float lineHeight = ImGui::GetTextLineHeight();
            for (std::size_t i = 0; i < m_results.size(); ++i)
            {
                const auto& result = m_results[i];
                const bool selected = i == m_navigation.Selected();
                const bool twoLines = !result.snippet.empty();
                const float rowHeight = twoLines ? lineHeight * 2.0f + 12.0f : lineHeight + 10.0f;
                ImGui::PushID(static_cast<int>(i));
                ImVec2 rowMin;
                ImVec2 rowMax;
                const bool clicked = ListRow("##result", selected, rowHeight, Green, &rowMin, &rowMax);
                ImGui::PopID();
                if (selected && m_navigationChanged)
                {
                    ImGui::SetScrollHereY(0.5f);
                }

                std::string title;
                std::string detail;
                if (documentFind)
                {
                    title = "line " + std::to_string(result.line);
                    detail = ":" + std::to_string(result.column);
                }
                else
                {
                    title = result.relativePath.stem().generic_string();
                    detail = result.relativePath.parent_path().generic_string();
                    if (m_mode == SearchMode::FullText)
                    {
                        detail += (detail.empty() ? "" : "  ") + std::string("line ") + std::to_string(result.line);
                    }
                }
                const float textX = rowMin.x + 14.0f;
                drawList->AddText(ImVec2(textX, rowMin.y + 5.0f), U32(selected ? Green : Primary), title.c_str());
                drawList->AddText(ImVec2(textX + ImGui::CalcTextSize(title.c_str()).x + 10.0f, rowMin.y + 5.0f), U32(TextFaint()),
                                  detail.c_str());
                if (twoLines)
                {
                    drawList->PushClipRect(rowMin, ImVec2(rowMax.x - 8.0f, rowMax.y), true);
                    drawList->AddText(ImVec2(textX, rowMin.y + 7.0f + lineHeight), U32(TextSecondary()), result.snippet.c_str());
                    drawList->PopClipRect();
                }
                if (clicked)
                {
                    m_navigation.SetSelected(i);
                    ImGui::EndChild();
                    EndCard();
                    ActivateSelected(callbacks);
                    return;
                }
            }
            m_navigationChanged = false;
            ImGui::EndChild();
        }

        EndCard();
    }
}
