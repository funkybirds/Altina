#include "Reflection/Object.h"
#include "Reflection/Serialization.h"
#include "Reflection/Serializer.h"
#include "Reflection/Reflection.h"

namespace AltinaEngine::Core::Reflection {

    void FObject::Serialize(ISerializer& serializer) const {
        if (mPtr == nullptr) {
            return;
        }
        Detail::DynamicSerializeInvokerImpl(
            const_cast<void*>(mPtr), serializer, mMetadata.GetHash());
    }

    void FObject::Deserialize(IDeserializer& deserializer) {
        if (mPtr == nullptr) {
            return;
        }
        Detail::DynamicDeserializeInvokerImpl(mPtr, deserializer, mMetadata.GetHash());
    }

} // namespace AltinaEngine::Core::Reflection
