#pragma once

#include "../Allocator.h"
#include "../../Types/Aliases.h"
#include "../../Types/Traits.h"

using AltinaEngine::Move;

namespace AltinaEngine::Core::Container {

    template <typename TKey, typename TValue, typename TKeyOfValue,
        typename TAllocatorType = TAllocator<TValue>,
        typename TCompare       = ::AltinaEngine::TLess<TKey>>
    class TRbTree {
    private:
        enum class ENodeColor : u8 {
            Red,
            Black
        };

        struct FNode {
            FNode*     mParent = nullptr;
            FNode*     mLeft   = nullptr;
            FNode*     mRight  = nullptr;
            ENodeColor mColor  = ENodeColor::Red;
            TValue     mValue;

            explicit FNode(const TValue& value) : mValue(value) {}
            explicit FNode(TValue&& value) : mValue(Move(value)) {}
        };

        using FNodeAllocator = typename TAllocatorType::template Rebind<FNode>::TOther;

    public:
        struct TIterator {
            FNode*             mNode  = nullptr;
            const TRbTree*     mOwner = nullptr;

            [[nodiscard]] auto operator*() const -> TValue& { return mNode->mValue; }
            [[nodiscard]] auto operator->() const -> TValue* { return &mNode->mValue; }

            auto               operator++() -> TIterator& {
                if (mOwner != nullptr) {
                    mNode = mOwner->Successor(mNode);
                }
                return *this;
            }

            auto operator++(int) -> TIterator {
                TIterator tmp = *this;
                ++(*this);
                return tmp;
            }

            [[nodiscard]] auto operator==(const TIterator& rhs) const -> bool {
                return mNode == rhs.mNode && mOwner == rhs.mOwner;
            }

            [[nodiscard]] auto operator!=(const TIterator& rhs) const -> bool {
                return !(*this == rhs);
            }
        };

        struct TInsertResult {
            TIterator mIterator;
            bool      mInserted = false;
        };

        TRbTree() = default;

        explicit TRbTree(const TCompare& compare, const TKeyOfValue& keyOfValue = TKeyOfValue(),
            const TAllocatorType& allocator = TAllocatorType())
            : mAllocator(allocator)
            , mNodeAllocator(allocator)
            , mCompare(compare)
            , mKeyOfValue(keyOfValue) {}

        explicit TRbTree(const TAllocatorType& allocator)
            : mAllocator(allocator), mNodeAllocator(allocator) {}

        TRbTree(const TRbTree&)                    = delete;
        auto operator=(const TRbTree&) -> TRbTree& = delete;

        TRbTree(TRbTree&& other) noexcept
            : mRoot(other.mRoot)
            , mSize(other.mSize)
            , mAllocator(Move(other.mAllocator))
            , mNodeAllocator(Move(other.mNodeAllocator))
            , mCompare(Move(other.mCompare))
            , mKeyOfValue(Move(other.mKeyOfValue)) {
            other.mRoot = nullptr;
            other.mSize = 0;
        }

        auto operator=(TRbTree&& other) noexcept -> TRbTree& {
            if (this == &other) {
                return *this;
            }

            Clear();
            mRoot          = other.mRoot;
            mSize          = other.mSize;
            mAllocator     = Move(other.mAllocator);
            mNodeAllocator = Move(other.mNodeAllocator);
            mCompare       = Move(other.mCompare);
            mKeyOfValue    = Move(other.mKeyOfValue);

            other.mRoot = nullptr;
            other.mSize = 0;
            return *this;
        }

        ~TRbTree() { Clear(); }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mSize == 0; }
        [[nodiscard]] auto Size() const noexcept -> usize { return mSize; }

        [[nodiscard]] auto Begin() const noexcept -> TIterator {
            return TIterator{ Minimum(mRoot), this };
        }
        [[nodiscard]] auto End() const noexcept -> TIterator { return TIterator{ nullptr, this }; }

        [[nodiscard]] auto Find(const TKey& key) const noexcept -> TIterator {
            return TIterator{ FindNode(key), this };
        }

        [[nodiscard]] auto Contains(const TKey& key) const noexcept -> bool {
            return FindNode(key) != nullptr;
        }

