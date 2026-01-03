#pragma once

#include "Reflection/Serializer.h"
#include "Container/Vector.h"

namespace AltinaEngine::Core::Reflection {
    using Container::TVector;

    /**
     * @brief Basic binary serializer that writes data to a byte buffer.
     *
     * This serializer writes primitive types directly to a binary format
     * without any schema or metadata. It's intended for simple serialization
     * scenarios where the data structure is known at deserialization time.
     */
    class AE_CORE_API FBinarySerializer final : public ISerializer {
    public:
        FBinarySerializer()           = default;
        ~FBinarySerializer() override = default;

        /**
         * @brief Get the serialized data buffer.
         * @return Const reference to the internal byte buffer.
         */
        [[nodiscard]] auto GetBuffer() const -> const TVector<u8>& { return mBuffer; }

        /**
         * @brief Clear the internal buffer.
         */
        void               Clear() { mBuffer.Clear(); }

        /**
         * @brief Reserve space in the buffer.
         * @param capacity Number of bytes to reserve.
         */
        void               Reserve(usize capacity) { mBuffer.Reserve(capacity); }

        /**
         * @brief Get the current size of the serialized data.
         * @return Number of bytes in the buffer.
         */
        [[nodiscard]] auto Size() const -> usize { return mBuffer.Size(); }

    protected:
        void WriteBytes(const void* data, usize size) override;

    private:
        TVector<u8> mBuffer;
    };

} // namespace AltinaEngine::Core::Reflection
