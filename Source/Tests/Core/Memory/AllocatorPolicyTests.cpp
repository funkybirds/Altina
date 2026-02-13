#include "TestHarness.h"

#include "Memory/AllocatorExecutor.h"
#include "Memory/BuddyAllocatorPolicy.h"
#include "Memory/RingAllocatorPolicy.h"

namespace {
    using AltinaEngine::u8;
    using AltinaEngine::Core::Memory::FBuddyAllocatorPolicy;
    using AltinaEngine::Core::Memory::FMemoryBufferBacking;
    using AltinaEngine::Core::Memory::FRingAllocatorPolicy;
    using AltinaEngine::Core::Memory::TAllocatorExecutor;
} // namespace

TEST_CASE("AllocatorPolicy.Ring.Wrap") {
    FRingAllocatorPolicy ring(32ULL);

    const auto           a = ring.Allocate(12ULL, 4ULL, 1ULL);
    REQUIRE(a.IsValid());
    REQUIRE_EQ(a.mOffset, 0ULL);

    const auto b = ring.Allocate(12ULL, 4ULL, 2ULL);
    REQUIRE(b.IsValid());
    REQUIRE_EQ(b.mOffset, 12ULL);

    const auto c = ring.Allocate(4ULL, 4ULL, 3ULL);
    REQUIRE(c.IsValid());
    REQUIRE_EQ(c.mOffset, 24ULL);

    const auto d = ring.Allocate(8ULL, 4ULL, 4ULL);
    REQUIRE(!d.IsValid());

    ring.ReleaseUpTo(2ULL);
    REQUIRE_EQ(ring.GetTail(), 24ULL);

    const auto wrapped = ring.Allocate(8ULL, 4ULL, 4ULL);
    REQUIRE(wrapped.IsValid());
    REQUIRE_EQ(wrapped.mOffset, 0ULL);
    REQUIRE_EQ(wrapped.mSize, 8ULL);

    ring.ReleaseUpTo(4ULL);
    REQUIRE_EQ(ring.GetTail(), 8ULL);
}

TEST_CASE("AllocatorPolicy.Buddy.Coalesce") {
    FBuddyAllocatorPolicy buddy(1024ULL, 64ULL);

    const auto            a = buddy.Allocate(100ULL, 1ULL);
    REQUIRE(a.IsValid());
    REQUIRE_EQ(a.mOffset, 0ULL);
    REQUIRE_EQ(a.mSize, 128ULL);
    REQUIRE_EQ(a.mOrder, 1U);

    const auto b = buddy.Allocate(100ULL, 1ULL);
    REQUIRE(b.IsValid());
    REQUIRE_EQ(b.mOffset, 128ULL);
    REQUIRE_EQ(b.mSize, 128ULL);
    REQUIRE_EQ(b.mOrder, 1U);

    const auto c = buddy.Allocate(200ULL, 1ULL);
    REQUIRE(c.IsValid());
    REQUIRE_EQ(c.mOffset, 256ULL);
    REQUIRE_EQ(c.mSize, 256ULL);
    REQUIRE_EQ(c.mOrder, 2U);

    REQUIRE(buddy.Free(a));
    REQUIRE(buddy.Free(b));
    REQUIRE(buddy.Free(c));

    const auto merged = buddy.Allocate(800ULL, 1ULL);
    REQUIRE(merged.IsValid());
    REQUIRE_EQ(merged.mOffset, 0ULL);
    REQUIRE_EQ(merged.mSize, 1024ULL);
    REQUIRE_EQ(merged.mOrder, 4U);

    REQUIRE(buddy.Free(merged));

    const auto tooLarge = buddy.Allocate(2048ULL, 1ULL);
    REQUIRE(!tooLarge.IsValid());

    const auto aligned = buddy.Allocate(1ULL, 256ULL);
    REQUIRE(aligned.IsValid());
    REQUIRE_EQ(aligned.mOffset, 0ULL);
    REQUIRE_EQ(aligned.mSize, 256ULL);
    REQUIRE_EQ(aligned.mOrder, 2U);
}

TEST_CASE("AllocatorExecutor.MemoryBackingWrite") {
    u8                                                             buffer[64] = {};
    FMemoryBufferBacking                                           backing(buffer, 64ULL);
    TAllocatorExecutor<FRingAllocatorPolicy, FMemoryBufferBacking> executor(backing);
    executor.InitPolicyFromBacking();

    const auto alloc = executor.Allocate(16ULL, 4ULL, 1ULL);
    REQUIRE(alloc.IsValid());

    const u8 payload[4] = { 1, 2, 3, 4 };
    REQUIRE(executor.Write(alloc, payload, 4ULL, 4ULL));

    REQUIRE_EQ(buffer[alloc.mOffset + 4ULL], 1);
    REQUIRE_EQ(buffer[alloc.mOffset + 7ULL], 4);
}
