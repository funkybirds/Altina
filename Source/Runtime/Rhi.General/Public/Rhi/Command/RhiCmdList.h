#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmd.h"
#include "Container/Allocator.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"
#include "Types/NonCopyable.h"
#include "Types/Traits.h"
#include <new>

using AltinaEngine::CClassBaseOf;
using AltinaEngine::Forward;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TAllocator;
    using Container::TVector;

    class FRhiCmdList : public FNonCopyableClass {
    public:
        FRhiCmdList() = default;
        explicit FRhiCmdList(usize blockSize) : mBlockSize(blockSize) {}
        ~FRhiCmdList() override {
            Reset();
            ReleaseBlocks();
        }

        void Reset() {
            for (auto& entry : mCommands) {
                if (entry.mCommand && entry.mDestroy) {
                    entry.mDestroy(entry.mCommand);
                }
            }
            mCommands.Clear();
            for (auto& block : mBlocks) {
                block.mOffset = 0;
            }
        }

        template <typename TCmd, typename... Args>
            requires CClassBaseOf<FRhiCmd, TCmd>
        auto Emplace(Args&&... args) -> TCmd* {
            void* memory = Allocate(sizeof(TCmd), alignof(TCmd));
            if (!memory) {
                return nullptr;
            }
            auto*         command = ::new (memory) TCmd(Forward<Args>(args)...);
            FCommandEntry entry{};
            entry.mCommand = command;
            entry.mDestroy = &DestroyCommand<TCmd>;
            mCommands.PushBack(entry);
            return command;
        }

        void Execute(FRhiCmdContext& context) {
            for (auto& command : mCommands) {
                if (command.mCommand) {
                    command.mCommand->Execute(context);
                }
            }
        }

        [[nodiscard]] auto GetCommandCount() const noexcept -> u32 {
            return static_cast<u32>(mCommands.Size());
        }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mCommands.IsEmpty(); }

    private:
        struct FBlock {
            u8*   mData   = nullptr;
            usize mSize   = 0;
            usize mOffset = 0;
        };

        struct FCommandEntry {
            FRhiCmd* mCommand                  = nullptr;
            void (*mDestroy)(FRhiCmd* command) = nullptr;
        };

        static constexpr usize               kDefaultBlockSize = 64 * 1024;

        template <typename TCmd> static void DestroyCommand(FRhiCmd* command) {
            static_cast<TCmd*>(command)->~TCmd();
        }

        static auto AlignUp(usize value, usize alignment) -> usize {
            if (alignment == 0) {
                return value;
            }
            const usize mask = alignment - 1;
            return (value + mask) & ~mask;
        }

        auto Allocate(usize size, usize alignment) -> void* {
            if (size == 0) {
                return nullptr;
            }

            if (alignment == 0) {
                alignment = alignof(void*);
            }

            if (mBlocks.IsEmpty()) {
                AllocateBlock(size + alignment);
            }

            FBlock* block  = &mBlocks.Back();
            usize   offset = AlignUp(block->mOffset, alignment);
            if (offset + size > block->mSize) {
                AllocateBlock(size + alignment);
                block  = &mBlocks.Back();
                offset = AlignUp(block->mOffset, alignment);
            }

            block->mOffset = offset + size;
            return block->mData + offset;
        }

        void AllocateBlock(usize minSize) {
            const usize blockSize = (mBlockSize > minSize) ? mBlockSize : minSize;
            u8*         data      = mAllocator.Allocate(blockSize);
            FBlock      block;
            block.mData   = data;
            block.mSize   = blockSize;
            block.mOffset = 0;
            mBlocks.PushBack(block);
        }

        void ReleaseBlocks() {
            for (auto& block : mBlocks) {
                if (block.mData) {
                    mAllocator.Deallocate(block.mData, block.mSize);
                    block.mData = nullptr;
                }
            }
            mBlocks.Clear();
        }

        TVector<FCommandEntry> mCommands;
        TVector<FBlock>        mBlocks;
        TAllocator<u8>         mAllocator;
        usize                  mBlockSize = kDefaultBlockSize;
    };

} // namespace AltinaEngine::Rhi
