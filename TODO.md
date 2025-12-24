# AltinaEngine TODO

0. 在实现时，不要使用STL库，除了特殊说明。（这是规范，不需要修改现有实现）
   1. 所有Concept和TypeTraits均在 `Source/Engine/Core/Public/Types` 下
   2. 所有权和指针(TOwner和TShared)、字符串(FString)均在 `Source/Engine/Core/Public/Container` 下
   3. 所有日志使用带类别的日志，不要使用默认日志
   4. 禁止使用平台特定库：如果必须，添加平台无关声明并在 `Source/Engine/Core/Private/Platform/<Platform>` 实现

1. 移除所有冗余的 `AltinaEngine` 命名空间前缀（计划） — 待执行
   - 范围: TypeTraits、Concepts、容器类型、智能指针、字符串、基本类型别名 (usize, i8, f32 等)
   - 参考: 已在 `AGENTS.md` 添加命名空间命名规则

2. 已完成 — Core/Types 实用项
   - 已实现: `CheckedCast<T, U>` (`Source/Engine/Core/Public/Types/CheckedCast.h`)
   - 已实现: `NonCopyableClass` 和 `NonCopyableStruct` (`Source/Engine/Core/Public/Types/NonCopyable.h`)

3. 已完成 — Range 概念
   - 已添加: `IRange`, `IWritableRange`, `IReadableRange`, `ICommonRange`, `IIncrementable`, `IForwardRange` (`Source/Engine/Core/Public/Types/Concepts.h`)

4. 已完成 — Range 算法
   - 已实现: `MaxElement`, `MinElement`, `AnyOf`, `AllOf`, `NoneOf`, `IsSorted` (`Source/Engine/Core/Public/Algorithm/Range.h`)
   - 已添加测试: `Source/Tests/Core/RangeTests.cpp`

5. 已完成 — 容器与并发辅助
   - 已实现: `FScopedLock` (`Source/Engine/Core/Public/Container/Concurrent/ScopedLock.h`)
   - 已实现: `TDeque` (`Source/Engine/Core/Public/Container/Deque.h`)
   - 已实现: `TQueue` (`Source/Engine/Core/Public/Container/Queue.h`)
   - 已实现: `TStack` (`Source/Engine/Core/Public/Container/Stack.h`)
   - 已添加测试: `Source/Tests/Core/ContainerTests.cpp`

6. 下一步 / 开放项
   - 移除 `AltinaEngine` 前缀（计划执行顺序待确认）
   - 将命名规则和约定写入 `AGENTS.md` / `docs/CodingStyle.md`
