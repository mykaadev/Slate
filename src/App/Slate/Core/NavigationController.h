#pragma once

#include <cstddef>

namespace Software::Slate
{
    // Tracks the active row for list style navigation
    class NavigationController
    {
    public:
        // Sets how many rows are available
        void SetCount(std::size_t count);
        // Moves the selection forward by one row
        void MoveNext();
        // Moves the selection backward by one row
        void MovePrevious();
        // Returns the selection to the first row
        void Reset();
        // Moves the selection to a specific row
        void SetSelected(std::size_t selected);
        // Returns the selected row index
        std::size_t Selected() const;
        // Reports whether a selection exists
        bool HasSelection() const;

    private:
        // Stores the number of selectable rows
        std::size_t m_count = 0;
        // Stores the selected row index
        std::size_t m_selected = 0;
    };
}
