# AltinaEngine TODO

- Note: Do not use any `std` headers. Use rust-like type aliases, like `u8` (already defined).
  - Follow `CodingStyle.md` for naming conventions and file structure.
      - Use trailing return types ALWAYS!
      - `constexpr` and `[[nodiscard]]` as much as possible
      - DO NOT USE ANY `try catch` and `exceptions`
      - Do not use redundant namespace, following is not recommended
          - Do not use`AltinaEngine::Core::Reflection::FObject`, use `using namespace` in `xxx::Detail` or
            `using xxx::FObject` instead
          - Do not use `Altina::TDecay<T>`, directly use `TDecay<T>`
          - Do not use `Altina::f32` or `float`, use `f32` directly
      - Do not use `std::vector<T>/std::unordered_map<K,V>/std::string` etc., use `TVector<T>/THashMap<K,V>/FString`
        instead
        - Do not use functions derived from std containers (like `.data()` and `.size()`). Use `.Data()` and `.Size()` instead.
          - If the required method is missing, create a wrapper in the container.
      - Do not use `if constexpr (Traits<T>::Value)`, use `if constexpr (CTrait<T>)` instead
        - When creating new traits, PREFER `concept` to `type trait`
      - Use log as possible as 

## Rhi.General 
- Note: This is an independent compilation target (`Rhi.General`, not `Rhi`)
- [ ] Draft (NOT CODE) a architecture for RHI abstraction, targeting legacy apis like `DX11`, `OpenGL` and modern apis like `DX12`,`Vulkan`. And fulfill the `Rhi.General` section with the architectural plan (DO NOT REMOVE THIS POINT).
  - [ ] `RhiContext` should be interface abstraction for rhi utilities, like device cration, swapchain preparation, resource creation, etc.
  - [ ] `RhiDevice` should be logical device abstraction (VkDevice&ID3D12Device). It should provides interfaces for device feature/caps querying (like `support bindless`,`support hwrt`,`support multithreaded command recording`)
    - [ ] For limits, `RhiSupportedLimits` and `RhiSupportedFeatures` can be defined.
    - [ ] `RhiQueue` should be defined. Although some apis does not support this concept explicitly.
  - [ ] `RhiResource` and `RhiMemoryResource`. Abstraction for buffer, texture, shader, acceleration structures, samplers, pipelines or pso (graphics,compute,hwrt)
    - [ ] some apis might have abstraction for `RhiAllocator` and `RhiMemory`. Keep these in `Rhi` scope, and do not expose to other modules.
    - [ ] shader format should be defined. Unity-like `shaderlab` is favored. and `slang` is an option for shader lang and corresponding compiler. However, `slang` does not directly support shaderlab. Processing scripts should be planned.
      - [ ] Also consider the portability for low-end and high-end devices, which have bindless support.
      - [ ] Shader compilation and parsing should be done in `ShaderCompile` scope and not in `Rhi` scope.
    - [ ] In `Core` scope, consider a container `TCountedRef<T>` and concept. which like (but not is) `TSharedPtr<T>`, and have custom deallocation function.
  - [ ] `RhiResourceBinding` and `RhiResourceView`. Abstraction for `UAV`, `CBV` and `SRV`, etc. Use `DirectX` standard here.
    - [ ] Refer to `webgpu` for `RhiResourceBinding` definitions.
  - [ ] `RhiCommandBuffer` and `RhiCommandEncoder`. 
    - [ ] Support for multithreaded rendering (if able). Should support async compute and transfer.
    - [ ] Make sure design pattern choice for command encoders. Keeps idea and code simple. (A) encoder.transistion(RhiTransition{}) (B) transition.cmdTransition(encoder) (C) 
    - [ ] Consider the design for sync primitives, like `RhiSemaphore` and `RhiFence`. Make proper management for these stuffs.
    - [ ] Stateless!. `RenderGraph` is not a part of `Rhi`. So introduce utilties for explicit state transition.
  - [ ] `RhiQuery`
    - [ ] Provide utilities/debug markers.
  - [ ] `RhiPipeline` and `RhiPipelineLayout`.
    - [ ] Abstraction for `RootSignature` (DX12) / `PipelineLayout` (Vulkan). define the interface between resources and shaders.
    - [ ] Pipeline State Object (PSO) creation is heavy (compile+link). Plan for `RhiPipelineCache` to support disk caching and runtime deduplication.
    - [ ] Distinguish between `GraphicsPipeline`, `ComputePipeline` and `RayTracingPipeline`.
  - [ ] `RhiRenderPassInfo`.
    - [ ] Even with Dynamic Rendering features, an structure for describing RenderTargets, DepthStencil, Load/Store ops and ClearValues is required for `BeginRendering`.
  - [ ] `RhiSampler`. Independent object for texture filtering and addressing modes.
  - [ ] `RhiTransientResource` and `Staging`.
    - [ ] Need a strategy for per-frame dynamic data (e.g. `RingBuffer` or `LinearAllocator` for dynamic constant buffers).
    - [ ] Staging abstraction for efficient CPU-to-GPU data transfer (Upload Heaps).
  - [ ] `RhiIndirectCommand`.
    - [ ] Abstraction for `CommandSignature` to support GPU-Driven rendering (`ExecuteIndirect`).

