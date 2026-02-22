#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "../../../Public/Platform/Generic/GenericPlatformDecl.h"
#include <new>

using namespace AltinaEngine::Core::Platform::Generic;

extern "C" {

auto PlatformCreateCriticalSection() -> void* {
    auto* cs = static_cast<CRITICAL_SECTION*>(::operator new(sizeof(CRITICAL_SECTION)));
    ::InitializeCriticalSectionEx(cs, 4000, 0);
    return cs;
}

void PlatformDeleteCriticalSection(void* CS) {
    if (CS) {
        auto* cs = static_cast<CRITICAL_SECTION*>(CS);
        DeleteCriticalSection(cs);
        operator delete(cs);
    }
}

void PlatformEnterCriticalSection(void* CS) {
    auto* cs = static_cast<CRITICAL_SECTION*>(CS);
    EnterCriticalSection(cs);
}

auto PlatformTryEnterCriticalSection(void* CS) -> int {
    auto* cs = static_cast<CRITICAL_SECTION*>(CS);
    return TryEnterCriticalSection(cs) ? 1 : 0;
}

void PlatformLeaveCriticalSection(void* CS) {
    auto* cs = static_cast<CRITICAL_SECTION*>(CS);
    LeaveCriticalSection(cs);
}

auto PlatformCreateConditionVariable() -> void* {
    auto* cv = static_cast<CONDITION_VARIABLE*>(operator new(sizeof(CONDITION_VARIABLE)));
    InitializeConditionVariable(cv);
    return cv;
}

void PlatformDeleteConditionVariable(void* CV) {
    if (CV) {
        ::operator delete(CV);
    }
}

void PlatformWakeConditionVariable(void* CV) {
    auto* cv = static_cast<CONDITION_VARIABLE*>(CV);
    WakeConditionVariable(cv);
}

void PlatformWakeAllConditionVariable(void* CV) {
    auto* cv = static_cast<CONDITION_VARIABLE*>(CV);
    WakeAllConditionVariable(cv);
}

auto PlatformSleepConditionVariableCS(void* CV, void* CS, unsigned long Milliseconds) -> int {
    auto* cv = static_cast<CONDITION_VARIABLE*>(CV);
    auto* cs = static_cast<CRITICAL_SECTION*>(CS);
    BOOL  r  = ::SleepConditionVariableCS(cv, cs, Milliseconds);
    return r ? 1 : 0;
}

auto PlatformCreateEvent(int bManualReset, int bInitiallySignaled) -> void* {
    HANDLE h = ::CreateEventA(
        nullptr, bManualReset ? TRUE : FALSE, bInitiallySignaled ? TRUE : FALSE, nullptr);
    return static_cast<void*>(h);
}

void PlatformCloseEvent(void* Event) {
    if (Event)
        CloseHandle(Event);
}

void PlatformSetEvent(void* Event) {
    if (Event)
        SetEvent(Event);
}

void PlatformResetEvent(void* Event) {
    if (Event)
        ::ResetEvent(Event);
}

auto PlatformWaitForEvent(void* Event, unsigned long Milliseconds) -> int {
    if (!Event)
        return 0;
    DWORD r = ::WaitForSingleObject(Event, Milliseconds);
    return (r == WAIT_OBJECT_0) ? 1 : 0;
}

auto PlatformInterlockedCompareExchange32(
    volatile int32_t* ptr, int32_t exchange, int32_t comparand) -> int32_t {
    return ::InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(ptr), exchange, comparand);
}

auto PlatformInterlockedExchange32(volatile int32_t* ptr, int32_t value) -> int32_t {
    return ::InterlockedExchange(reinterpret_cast<volatile LONG*>(ptr), value);
}

auto PlatformInterlockedIncrement32(volatile int32_t* ptr) -> int32_t {
    return ::InterlockedIncrement(reinterpret_cast<volatile LONG*>(ptr));
}

auto PlatformInterlockedDecrement32(volatile int32_t* ptr) -> int32_t {
    return ::InterlockedDecrement(reinterpret_cast<volatile LONG*>(ptr));
}

auto PlatformInterlockedExchangeAdd32(volatile int32_t* ptr, int32_t add) -> int32_t {
    return ::InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(ptr), add);
}

auto PlatformInterlockedCompareExchange64(
    volatile int64_t* ptr, int64_t exchange, int64_t comparand) -> int64_t {
    return ::InterlockedCompareExchange64(
        reinterpret_cast<volatile LONGLONG*>(ptr), exchange, comparand);
}

auto PlatformInterlockedExchange64(volatile int64_t* ptr, int64_t value) -> int64_t {
    return ::InterlockedExchange64(reinterpret_cast<volatile LONGLONG*>(ptr), value);
}

auto PlatformInterlockedIncrement64(volatile int64_t* ptr) -> int64_t {
    return ::InterlockedIncrement64(reinterpret_cast<volatile LONGLONG*>(ptr));
}

auto PlatformInterlockedDecrement64(volatile int64_t* ptr) -> int64_t {
    return ::InterlockedDecrement64(reinterpret_cast<volatile LONGLONG*>(ptr));
}

auto PlatformInterlockedExchangeAdd64(volatile int64_t* ptr, int64_t add) -> int64_t {
    return ::InterlockedExchangeAdd64(reinterpret_cast<volatile LONGLONG*>(ptr), add);
}

} // extern "C"

namespace AltinaEngine::Core::Platform::Generic {

    void PlatformSleepMilliseconds(unsigned long Milliseconds) { ::Sleep(Milliseconds); }

} // namespace AltinaEngine::Core::Platform::Generic
