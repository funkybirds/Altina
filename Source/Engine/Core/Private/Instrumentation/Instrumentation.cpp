#include "../../Public/Instrumentation/Instrumentation.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Threading/Atomic.h"
#include "../../Public/Container/HashMap.h"
#include "../../Public/Container/String.h"
#include "../../Public/Container/SmartPtr.h"
#include <chrono>

using namespace AltinaEngine;
using namespace AltinaEngine::Core;

namespace AltinaEngine::Core::Instrumentation
{
    using Container::MakeShared;
    using Container::THashMap;
    using Container::TShared;
    using std::string;
    using Threading::TAtomic;

    struct FCounter
    {
        Threading::TAtomic<long long> mValue{ 0 };
    };

    struct FTiming
    {
        Threading::TAtomic<unsigned long long> mTotalMs{ 0 };
        Threading::TAtomic<unsigned long long> mCount{ 0 };
    };

    static Threading::FMutex                   gMutex;
    static THashMap<string, TShared<FCounter>> gCounters;
    static THashMap<string, TShared<FTiming>>  gTimings;

    // Per-thread name stored in thread_local for fast reads.
    thread_local const char*                   tThreadName = nullptr;

    void                                       SetCurrentThreadName(const char* name) noexcept
    {
        tThreadName = name;
        if (!name)
            return;

        // Ensure a placeholder counter entry exists for visibility tools.
        Threading::FScopedLock lk(gMutex);
        string                 key(name);
        if (gCounters.find(key) == gCounters.end())
        {
            gCounters.emplace(AltinaEngine::Move(key), MakeShared<FCounter>());
        }
    }

    const char* GetCurrentThreadName() noexcept { return tThreadName ? tThreadName : ""; }

    void        IncrementCounter(const char* name, long long delta) noexcept
    {
        if (!name)
            return;
        Threading::FScopedLock lk(gMutex);
        string                 key(name);
        auto                   it = gCounters.find(key);
        if (it == gCounters.end())
        {
            auto shared = MakeShared<FCounter>();
            shared->mValue.FetchAdd(static_cast<long long>(delta));
            gCounters.emplace(AltinaEngine::Move(key), shared);
            return;
        }
        it->second->mValue.FetchAdd(static_cast<long long>(delta));
    }

    long long GetCounterValue(const char* name) noexcept
    {
        if (!name)
            return 0;
        Threading::FScopedLock lk(gMutex);
        string                 key(name);
        auto                   it = gCounters.find(key);
        if (it == gCounters.end())
            return 0;
        return it->second->mValue.Load();
    }

    void RecordTimingMs(const char* name, unsigned long long ms) noexcept
    {
        if (!name)
            return;
        Threading::FScopedLock lk(gMutex);
        string                 key(name);
        auto                   it = gTimings.find(key);
        if (it == gTimings.end())
        {
            auto shared = MakeShared<FTiming>();
            shared->mTotalMs.FetchAdd(ms);
            shared->mCount.FetchAdd(1);
            gTimings.emplace(AltinaEngine::Move(key), shared);
            return;
        }
        it->second->mTotalMs.FetchAdd(ms);
        it->second->mCount.FetchAdd(1);
    }

    void GetTimingAggregate(const char* name, unsigned long long& outTotalMs, unsigned long long& outCount) noexcept
    {
        outTotalMs = 0ULL;
        outCount   = 0ULL;
        if (!name)
            return;
        Threading::FScopedLock lk(gMutex);
        string                 key(name);
        auto                   it = gTimings.find(key);
        if (it == gTimings.end())
            return;
        outTotalMs = it->second->mTotalMs.Load();
        outCount   = it->second->mCount.Load();
    }

    // Simple clock helper (ms)
    static auto NowMs() -> unsigned long long
    {
        using namespace std::chrono;
        auto now = steady_clock::now();
        return static_cast<unsigned long long>(duration_cast<milliseconds>(now.time_since_epoch()).count());
    }

    FScopedTimer::FScopedTimer(const char* name) noexcept : mName(name), mStartMs(NowMs()) {}

    FScopedTimer::~FScopedTimer() noexcept
    {
        if (!mName)
            return;
        const auto elapsed = NowMs() - mStartMs;
        RecordTimingMs(mName, elapsed);
    }

} // namespace AltinaEngine::Core::Instrumentation