## Rhi.DirectX11

## RenderCore.RenderGraph

## Core.Reflection
- [x] Reflection Support
    - [x] Add `FPropertyDesc` struct with `FString mName` and `FObject mProperty` member.
    - [x] Add `GetAllProperties(FObject&) -> TVector<FPropertyDesc>` function.
- [x] Serialization Support
    - [x] Basic support for `FObject` Serialization
        - [x] Add `SerializeInvoker<T>(T&,ISerializer&)->void` template in `Core/Public/Reflection/Serialization.h`
            - [x] Add requirements for this template: `CDefinedType<T>` and `!CPointer<T>`
                - [x] Add `CPointer<T>` concept in `Core/Public/Reflection/Concepts.h`
            - [x] If `T` is `CTriviallySerializable<T>` directly call `serializer.Write(t)`
            - [x] If `T` is `CCustomInternalSerializable<T>` call `t.Serialize(serializer)`
            - [x] If `T` is `CCustomExternalSerializable<T>` call
              `TCustomSerializeRule<T>::Serialize(t.As<T>(),serializer)`
            - [x] Otherwise, call `DynamicSerializeInvoker<T>(T&,ISerializer&)`
                - [x] It calls api call `DynamicSerializeInvokerImpl(t,serializer,FTypeMetaInfo::Create<T>().GetHash())`
                    - The function def is `DynamicSerializeInvokerImpl(void*,ISerializer&,u64 hash)->void`
                    - The logic for `DynamicSerializeInvokerImpl` should be as follows:
                        - [x] If hash is not registered, trigger a reflection error using `ReflectionAssert`
                        - [x] If hash is registered, do following:
                            - [x] Construct `FObject` use `void*` and hash. Inspect `FObject`'s constructor first
                            - [x] Begin object twice, then write field `AE_REFLHASH` with type hash. End Object ONCE!
                            - [x] Call `GetAllProperties()`, for each property do:
                                - [x] Begin object with property name
                                - [x] Call `v.Serialize(serializer)`. (`FObject::Serialize` will be added later)
                                - [x] End object
                            - [x] End object
    - [x] Add concept `CSerializable<T>`. If `SerializeInvoker<T>` can be instantiated for `T`, then
      `CSerializable<T>` is satisfied.
    - [x] Add `Serialize(ISerializer&)` interface for `FObject`.
        - [x] In `FTypeMetaInfo`, add `mSerializable` property
        - [x] In `FTypeMetaInfo`, add `mSerializeRule` property, with type `void (*)(void*)`
            - [x] this is a lambda that calls `SerializeInvoker<T>` internally
        - [x] This function `Serialize(ISerializer&)` calls metadata's `mSerializeRule`
    - [x] Implement basic binary serializer which inherits `ISerializer`. The serializer should be under
      `Core/Reflection/SerializerImpl`. Make sure that private functions should be in `Private` folder. Do not use
      any
      `std` headers. Use rust-like type aliases, like `u8` (already defined).
        - [x] Implement serialization for primitive types: `bool`, `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`,
          `u64`,
          `f32`, `f64`. (DO NOT CONSIDER STRING/STRINGVIEW NOW)
        - [x] Remove interfaces for `FObject`.
    - [x] Basic support for `FObject` Deserialization
        - [x] Add `DeserializeInvokerImpl<T>(T*,IDeserializer&)->void` template in
          `Core/Public/Reflection/Serialization.h`. Implementation logic is similar to `SerializeInvoker<T>`, but in
          reverse
        - [x] Add `DeserializeInvoker<T>(IDeserializer&)->T` template in
          `Core/Public/Reflection/Serialization.h`. This is the wrapper for `DeserializeInvokerImpl<T>`
    - [x] Add concept `CDeserializable<T>`. If `DeserializeInvoker<T>` can be instantiated for `T`, then
      `CDeserializable<T>` is satisfied.
    - [x] Add `Deserialize(ISerializer&)` interface for `FObject`.
        - [x] In `FTypeMetaInfo`, add `mDeserializable` property
        - [x] In `FTypeMetaInfo`, add `mDeserializeRule` property, with type `void (*)(void*)`
            - [x] this is a lambda that calls `SerializeInvoker<T>` internally
        - [x] This function `Serialize(ISerializer&)` calls metadata's `mSerializeRule`
    - [x] Add basic binary deserializer which inherits `IDeserializer`. The deserializer should be under
      `Core/Reflection/SerializerImpl`.
    - [x] Add unit tests for serialization and deserialization under `Source/Tests/Core/Reflection/`
        - [x] Move existing tests for `Reflection` from `Source/Test/Reflection/` to `Source/Tests/Core/Reflection/`
        - [x] Add tests for serialization and deserialization of primitive types
        - [x] Add tests for serialization and deserialization of user-defined types with INTERNAL serialization rules
        - [x] Add tests for serialization and deserialization of user-defined types with EXTERNAL serialization rules

