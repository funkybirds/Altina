# AltinaEngine Memo

## Ifrit-v2中的Bug

- `static_cast`和继承：
  - https://github.com/Aeroraven/Ifrit-v2/blob/dev/public_mirror/include/ifrit/core/reflection/Object.h 的 `Object::As<T>`的实现是 UB