#pragma once

#include "Deque.h"

namespace AltinaEngine::Core::Container
{
    

    template <typename T, typename C = TDeque<T>>
    class TQueue
    {
    public:
        using value_type = T;
        using size_type = usize;

        bool IsEmpty() const noexcept { return mContainer.IsEmpty(); }
        size_type Size() const noexcept { return mContainer.Size(); }

        void Push(const value_type& v) { mContainer.PushBack(v); }
        void Push(value_type&& v) { mContainer.PushBack(std::move(v)); }

        void Pop() { mContainer.PopFront(); }

        value_type& Front() { return mContainer.Front(); }
        const value_type& Front() const { return mContainer.Front(); }

    private:
        C mContainer;
    };

} // namespace AltinaEngine::Core::Container
