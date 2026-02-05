#include "Reflection/BinaryDeserializer.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Reflection/ReflectionBase.h"

namespace AltinaEngine::Core::Reflection {

    void FBinaryDeserializer::ReadBytes(void* data, usize size) {
        if ((data == nullptr) || size == 0) {
            return;
        }

        if (mPosition + size > mBuffer.Size()) {
            ReflectionAssert(
                false, EReflectionErrorCode::DeserializeCorruptedArchive, FReflectionDumpData{});
            return;
        }

        // Copy bytes from buffer
        Platform::Generic::Memcpy(data, mBuffer.Data() + mPosition, size);
        mPosition += size;
    }

} // namespace AltinaEngine::Core::Reflection