## Core.Threading / Core.Jobs

### Job System Foundations

- [x] Draft `Source/Engine/Core/Public/Jobs/JobSystem.h` covering job submission, job handles, fences, and dependency
  nodes.
    - [x] Enumerate minimal forward declarations and include order for `Jobs` public headers.
    - [x] Define `JobHandle`, `JobFence`, and `DependencyNode` interfaces with API docs.
    - [x] Add configuration structs (worker counts, affinity tags) referenced by consumers.
- [ ] Create `Source/Engine/Core/Private/Jobs/JobSystem.cpp` with queue plumbing, dependency evaluation, and hooks for
  future scheduler backends.
    - [x] Implement core runtime (`JobSystem.cpp`) with worker pool, job queueing, delayed jobs, and dependency
      emission.
    - [x] Stand up basic job queue storage using `TThreadSafeQueue` with delayed-job handling.
    - [x] Implement dependency emission that produces `JobHandle` results and waits on prerequisites.
    - [ ] Insert stub interfaces for platform/architecture-specific scheduler backends (TODO).
- [ ] Add `JobDescriptor` and `JobDependency` types plus documentation in `docs/ModuleContracts.md` describing lifetime
  and threading guarantees.
    - [ ] Define struct fields (callback, payload pointer, debug label, affinity mask).
    - [x] Define `JobDescriptor` (callback, payload pointer, debug label, affinity mask, priority).
    - [ ] Document ownership rules and payload lifetime in `ModuleContracts` (TODO).
    - [ ] Provide example snippets showing descriptor construction for named threads vs generic workers (TODO).
- [ ] Implement a lightweight `JobGraphBuilder` utility to batch job creation, attach dependencies, and emit execution
  plans.
    - [ ] Design fluent API for adding jobs, dependencies, and metadata labels.
    - [ ] Validate emission path (topological ordering + cycle detection) before handing to runtime queue.
    - [ ] Add debug dump that prints the graph for troubleshooting.
- [ ] Support dependency graph resolution so jobs/named-thread tasks can reference `JobHandle`/`ThreadTaskHandle`
  prerequisites before dispatch.
    - [ ] Introduce `WaitForHandles(...)` entry points that block or enqueue continuations.
    - [x] Provide `JobSystem::Wait(JobHandle)` and `JobFence` APIs to block until completion.
    - [x] Scheduler tracks completion state for handles via `JobState` storage.
    - [ ] Add validation that cross-thread dependencies do not deadlock (e.g., named thread waiting on itself) (TODO).

### Threading Infrastructure

- [x] Introduce `Engine/Core` threading primitives (mutex, event, condition variable wrappers) that abstract platform
  specifics.
    - [x] Wrap Win32/POSIX primitives behind unified API with RAII helpers.
    - [x] Add unit tests checking lock recursion rules and timeout semantics.
    - [ ] Document cost/behavior tradeoffs for each primitive in headers.
