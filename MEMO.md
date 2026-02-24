# AltinaEngine Memo

## Ifrit-v2中的Bug

- `static_cast`和继承：
  - https://github.com/Aeroraven/Ifrit-v2/blob/dev/public_mirror/include/ifrit/core/reflection/Object.h 的 `Object::As<T>`的实现是 UB

## 待解决问题
- NDC->纹理坐标的 Y 方向约定差异
  - 引用位置: `Source/Shader/Deferred/DeferredLighting.hlsl`
- Slang->DXBC对矩阵布局转义存在问题（DXBC-IR解释已经确认）
  - 引用位置: `Source/Shader/Deferred/DeferredLighting.hlsl`