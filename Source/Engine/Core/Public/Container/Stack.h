#pragma once

#include "Deque.h"

using AltinaEngine::Move;
namespace AltinaEngine::Core::Container {

    template <typename T, typename C = TDeque<T>> class TStack {
    public:
        using TValueType = T;
        using TSizeType  = usize;

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mContainer.IsEmpty(); }
        [[nodiscard]] auto Size() const noexcept -> TSizeType { return mContainer.Size(); }

        void               Push(const TValueType& v) { mContainer.PushBack(v); }
        void               Push(TValueType&& v) { mContainer.PushBack(Move(v)); }

        void               Pop() { mContainer.PopBack(); }

        [[nodiscard]] auto Top() -> TValueType& { return mContainer.Back(); }
        [[nodiscard]] auto Top() const -> const TValueType& { return mContainer.Back(); }

    private:
        C mContainer;
    };

} // namespace AltinaEngine::Core::Container
