# Altina RHI Architecture Draft

This document outlines the architectural design for the Render Hardware Interface (RHI) module of AltinaEngine. The goal is to provide a low-level, zero-overhead abstraction over modern APIs (DirectX 12, Vulkan) while maintaining compatibility paths for legacy APIs (DirectX 11, OpenGL) where feasible, though the design prioritizes modern explicit control.

## Design Philosophy
- **Stateless**: The RHI itself does not track global state (binding slots, current pipeline) implicitly. All state changes must be recorded into Command Lists/Buffers explicitly.
- **Explicit Synchronization**: Barriers, fences, and semaphores are exposed to the upper layers (RenderGraph).
- **Thread Safety**: Command list recording must be free-threaded.
- **Rust-like Primitives**: Use `TUniquePtr`, `TSharedPtr` (or `TCountedRef`), and avoid STL containers in favor of engine `TVector`, independent compilation target `Rhi.General`.

---

## 1. RhiContext & Adapter
**Role**: Entry point for the RHI module. Handles instance creation, debugging layers, and physical device (adapter) enumeration.

### Core Concepts
- **RhiInstance**: Represents the API instance (VkInstance / IDXGIFactory).
- **RhiAdapter**: Represents a physical GPU.

### Core Interfaces (`RhiContext`)
- `Init(const RhiInitDesc& desc) -> bool`
  - Initializes the backend (Load DLLs, create Instance).
- `EnumerateAdapters() -> TVector<RhiAdapterDesc>`
  - Returns list of available GPUs with basic info (VRAM, Vendor ID).
- `CreateDevice(u32 adapterIndex) -> TSharedPtr<RhiDevice>`
  - Creates the logical device.

---

## 2. RhiDevice
**Role**: The logical device (VkDevice / ID3D12Device). It is the factory for all GPU resources and the owner of command queues.

### Core Concepts
- **Capabilities**: `RhiSupportedFeatures` struct (Bindless, RayTracing, MeshShaders, Barycentrics).
- **Limits**: `RhiSupportedLimits` struct (MaxTextureSize, MaxUAVs, etc).

### Core Interfaces
- **Queues**: `GetQueue(ERhiQueueType type) -> TRhiSharedPtr<RhiQueue>`
- **Resource Creation**:
  - `CreateBuffer(const RhiBufferDesc& desc) -> TRhiSharedPtr<RhiBuffer>`
  - `CreateTexture(const RhiTextureDesc& desc) -> TRhiSharedPtr<RhiTexture>`
  - `CreateSampler(const RhiSamplerDesc& desc) -> TRhiSharedPtr<RhiSampler>`
  - `CreateShader(const RhiShaderDesc& desc) -> TRhiSharedPtr<RhiShader>`
- **Pipeline Creation**:
  - `CreateGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) -> TRhiSharedPtr<RhiPipeline>`
  - `CreateComputePipeline(const RhiComputePipelineDesc& desc) -> TRhiSharedPtr<RhiPipeline>`
  - `CreatePipelineLayout(const RhiPipelineLayoutDesc& desc) -> TRhiSharedPtr<RhiPipelineLayout>`
- **Binding**:
  - `CreateBindGroupLayout(const RhiBindGroupLayoutDesc& desc) -> TRhiSharedPtr<RhiBindGroupLayout>`
  - `CreateBindGroup(const RhiBindGroupDesc& desc) -> TRhiSharedPtr<RhiBindGroup>` (or DescriptorSet)
- **Synchronization**:
  - `CreateFence(bool signaled) -> TRhiSharedPtr<RhiFence>`
  - `CreateSemaphore() -> TRhiSharedPtr<RhiSemaphore>`
- **Command**:
  - `CreateCommandPool(const RhiCommandPoolDesc& desc) -> TRhiSharedPtr<RhiCommandPool>`

---

## 3. RhiQueue
**Role**: Abstraction of hardware queues (Graphics, Compute, Copy/Transfer).

### Core Interfaces
- `Submit(const RhiSubmitInfo& info)`
  - `info` contains: `CommandBuffers`, `WaitSemaphores`, `SignalSemaphores`, `SignalFence`.
- `WaitIdle()`
- `Present(const RhiPresentInfo& info)` (For Graphics Queues connected to Swapchain)

---

## 4. RhiResource & Memory
**Role**: Abstraction for GPU resources.

### 4.1. RhiMemory
- **Role**: Concept for device memory allocations.
- **Strategy**: RHI should expose an allocator interface, but user generally interacts with Resources.
- **RhiHeap**: Abstraction for `ID3D12Heap` or `VkDeviceMemory` blocks for placed resources (aliasing).

