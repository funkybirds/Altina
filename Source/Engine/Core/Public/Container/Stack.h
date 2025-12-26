#pragma once

#include "Deque.h"

namespace AltinaEngine::Core::Container
{
    

    template <typename T, typename C = TDeque<T>>
    class TStack
    {
    public:
        using value_type = T;
        using size_type = usize;

        bool IsEmpty() const noexcept { return mContainer.IsEmpty(); }
        size_type Size() const noexcept { return mContainer.Size(); }

        void Push(const value_type& v) { mContainer.PushBack(v); }
        void Push(value_type&& v) { mContainer.PushBack(AltinaEngine::Move(v)); }

        void Pop() { mContainer.PopBack(); }

        value_type& Top() { return mContainer.Back(); }
        const value_type& Top() const { return mContainer.Back(); }

    private:
        C mContainer;
    };

} // namespace AltinaEngine::Core::Container
