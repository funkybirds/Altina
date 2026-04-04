#pragma once

#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Core::Utility::ContainerUtils {
    template <typename TVectorType, typename TValueType>
    void InsertAt(TVectorType& values, usize index, TValueType&& value) {
        TVectorType reordered{};
        const usize oldSize = values.Size();
        if (index > oldSize) {
            index = oldSize;
        }

        reordered.Reserve(oldSize + 1U);
        for (usize i = 0U; i < index; ++i) {
            reordered.PushBack(values[i]);
        }
        reordered.PushBack(AltinaEngine::Forward<TValueType>(value));
        for (usize i = index; i < oldSize; ++i) {
            reordered.PushBack(values[i]);
        }
        values = AltinaEngine::Move(reordered);
    }
} // namespace AltinaEngine::Core::Utility::ContainerUtils