### 4.2. RhiBuffer
- **Role**: Vertex, Index, Constant, Structured, Raw buffers.
- **Desc**: Size, Usage (TransferSrc, TransferDst, Uniform, Storage, Index, Vertex), CPU access flags.

### 4.3. RhiTexture
- **Role**: 1D/2D/3D textures, CubeMaps, Arrays.
- **Desc**: Format, Extent, MipLevels, ArrayLayers, SampleCount, Usage (RTV, DSV, SRV, UAV).

### 4.4. RhiSampler
- **Role**: Texture sampling state (Filter, AddressMode, MipLODBias, Anisotropy).
- **Core Interface**: Immutable object, created via Device.

---

## 5. RhiResourceBinding
**Role**: How resources are exposed to Shaders.

### 5.1. Resource Views
- **Role**:  Describe how to interpret memory.
- **Types**:
  - `RhiShaderResourceView (SRV)`
  - `RhiUnorderedAccessView (UAV)`
  - `RhiRenderTargetView (RTV)`
  - `RhiDepthStencilView (DSV)`
  - `RhiConstantBufferView (CBV)`
- **Note**: In Bindless workflows, these might just return a descriptor index (u32).

### 5.2. RhiBindGroup (or DescriptorSet)
- **Role**: A collection of resources bound together. Corresponds to `VkDescriptorSet` or a D3D12 Descriptor Table.
- **Creation**: Created from a `RhiBindGroupLayout`.
- **Interface**:
  - `Update(const RhiBindGroupUpdateInfo& updateInfo)`: Sets actual views into the group.

---

## 6. RhiPipeline & Layout
**Role**: Defines the GPU programming state.

### 6.1. RhiPipelineLayout (RootSignature)
- **Role**: The "Interface Definition" or "Menu". Defines signature of inputs (BindGroups, Root Constants).
- **Structure**: List of `RhiBindGroupLayout`s and `PushConstants`/`RootConstants` ranges.

### 6.2. RhiPipeline
- **Role**: The "Compiled Program". PSO (Pipeline State Object).
- **Variants**: Graphics, Compute, RayTracing.
- **Caching**: `RhiPipelineCache` interface to save/load PSOs blob to disk.
- **Desc (Graphics)**:
  - Shaders (VS, PS, etc.)
  - Blend State
  - Rasterizer State
  - DepthStencil State
  - Input Layout (Vertex Attributes) - Optional if using vertex pulling.
  - Primitive Topology
  - **PipelineLayout Reference**
  - RenderTarget Formats (for Dynamic Rendering)

---

## 7. RhiCommand (Encoder & Buffer)
**Role**: Recording commands.

### 7.1. RhiCommandPool
- **Role**: Allocator for command buffers. Thread-local usually.

### 7.2. RhiCommandBuffer
- **Role**: A recorded list of commands ready for submission.

### 7.3. RhiCommandEncoder (Concept)
- **Role**: The interface used to write into a CommandBuffer.
- **Design Choice**: "Encoder" pattern (WebGPU/Metal style) to prevent state pollution.
- **Interfaces**:
  - `Begin() / End()`
  - `SetPipeline(RhiPipeline*)`
  - `SetBindGroup(u32 setIndex, RhiBindGroup* group)`
  - `SetVertexBuffers(...)`, `SetIndexBuffer(...)`
  - `SetScissor()`, `SetViewport()`
  - `DrawInstanced(...)`, `DrawIndexedInstanced(...)`
  - `Dispatch(...)`
  - `ResourceBarrier(const RhiBarrierBatch& barriers)`: Explicit split barriers (Transition, UAV, Aliasing).
  - `BeginRendering(const RhiRenderPassInfo& info)` (Dynamic Rendering).
  - `EndRendering()`
  - `ExecuteIndirect(RhiCommandSignature*, ...)`

---

## 8. RhiSynchronization
**Role**: Validating GPU timeline dependencies.

- **RhiFence**: CPU-GPU sync. (e.g., Frame buffering).
  - `GetStatus()`, `Reset()`, `Signal()`, `Wait()`.
- **RhiSemaphore**: GPU-GPU sync (Queue ownership transfer, Queue dependency). (Timeline Semaphores preferred).

---

## 9. RhiTransientResource (Staging)
**Role**: Handling per-frame dynamic data.

