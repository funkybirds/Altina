#include "Reflection/BinarySerializer.h"
#include "Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Reflection {

    void FBinarySerializer::WriteBytes(const void* data, usize size) {
        if (!data || size == 0) {
            return;
        }

        const usize oldSize = mBuffer.Size();
        mBuffer.Resize(oldSize + size);
        Platform::Generic::Memcpy(mBuffer.Data() + oldSize, data, size);
    }

} // namespace AltinaEngine::Core::Reflection
