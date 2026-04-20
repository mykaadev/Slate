#pragma once

#include <cstddef>

namespace Software::Slate
{
    class NavigationController
    {
    public:
        void SetCount(std::size_t count);
        void MoveNext();
        void MovePrevious();
        void Reset();
        void SetSelected(std::size_t selected);
        std::size_t Selected() const;
        bool HasSelection() const;

    private:
        std::size_t m_count = 0;
        std::size_t m_selected = 0;
    };
}