        auto InsertUnique(const TValue& value) -> TInsertResult {
            FNode* parent = nullptr;
            FNode* cur    = mRoot;
            auto   key    = mKeyOfValue(value);

            while (cur != nullptr) {
                parent          = cur;
                const auto& rhs = mKeyOfValue(cur->mValue);
                if (mCompare(key, rhs)) {
                    cur = cur->mLeft;
                } else if (mCompare(rhs, key)) {
                    cur = cur->mRight;
                } else {
                    return TInsertResult{ TIterator{ cur, this }, false };
                }
            }

            FNode* node   = AllocateNode(value);
            node->mParent = parent;
            node->mColor  = ENodeColor::Red;
            if (parent == nullptr) {
                mRoot = node;
            } else if (mCompare(key, mKeyOfValue(parent->mValue))) {
                parent->mLeft = node;
            } else {
                parent->mRight = node;
            }

            FixInsert(node);
            ++mSize;
            return TInsertResult{ TIterator{ node, this }, true };
        }

        auto InsertUnique(TValue&& value) -> TInsertResult {
            FNode* parent = nullptr;
            FNode* cur    = mRoot;
            auto   key    = mKeyOfValue(value);

            while (cur != nullptr) {
                parent          = cur;
                const auto& rhs = mKeyOfValue(cur->mValue);
                if (mCompare(key, rhs)) {
                    cur = cur->mLeft;
                } else if (mCompare(rhs, key)) {
                    cur = cur->mRight;
                } else {
                    return TInsertResult{ TIterator{ cur, this }, false };
                }
            }

            FNode* node   = AllocateNode(Move(value));
            node->mParent = parent;
            node->mColor  = ENodeColor::Red;
            if (parent == nullptr) {
                mRoot = node;
            } else if (mCompare(key, mKeyOfValue(parent->mValue))) {
                parent->mLeft = node;
            } else {
                parent->mRight = node;
            }

            FixInsert(node);
            ++mSize;
            return TInsertResult{ TIterator{ node, this }, true };
        }

        [[nodiscard]] auto LowerBound(const TKey& key) const noexcept -> TIterator {
            FNode* cur       = mRoot;
            FNode* candidate = nullptr;

            while (cur != nullptr) {
                const auto& nodeKey = mKeyOfValue(cur->mValue);
                if (!mCompare(nodeKey, key)) {
                    candidate = cur;
                    cur       = cur->mLeft;
                } else {
                    cur = cur->mRight;
                }
            }

            return TIterator{ candidate, this };
        }

        [[nodiscard]] auto UpperBound(const TKey& key) const noexcept -> TIterator {
            FNode* cur       = mRoot;
            FNode* candidate = nullptr;

            while (cur != nullptr) {
                const auto& nodeKey = mKeyOfValue(cur->mValue);
                if (mCompare(key, nodeKey)) {
                    candidate = cur;
                    cur       = cur->mLeft;
                } else {
                    cur = cur->mRight;
                }
            }

            return TIterator{ candidate, this };
        }

        auto Erase(const TKey& key) -> bool {
            FNode* node = FindNode(key);
            if (node == nullptr) {
                return false;
            }

            EraseNode(node);
            --mSize;
            return true;
        }

        void Clear() noexcept {
            DestroySubTree(mRoot);
            mRoot = nullptr;
            mSize = 0;
        }

    private:
        [[nodiscard]] static auto ColorOf(const FNode* node) noexcept -> ENodeColor {
            if (node == nullptr) {
                return ENodeColor::Black;
            }
            return node->mColor;
        }

        static void SetColor(FNode* node, const ENodeColor color) noexcept {
            if (node != nullptr) {
                node->mColor = color;
            }
        }

        [[nodiscard]] static auto Minimum(FNode* node) noexcept -> FNode* {
            FNode* cur = node;
            while (cur != nullptr && cur->mLeft != nullptr) {
                cur = cur->mLeft;
            }
            return cur;
        }

        [[nodiscard]] auto Successor(FNode* node) const noexcept -> FNode* {
            if (node == nullptr) {
                return nullptr;
            }

            if (node->mRight != nullptr) {
                return Minimum(node->mRight);
            }

            FNode* parent = node->mParent;
            FNode* cur    = node;
            while (parent != nullptr && cur == parent->mRight) {
                cur    = parent;
                parent = parent->mParent;
            }
            return parent;
        }

        [[nodiscard]] auto FindNode(const TKey& key) const noexcept -> FNode* {
            FNode* cur = mRoot;
            while (cur != nullptr) {
                const auto& nodeKey = mKeyOfValue(cur->mValue);
                if (mCompare(key, nodeKey)) {
                    cur = cur->mLeft;
                } else if (mCompare(nodeKey, key)) {
                    cur = cur->mRight;
                } else {
                    return cur;
                }
            }
            return nullptr;
        }

        auto AllocateNode(const TValue& value) -> FNode* {
            FNode* node = mNodeAllocator.Allocate(1);
            mNodeAllocator.Construct(node, value);
            return node;
        }

