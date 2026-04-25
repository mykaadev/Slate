#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "App/Slate/Input/ShortcutService.h"
#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>
#include <vector>

namespace Software::Slate
{
    /** Splits search overlay behavior between workspace and document search. */
    enum class SearchOverlayScope
    {
        /** Searches across the active workspace. */
        Workspace,

        /** Searches inside the active document only. */
        Document
    };

    /** Describes the result selected by the user in the search overlay. */
    struct SearchOverlaySelection
    {
        /** Selected search result row. */
        SearchResult result;

        /** Scope that produced the result. */
        SearchOverlayScope scope = SearchOverlayScope::Workspace;

        /** Search mode that produced the result. */
        SearchMode mode = SearchMode::FileNames;
    };

    /** Callback surface used by the shell/mode that owns navigation side effects. */
    struct SearchOverlayCallbacks
    {
        /** Opens or jumps to the selected search result. */
        std::function<void(const SearchOverlaySelection& selection)> openSelection;
    };

    /** Owns search overlay state, result querying, keyboard navigation, and rendering. */
    class SlateSearchOverlayController
    {
    public:
        /** Opens the overlay and optionally clears the query. */
        void Open(bool clearQuery, SearchOverlayScope scope = SearchOverlayScope::Workspace);

        /** Closes the overlay while preserving query state. */
        void Close();

        /** Clears query, results, navigation, and visibility. */
        void Reset();

        /** Returns whether the overlay is visible. */
        bool IsOpen() const;

        /** Returns the active search scope. */
        SearchOverlayScope Scope() const;

        /** Returns the active search mode. */
        SearchMode Mode() const;

        /** Returns the current query text. */
        const std::string& Query() const;

        /** Sets the query text and optionally resets selection. */
        void SetQuery(std::string query, bool resetSelection = true);

        /** Returns helper text for the global status bar. */
        std::string HelperText(const ShortcutService& shortcuts) const;

        /** Draws the overlay and handles local keyboard input. */
        void Draw(Software::Core::Runtime::AppContext& context, const SearchOverlayCallbacks& callbacks);

    private:
        /** Rebuilds visible results from the current query/scope/mode. */
        void RefreshResults(Software::Core::Runtime::AppContext& context);

        /** Emits the selected result through callbacks. */
        void ActivateSelected(const SearchOverlayCallbacks& callbacks);

        /** Navigation state for result rows. */
        NavigationController m_navigation;

        /** Results currently visible in the overlay. */
        std::vector<SearchResult> m_results;

        /** Current query text. */
        std::string m_query;

        /** True while the overlay is visible. */
        bool m_open = false;

        /** True when the query input should receive focus. */
        bool m_focusInput = false;

        /** Current search scope. */
        SearchOverlayScope m_scope = SearchOverlayScope::Workspace;

        /** Current search mode. */
        SearchMode m_mode = SearchMode::FileNames;
    };
}