- [ ] Build a configurable worker thread pool (min/max threads, stealability toggles) consuming the job queues.
    - [ ] Parse configuration data (CVar, preset, config file) at startup.
    - [x] Implement `FWorkerPool` with `Start()`/`Stop()` and basic lifecycle management.
    - [ ] Parse configuration data (CVar, preset, config file) at startup (TODO).
    - [ ] Add optional work-stealing deque implementation with telemetry toggles (TODO).
- [x] Provide instrumentation hooks (per-thread names, counters, timing) to integrate with future profiling tools.
    - [ ] Emit tracing events on job enqueue/dequeue/complete.
    - [x] Surface per-thread counters accessible via debug console/API.
    - [ ] Integrate with existing logging/assert systems for anomalies.
- [ ] Define named thread descriptors for `RHI`, `Rendering`, `Gameplay`, and `Audio` including their startup order and
  run-loop responsibilities.
    - [ ] Capture initialization contracts per named thread (modules they bootstrap, required services).
    - [ ] Specify run-loop pseudo code documenting how they drain queues vs perform frame tasks.
    - [ ] Store descriptor metadata in a sharable manifest for debug tools.
- [ ] Expose APIs allowing named threads to register internal helper threads (e.g., `RHI` command submission, async
  uploads) with lifecycle tracking and visibility to profilers.
    - [ ] Define helper-thread registration structure (name, owning thread, purpose).
    - [ ] Track helper thread states (idle, busy) and expose metrics.
    - [ ] Ensure helper threads inherit affinity + logging configuration from parents.
- [ ] Ensure the job system can target named threads/pools via affinity + priority tags so module jobs land on the
  correct execution context.
    - [ ] Add affinity mask encoding that maps to named thread IDs and worker pools.
    - [ ] Implement priority buckets in queues to avoid starvation of critical tasks.
    - [ ] Provide validation helpers to catch invalid affinity combinations in debug builds.
- [ ] Add watchdog/logging support that surfaces when named threads stall or internal helper threads are not consuming
  work.
    - [ ] Track per-thread heartbeat timestamps and expose thresholds for alerts.
    - [ ] Emit structured logs when watchdog triggers and include callstack capture hooks.
    - [ ] Integrate with future telemetry (e.g., editor overlay) to highlight stalled threads.
- [x] Stand up interim thread-safe `TQueue`/`TStack` wrappers using `FScopedLock` until lock-free containers arrive.
    - [x] Create `FThreadSafeQueue`/`FThreadSafeStack` templates that wrap enqueue/dequeue/push/pop with scoped locks.
    - [x] Add unit tests proving basic correctness under concurrent producers/consumers.
    - [x] Document limitations (blocking behavior, contention) and plan follow-up work for true lock-free structures.

### Integration Tasks

- [ ] Expose configuration knobs via `Engine/Application` plus preset files to set worker counts, named thread
  enablement, and helper thread limits.
    - [ ] Extend application config schema/CLI flags for job + thread settings.
    - [ ] Ensure presets map to sane platform defaults (console vs desktop vs mobile).
    - [ ] Add runtime console commands to inspect/adjust counts for debugging.
- [ ] Integrate job-system startup/shutdown sequencing into `Engine/Application` so named threads register before
  subsystems boot.
    - [ ] Define startup ordering dependencies (e.g., Core -> Job System -> Named Threads -> Subsystems).
    - [ ] Ensure graceful shutdown drains queues, joins helper threads, and reports unresolved jobs.
    - [ ] Add startup/shutdown tracing to verify order in automated tests.
- [ ] Write unit tests under `Source/Tests/Core/` validating dependency resolution, affinity routing, and named-thread
  submission.
    - [ ] Add synthetic workload tests that schedule jobs with dependency chains and assert ordering.
    - [ ] Test affinity routing by forcing jobs to specific named threads and confirming execution context.
    - [ ] Simulate failure/timeout cases to ensure diagnostics fire.
- [ ] Add smoke tests that spin up RHI/Rendering/Gaming/Audio named threads and verify helper thread registration.
    - [ ] Build harness that boots a miniature application loop with all named threads active.
    - [ ] Validate helper thread registration tables and telemetry outputs.
    - [ ] Capture timing metrics to ensure startup/shutdown budgets are met.
- [ ] Update `docs/CodingStyle.md` with guidelines for submitting work to the job system, declaring dependencies, and
  ensuring thread safety.
    - [ ] Document best practices for job granularity, affinity usage, and dependency chains.
    - [ ] Add examples covering named-thread submissions and helper thread creation.
    - [ ] Highlight anti-patterns (blocking on named threads, long-lived locks) and mitigation strategies.
