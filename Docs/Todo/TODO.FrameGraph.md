# FrameGraph 设计草案（RenderCore）

## 目标
- 提供基于 Pass 的渲染调度系统，具备资源生命周期管理、状态转换与别名复用能力。
- 让 Pass 的输入/输出显式化，以便图编译阶段做校验与裁剪。
- 保持后端无关并对接 RHI（D3D11 转换为 no-op，D3D12/VK 生成真实 barrier）。

## 非目标（暂时不做）
- 完整的渲染管线状态系统或着色器编译流程。
- 多队列并行调度（允许声明队列，但不做 async overlap）。
- 复杂的 transient 堆分配器（先用基础池化）。

---

## 核心概念

### 图生命周期
```
FFrameGraphGraph rdg(device);
rdg.BeginFrame(frameIndex);
// 添加 Pass 与资源
rdg.Compile();
rdg.Execute(cmdContext);
rdg.EndFrame();
```

### 资源句柄（逻辑）
- `FFrameGraphTextureRef` / `FFrameGraphBufferRef` 指向图内资源。
- 图内资源在 `Compile()` 前是逻辑资源，编译后映射到真实 RHI 资源。

### 视图句柄（逻辑）
- `FFrameGraphSrvRef / FFrameGraphUavRef / FFrameGraphRtvRef / FFrameGraphDsvRef`
- 视图由图内资源创建，可指定子资源范围。

### Pass
每个 Pass 需要声明：
- 名称
- 队列类型（Graphics/Compute/Copy）
- 资源/视图的读写访问
- 执行回调

---

## 主要接口草案

```cpp
namespace AltinaEngine::RenderCore {

enum class ERdgQueue : u8 { Graphics, Compute, Copy };
enum class ERdgPassType : u8 { Raster, Compute, Copy };

struct FFrameGraphTextureDesc {
    Rhi::FRhiTextureDesc mDesc;
    Rhi::ERhiResourceState mInitialState = Rhi::ERhiResourceState::Common;
};

struct FFrameGraphBufferDesc {
    Rhi::FRhiBufferDesc mDesc;
    Rhi::ERhiResourceState mInitialState = Rhi::ERhiResourceState::Common;
};

struct FFrameGraphTextureRef { u32 mId = 0; };
struct FFrameGraphBufferRef  { u32 mId = 0; };
struct FFrameGraphSrvRef     { u32 mId = 0; };
struct FFrameGraphUavRef     { u32 mId = 0; };
struct FFrameGraphRtvRef     { u32 mId = 0; };
struct FFrameGraphDsvRef     { u32 mId = 0; };

struct FFrameGraphPassResources {
    Rhi::FRhiTexture* GetTexture(FFrameGraphTextureRef ref) const;
    Rhi::FRhiBuffer*  GetBuffer(FFrameGraphBufferRef ref) const;
    Rhi::FRhiShaderResourceView* GetSrv(FFrameGraphSrvRef ref) const;
    Rhi::FRhiUnorderedAccessView* GetUav(FFrameGraphUavRef ref) const;
    Rhi::FRhiRenderTargetView* GetRtv(FFrameGraphRtvRef ref) const;
    Rhi::FRhiDepthStencilView* GetDsv(FFrameGraphDsvRef ref) const;
};

struct FFrameGraphPassBuilder {
    FFrameGraphTextureRef CreateTexture(const FFrameGraphTextureDesc& desc);
    FFrameGraphBufferRef  CreateBuffer(const FFrameGraphBufferDesc& desc);

    FFrameGraphTextureRef Read(FFrameGraphTextureRef tex, Rhi::ERhiResourceState state);
    FFrameGraphTextureRef Write(FFrameGraphTextureRef tex, Rhi::ERhiResourceState state);
    FFrameGraphBufferRef  Read(FFrameGraphBufferRef buf, Rhi::ERhiResourceState state);
    FFrameGraphBufferRef  Write(FFrameGraphBufferRef buf, Rhi::ERhiResourceState state);

    FFrameGraphSrvRef CreateSrv(FFrameGraphTextureRef tex, const Rhi::FRhiShaderResourceViewDesc& desc);
    FFrameGraphUavRef CreateUav(FFrameGraphTextureRef tex, const Rhi::FRhiUnorderedAccessViewDesc& desc);
    FFrameGraphRtvRef CreateRtv(FFrameGraphTextureRef tex, const Rhi::FRhiRenderTargetViewDesc& desc);
    FFrameGraphDsvRef CreateDsv(FFrameGraphTextureRef tex, const Rhi::FRhiDepthStencilViewDesc& desc);

    void SetRenderTargets(const FFrameGraphRtvRef* rtvs, u32 rtvCount, FFrameGraphDsvRef dsv);
};

using TRdgPassExecuteFn = void(*)(Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& res);

struct FFrameGraphPassDesc {
    const char* mName = "UnnamedPass";
    ERdgPassType mType = ERdgPassType::Raster;
    ERdgQueue    mQueue = ERdgQueue::Graphics;
    TRdgPassExecuteFn mExecute = nullptr;
};

class FFrameGraphGraph {
public:
    explicit FFrameGraphGraph(Rhi::FRhiDevice& device);

    void BeginFrame(u64 frameIndex);
    void EndFrame();

    template <typename SetupFunc>
    void AddPass(const FFrameGraphPassDesc& desc, SetupFunc&& setup);

    void Compile();
    void Execute(Rhi::FRhiCmdContext& cmdContext);

private:
    // 内部保存 Pass、资源与编译结果。
};

} // namespace AltinaEngine::RenderCore
```

---

## 编译流程（高层）
1. **Validate**：每个读资源必须有生产者；状态一致性校验。  
2. **Cull**：裁剪无用 Pass/资源（没有外部输出）。  
3. **Lifetime**：建立资源生命周期区间（首次写、最后读）。  
4. **Alias**：基于生命周期从池中分配 transient 资源。  
5. **Transitions**：  
   - 资源状态在 Pass 之间变化时生成转换。  
   - 通过 `RHIBeginTransition / RHIEndTransition` 发送 `FRhiTransitionCreateInfo`。  
6. **Build RenderPass**：  
   - Raster Pass 生成 `FRhiRenderPassDesc`（RTV/DSV + load/store）。  
7. **Execution**：  
   - 按 Pass 顺序录制命令并提交到指定队列。  

---

## RHI 对接说明
- D3D11：Transition 是 no-op；RenderPass 映射到 `OMSetRenderTargets + Clear`。  
- D3D12/VK：Transition 生成真实 barrier；支持 queue 与 split begin/end。  
- `FRhiRenderPassDesc` 应由 FrameGraph 生成（而非业务侧手写）。  

---

## 外部资源导入
支持导入已有 RHI 资源：
```
FFrameGraphTextureRef ImportTexture(Rhi::FRhiTexture* external, Rhi::ERhiResourceState state);
FFrameGraphBufferRef  ImportBuffer(Rhi::FRhiBuffer* external, Rhi::ERhiResourceState state);
```
导入资源由外部管理生命周期，不参与别名复用。  

---

## 输出与 Present
- 允许标记纹理为外部输出（例如 swapchain back buffer）。  
- FrameGraph 需保证输出资源以 `Present` 状态结束。  

---

## 下一步
- 实现最小 FrameGraph：资源存储 + 校验 + Pass 执行（先不做 alias）。  
- 做一个最小 transient 纹理池。  
- 在 `Source/Tests` 增加 RenderGraph 单测，验证裁剪与转换。  
