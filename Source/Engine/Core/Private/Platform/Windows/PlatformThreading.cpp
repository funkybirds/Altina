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

void* PlatformCreateCriticalSection() {
    CRITICAL_SECTION* cs = static_cast<CRITICAL_SECTION*>(::operator new(sizeof(CRITICAL_SECTION)));
    ::InitializeCriticalSectionEx(cs, 4000, 0);
    return cs;
}

void PlatformDeleteCriticalSection(void* CS) {
    if (CS) {
        CRITICAL_SECTION* cs = static_cast<CRITICAL_SECTION*>(CS);
        ::DeleteCriticalSection(cs);
        ::operator delete(cs);
    }
}

void PlatformEnterCriticalSection(void* CS) {
    CRITICAL_SECTION* cs = static_cast<CRITICAL_SECTION*>(CS);
    ::EnterCriticalSection(cs);
}

int PlatformTryEnterCriticalSection(void* CS) {
    CRITICAL_SECTION* cs = static_cast<CRITICAL_SECTION*>(CS);
    return ::TryEnterCriticalSection(cs) ? 1 : 0;
}

void PlatformLeaveCriticalSection(void* CS) {
    CRITICAL_SECTION* cs = static_cast<CRITICAL_SECTION*>(CS);
    ::LeaveCriticalSection(cs);
}

void* PlatformCreateConditionVariable() {
    CONDITION_VARIABLE* cv = static_cast<CONDITION_VARIABLE*>(::operator new(sizeof(CONDITION_VARIABLE)));
    ::InitializeConditionVariable(cv);
    return cv;
}

void PlatformDeleteConditionVariable(void* CV) {
    if (CV) {
        ::operator delete(CV);
    }
}

void PlatformWakeConditionVariable(void* CV) {
    CONDITION_VARIABLE* cv = static_cast<CONDITION_VARIABLE*>(CV);
    ::WakeConditionVariable(cv);
}

void PlatformWakeAllConditionVariable(void* CV) {
    CONDITION_VARIABLE* cv = static_cast<CONDITION_VARIABLE*>(CV);
    ::WakeAllConditionVariable(cv);
}

int PlatformSleepConditionVariableCS(void* CV, void* CS, unsigned long Milliseconds) {
    CONDITION_VARIABLE* cv = static_cast<CONDITION_VARIABLE*>(CV);
    CRITICAL_SECTION* cs = static_cast<CRITICAL_SECTION*>(CS);
    BOOL r = ::SleepConditionVariableCS(cv, cs, Milliseconds);
    return r ? 1 : 0;
}

void* PlatformCreateEvent(int bManualReset, int bInitiallySignaled) {
    HANDLE h = ::CreateEventA(nullptr, bManualReset ? TRUE : FALSE, bInitiallySignaled ? TRUE : FALSE, nullptr);
    return static_cast<void*>(h);
}

void PlatformCloseEvent(void* Event) {
    if (Event) ::CloseHandle(static_cast<HANDLE>(Event));
}

void PlatformSetEvent(void* Event) {
    if (Event) ::SetEvent(static_cast<HANDLE>(Event));
}

void PlatformResetEvent(void* Event) {
    if (Event) ::ResetEvent(static_cast<HANDLE>(Event));
}

int PlatformWaitForEvent(void* Event, unsigned long Milliseconds) {
    if (!Event) return 0;
    DWORD r = ::WaitForSingleObject(static_cast<HANDLE>(Event), Milliseconds);
    return (r == WAIT_OBJECT_0) ? 1 : 0;
}

int32_t PlatformInterlockedCompareExchange32(volatile int32_t* ptr, int32_t exchange, int32_t comparand) {
    return ::InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(ptr), exchange, comparand);
}

int32_t PlatformInterlockedExchange32(volatile int32_t* ptr, int32_t value) {
    return ::InterlockedExchange(reinterpret_cast<volatile LONG*>(ptr), value);
}

int32_t PlatformInterlockedIncrement32(volatile int32_t* ptr) {
    return ::InterlockedIncrement(reinterpret_cast<volatile LONG*>(ptr));
}

int32_t PlatformInterlockedDecrement32(volatile int32_t* ptr) {
    return ::InterlockedDecrement(reinterpret_cast<volatile LONG*>(ptr));
}

int32_t PlatformInterlockedExchangeAdd32(volatile int32_t* ptr, int32_t add) {
    return ::InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(ptr), add);
}

int64_t PlatformInterlockedCompareExchange64(volatile int64_t* ptr, int64_t exchange, int64_t comparand) {
    return ::InterlockedCompareExchange64(reinterpret_cast<volatile LONGLONG*>(ptr), exchange, comparand);
}

int64_t PlatformInterlockedExchange64(volatile int64_t* ptr, int64_t value) {
    return ::InterlockedExchange64(reinterpret_cast<volatile LONGLONG*>(ptr), value);
}

int64_t PlatformInterlockedIncrement64(volatile int64_t* ptr) {
    return ::InterlockedIncrement64(reinterpret_cast<volatile LONGLONG*>(ptr));
}

int64_t PlatformInterlockedDecrement64(volatile int64_t* ptr) {
    return ::InterlockedDecrement64(reinterpret_cast<volatile LONGLONG*>(ptr));
}

int64_t PlatformInterlockedExchangeAdd64(volatile int64_t* ptr, int64_t add) {
    return ::InterlockedExchangeAdd64(reinterpret_cast<volatile LONGLONG*>(ptr), add);
}

} // extern "C"

namespace AltinaEngine::Core::Platform::Generic {

void PlatformSleepMilliseconds(unsigned long Milliseconds) {
    ::Sleep(Milliseconds);
}

} // namespace AltinaEngine::Core::Platform::Generic
