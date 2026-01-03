#pragma once

#include "Reflection/Serializer.h"
#include "Container/Vector.h"

namespace AltinaEngine::Core::Reflection {
    using Container::TVector;

    /**
     * @brief Basic binary deserializer that reads data from a byte buffer.
     *
     * This deserializer reads primitive types directly from a binary format.
     * It must be used with data serialized by FBinarySerializer.
     */
    class AE_CORE_API FBinaryDeserializer final : public IDeserializer {
    public:
        FBinaryDeserializer() = default;
        explicit FBinaryDeserializer(const TVector<u8>& buffer) : mBuffer(buffer), mPosition(0) {}
        explicit FBinaryDeserializer(TVector<u8>&& buffer) : mBuffer(Move(buffer)), mPosition(0) {}
        ~FBinaryDeserializer() override = default;

        /**
         * @brief Set the buffer to deserialize from.
         * @param buffer The byte buffer containing serialized data.
         */
        void SetBuffer(const TVector<u8>& buffer) {
            mBuffer   = buffer;
            mPosition = 0;
        }

        /**
         * @brief Set the buffer to deserialize from (move version).
         * @param buffer The byte buffer containing serialized data.
         */
        void SetBuffer(TVector<u8>&& buffer) {
            mBuffer   = Move(buffer);
            mPosition = 0;
        }

        /**
         * @brief Get the current read position.
         * @return Current position in the buffer.
         */
        [[nodiscard]] auto GetPosition() const -> usize { return mPosition; }

        /**
         * @brief Reset the read position to the beginning.
         */
        void               Reset() { mPosition = 0; }

        /**
         * @brief Check if there is more data to read.
         * @return True if there is unread data, false otherwise.
         */
        [[nodiscard]] auto HasMoreData() const -> bool { return mPosition < mBuffer.Size(); }

    protected:
        void ReadBytes(void* data, usize size) override;

    private:
        TVector<u8> mBuffer;
        usize       mPosition{ 0 };
    };

} // namespace AltinaEngine::Core::Reflection
