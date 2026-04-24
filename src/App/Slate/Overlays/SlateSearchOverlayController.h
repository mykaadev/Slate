#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>
#include <vector>

namespace Software::Slate
{
    // Splits search overlay behavior between workspace and document search.
    enum class SearchOverlayScope
    {
        Workspace,
        Document
    };

    // Describes the result selected by the user in the search overlay.
    struct SearchOverlaySelection
    {
        SearchResult result;
        SearchOverlayScope scope = SearchOverlayScope::Workspace;
        SearchMode mode = SearchMode::FileNames;
    };

    // Callback surface used by the shell/mode that owns navigation side effects.
    struct SearchOverlayCallbacks
    {
        std::function<void(const SearchOverlaySelection& selection)> openSelection;
    };

    // Owns search overlay state, result querying, keyboard navigation, and rendering.
    class SlateSearchOverlayController
    {
    public:
        void Open(bool clearQuery, SearchOverlayScope scope = SearchOverlayScope::Workspace);
        void Close();
        void Reset();

        bool IsOpen() const;
        SearchOverlayScope Scope() const;
        SearchMode Mode() const;
        const std::string& Query() const;

        void SetQuery(std::string query, bool resetSelection = true);
        std::string HelperText() const;

        void Draw(Software::Core::Runtime::AppContext& context, const SearchOverlayCallbacks& callbacks);

    private:
        void RefreshResults(Software::Core::Runtime::AppContext& context);
        void ActivateSelected(const SearchOverlayCallbacks& callbacks);

        NavigationController m_navigation;
        std::vector<SearchResult> m_results;
        std::string m_query;
        bool m_open = false;
        bool m_focusInput = false;
        SearchOverlayScope m_scope = SearchOverlayScope::Workspace;
        SearchMode m_mode = SearchMode::FileNames;
    };
}