        auto AllocateNode(TValue&& value) -> FNode* {
            FNode* node = mNodeAllocator.Allocate(1);
            mNodeAllocator.Construct(node, Move(value));
            return node;
        }

        void DestroyNode(FNode* node) noexcept {
            if (node == nullptr) {
                return;
            }
            mNodeAllocator.Destroy(node);
            mNodeAllocator.Deallocate(node, 1);
        }

        void DestroySubTree(FNode* node) noexcept {
            if (node == nullptr) {
                return;
            }
            DestroySubTree(node->mLeft);
            DestroySubTree(node->mRight);
            DestroyNode(node);
        }

        void RotateLeft(FNode* x) noexcept {
            FNode* y  = x->mRight;
            x->mRight = y->mLeft;
            if (y->mLeft != nullptr) {
                y->mLeft->mParent = x;
            }
            y->mParent = x->mParent;
            if (x->mParent == nullptr) {
                mRoot = y;
            } else if (x == x->mParent->mLeft) {
                x->mParent->mLeft = y;
            } else {
                x->mParent->mRight = y;
            }
            y->mLeft   = x;
            x->mParent = y;
        }

        void RotateRight(FNode* y) noexcept {
            FNode* x = y->mLeft;
            y->mLeft = x->mRight;
            if (x->mRight != nullptr) {
                x->mRight->mParent = y;
            }
            x->mParent = y->mParent;
            if (y->mParent == nullptr) {
                mRoot = x;
            } else if (y == y->mParent->mLeft) {
                y->mParent->mLeft = x;
            } else {
                y->mParent->mRight = x;
            }
            x->mRight  = y;
            y->mParent = x;
        }

        void FixInsert(FNode* node) noexcept {
            FNode* cur = node;
            while (cur != mRoot && ColorOf(cur->mParent) == ENodeColor::Red) {
                FNode* parent      = cur->mParent;
                FNode* grandParent = parent->mParent;
                if (parent == grandParent->mLeft) {
                    FNode* uncle = grandParent->mRight;
                    if (ColorOf(uncle) == ENodeColor::Red) {
                        SetColor(parent, ENodeColor::Black);
                        SetColor(uncle, ENodeColor::Black);
                        SetColor(grandParent, ENodeColor::Red);
                        cur = grandParent;
                    } else {
                        if (cur == parent->mRight) {
                            cur = parent;
                            RotateLeft(cur);
                            parent      = cur->mParent;
                            grandParent = parent->mParent;
                        }
                        SetColor(parent, ENodeColor::Black);
                        SetColor(grandParent, ENodeColor::Red);
                        RotateRight(grandParent);
                    }
                } else {
                    FNode* uncle = grandParent->mLeft;
                    if (ColorOf(uncle) == ENodeColor::Red) {
                        SetColor(parent, ENodeColor::Black);
                        SetColor(uncle, ENodeColor::Black);
                        SetColor(grandParent, ENodeColor::Red);
                        cur = grandParent;
                    } else {
                        if (cur == parent->mLeft) {
                            cur = parent;
                            RotateRight(cur);
                            parent      = cur->mParent;
                            grandParent = parent->mParent;
                        }
                        SetColor(parent, ENodeColor::Black);
                        SetColor(grandParent, ENodeColor::Red);
                        RotateLeft(grandParent);
                    }
                }
            }
            SetColor(mRoot, ENodeColor::Black);
        }

        void Transplant(FNode* u, FNode* v) noexcept {
            if (u->mParent == nullptr) {
                mRoot = v;
            } else if (u == u->mParent->mLeft) {
                u->mParent->mLeft = v;
            } else {
                u->mParent->mRight = v;
            }
            if (v != nullptr) {
                v->mParent = u->mParent;
            }
        }