- **Objective**: Efficiently upload `ViewProjection` matrices, dynamic vertex data, or UI instance data without creating persistent buffers every frame.
- **RhiLinearAllocator / RingBuffer**:
  - Allocates from a large pre-mapped Upload Heap.
  - `Allocate(u32 size, u32 alignment) -> AllocationInfo (GPUAddress, CPUAddress, Offset, BufferResource)`
- **UploadContext**:
  - Helper to queue upload commands (CopyBuffer, CopyTexture) and handle the lifetime of staging buffers (recycle after fence signal).

---

## 10. RhiIndirectCommand
**Role**: GPU-Driven Rendering support.

- **RhiCommandSignature**: Defines the format of the indirect arguments buffer (e.g., [DrawIndexed, Constant, View]).
- **Usage**: Used in `ExecuteIndirect`.

---

## 11. RhiRenderPassInfo
**Role**: Description of a rendering pass (Load/Store ops) for Dynamic Rendering.

- **Usage**: Passed to `CommandEncoder::BeginRendering`.
- **Members**:
  - ColorAttachments: `View`, `LoadOp`, `StoreOp`, `ClearColor`.
  - DepthStencilAttachment: `View`, `LoadOp`, `StoreOp`, `ClearDepth`, `ClearStencil`.

---

## Implementation Plan (TODO)
### Phase 0: Add Target (Rhi.General)
- [x] Add a target `Rhi.General` using CMake
  - [x] Add a dllexport-ed hello world to verify the step

### Phase 1: API Definition (Rhi.General)
- [ ] **Core Types**: Define `RhiEnums.h` (Formats, ResourceUsage, ShaderStages, etc.) and `RhiStructs.h` (Descriptors).
- [ ] **Device & Context**: Define `RhiContext`, `RhiAdapter`, `RhiDevice`, `RhiQueue` abstract base classes.
- [ ] **Resources**: Define base classes `RhiBuffer`, `RhiTexture`, `RhiSampler`, `RhiShader`.
    - [ ] **Crucial**: Implement common members (e.g., `mDesc`, `mDebugName`, `GetDesc()`) in these base classes to avoid duplication in backends.
- [ ] **Layout & Binding**: Define `RhiPipelineLayout`, `RhiBindGroupLayout`, `RhiBindGroup`.
- [ ] **Pipeline**: Define `RhiPipeline` and `RhiPipelineCache` base classes.
- [ ] **Commands**: Define `RhiCommandPool`, `RhiCommandBuffer`.
    - [ ] `RhiCommandEncoder` should be a **concrete helper class** (not virtual) that wraps a `RhiCommandBuffer` to provide a type-safe recording API.
- [ ] **Synchronization**: Define `RhiFence`, `RhiSemaphore`.

### Phase 2: Mock Backend & Verification
- [ ] Create `Source/Engine/Rhi/Mock` module.
- [ ] Implement a "No-op" RHI (inheriting from Rhi base classes) to validate architectural compilation and linking.
- [ ] Write basic unit tests in `Source/Tests/Rhi` to ensure interface creation/destruction doesn't crash.

### Phase 3: DX12 Backend (Rhi.DirectX12)
- [ ] **Initialization**: Implement `D3D12Context`, `D3D12Device` (inheriting `RhiDevice`), and Adapter enumeration (`IDXGIFactory`).
- [ ] **Memory Management**: Integrate D3D12MemoryAllocator (D3D12MA) library.
- [ ] **Resource Creation**: Implement Buffer and Texture creation using D3D12MA.
- [ ] **Descriptors**: Implement a CPU/GPU descriptor heap management strategy (`RhiDescriptorHeap`).
- [ ] **Command Recording**: Implement `D3D12CommandBuffer` wrapping `ID3D12GraphicsCommandList`.
- [ ] **Swapchain**: Implement `RhiSwapchain` for Present.
- [ ] **Barriers**: Implement state tracking or explicit barrier translation resource states.

### Phase 4: Vulkan Backend (Rhi.Vulkan)
- [ ] **Initialization**: Instance creation, Validation Layers, Physical Device selection.
- [ ] **Memory mgt**: Integrate VulkanMemoryAllocator (VMA).
- [ ] **Pipeline**: Shader Module creation (SPIR-V) and Pipeline caching.
- [ ] **Synchronization**: Accurate Semaphore/Fence mapping.

### Phase 5: Higher Level Utilities
- [ ] **Staging Manager**: Implement ring-buffer based upload heaps.
- [ ] **Transfers**: Async copy queue management.
- [ ] **Debug**: Pix/RenderDoc integration markers.
