#include "Reflection/BinaryDeserializer.h"
#include "Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Reflection {

    void FBinaryDeserializer::ReadBytes(void* data, usize size) {
        if (!data || size == 0) {
            return;
        }

        if (mPosition + size > mBuffer.Size()) {
            // Not enough data in buffer - this is an error condition
            // For now, just return without reading
            return;
        }

        // Copy bytes from buffer
        Platform::Generic::Memcpy(data, mBuffer.Data() + mPosition, size);
        mPosition += size;
    }

} // namespace AltinaEngine::Core::Reflection