        void FixErase(FNode* node, FNode* parent) noexcept {
            FNode* cur       = node;
            FNode* curParent = parent;

            while (cur != mRoot && ColorOf(cur) == ENodeColor::Black) {
                if (curParent == nullptr) {
                    break;
                }
                if (cur == (curParent != nullptr ? curParent->mLeft : nullptr)) {
                    FNode* sibling = (curParent != nullptr) ? curParent->mRight : nullptr;
                    if (ColorOf(sibling) == ENodeColor::Red) {
                        SetColor(sibling, ENodeColor::Black);
                        SetColor(curParent, ENodeColor::Red);
                        RotateLeft(curParent);
                        sibling = (curParent != nullptr) ? curParent->mRight : nullptr;
                    }

                    const ENodeColor siblingLeftColor =
                        ColorOf(sibling != nullptr ? sibling->mLeft : nullptr);
                    const ENodeColor siblingRightColor =
                        ColorOf(sibling != nullptr ? sibling->mRight : nullptr);

                    if (siblingLeftColor == ENodeColor::Black
                        && siblingRightColor == ENodeColor::Black) {
                        SetColor(sibling, ENodeColor::Red);
                        cur       = curParent;
                        curParent = (cur != nullptr) ? cur->mParent : nullptr;
                    } else {
                        if (siblingRightColor == ENodeColor::Black) {
                            SetColor(
                                sibling != nullptr ? sibling->mLeft : nullptr, ENodeColor::Black);
                            SetColor(sibling, ENodeColor::Red);
                            if (sibling != nullptr) {
                                RotateRight(sibling);
                            }
                            sibling = (curParent != nullptr) ? curParent->mRight : nullptr;
                        }
                        SetColor(sibling, ColorOf(curParent));
                        SetColor(curParent, ENodeColor::Black);
                        SetColor(sibling != nullptr ? sibling->mRight : nullptr, ENodeColor::Black);
                        RotateLeft(curParent);
                        cur       = mRoot;
                        curParent = nullptr;
                    }
                } else {
                    FNode* sibling = (curParent != nullptr) ? curParent->mLeft : nullptr;
                    if (ColorOf(sibling) == ENodeColor::Red) {
                        SetColor(sibling, ENodeColor::Black);
                        SetColor(curParent, ENodeColor::Red);
                        RotateRight(curParent);
                        sibling = (curParent != nullptr) ? curParent->mLeft : nullptr;
                    }

                    const ENodeColor siblingLeftColor =
                        ColorOf(sibling != nullptr ? sibling->mLeft : nullptr);
                    const ENodeColor siblingRightColor =
                        ColorOf(sibling != nullptr ? sibling->mRight : nullptr);

                    if (siblingLeftColor == ENodeColor::Black
                        && siblingRightColor == ENodeColor::Black) {
                        SetColor(sibling, ENodeColor::Red);
                        cur       = curParent;
                        curParent = (cur != nullptr) ? cur->mParent : nullptr;
                    } else {
                        if (siblingLeftColor == ENodeColor::Black) {
                            SetColor(
                                sibling != nullptr ? sibling->mRight : nullptr, ENodeColor::Black);
                            SetColor(sibling, ENodeColor::Red);
                            if (sibling != nullptr) {
                                RotateLeft(sibling);
                            }
                            sibling = (curParent != nullptr) ? curParent->mLeft : nullptr;
                        }
                        SetColor(sibling, ColorOf(curParent));
                        SetColor(curParent, ENodeColor::Black);
                        SetColor(sibling != nullptr ? sibling->mLeft : nullptr, ENodeColor::Black);
                        RotateRight(curParent);
                        cur       = mRoot;
                        curParent = nullptr;
                    }
                }
            }
            SetColor(cur, ENodeColor::Black);
        }

        void EraseNode(FNode* node) noexcept {
            FNode*     y              = node;
            ENodeColor yOriginalColor = y->mColor;
            FNode*     x              = nullptr;
            FNode*     xParent        = nullptr;

            if (node->mLeft == nullptr) {
                x       = node->mRight;
                xParent = node->mParent;
                Transplant(node, node->mRight);
            } else if (node->mRight == nullptr) {
                x       = node->mLeft;
                xParent = node->mParent;
                Transplant(node, node->mLeft);
            } else {
                y              = Minimum(node->mRight);
                yOriginalColor = y->mColor;
                x              = y->mRight;
                if (y->mParent == node) {
                    xParent = y;
                    if (x != nullptr) {
                        x->mParent = y;
                    }
                } else {
                    xParent = y->mParent;
                    Transplant(y, y->mRight);
                    y->mRight          = node->mRight;
                    y->mRight->mParent = y;
                }
                Transplant(node, y);
                y->mLeft          = node->mLeft;
                y->mLeft->mParent = y;
                y->mColor         = node->mColor;
            }

            DestroyNode(node);

            if (yOriginalColor == ENodeColor::Black) {
                FixErase(x, xParent);
            }
        }

    private:
        FNode*         mRoot = nullptr;
        usize          mSize = 0;
        TAllocatorType mAllocator{};
        FNodeAllocator mNodeAllocator{};
        TCompare       mCompare{};
        TKeyOfValue    mKeyOfValue{};
    };

} // namespace AltinaEngine::Core::Container
