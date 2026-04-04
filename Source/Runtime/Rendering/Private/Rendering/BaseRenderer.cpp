#include "Rendering/Renderer.h"

#include "Container/HashMap.h"
#include "Logging/Log.h"
#include "Utility/Assert.h"

namespace AltinaEngine::Rendering {
    using namespace Core::Logging;
    using Core::Container::THashMap;
    using Core::Utility::DebugAssert;

    namespace {
        constexpr auto kRendererPassCategory = TEXT("Rendering.BaseRenderer");

        auto InsertU32At(Core::Container::TVector<u32>& values, u32 index, u32 value) -> void {
            Core::Container::TVector<u32> reordered{};
            const u32                     oldSize = static_cast<u32>(values.Size());
            if (index > oldSize) {
                index = oldSize;
            }
            reordered.Reserve(oldSize + 1U);
            for (u32 i = 0U; i < index; ++i) {
                reordered.PushBack(values[i]);
            }
            reordered.PushBack(value);
            for (u32 i = index; i < oldSize; ++i) {
                reordered.PushBack(values[i]);
            }
            values = AltinaEngine::Move(reordered);
        }
    } // namespace

    auto FBaseRenderer::RegisterPassToSet(const FRendererPassRegistration& registration) -> bool {
        if (registration.mPassId.IsEmptyString()) {
            DebugAssert(false, TEXT("Rendering.BaseRenderer"),
                "Builtin renderer pass id must not be empty.");
            return false;
        }
        if (!registration.mExecute) {
            DebugAssert(false, TEXT("Rendering.BaseRenderer"),
                "Builtin renderer pass '{}' must provide callback.", registration.mPassId.ToView());
            return false;
        }

        FRegisteredPass pass{};
        pass.mRegistration      = registration;
        pass.mFromPlugin        = false;
        pass.mRegistrationOrder = mRegistrationOrderCursor++;
        mBuiltinPasses.PushBack(AltinaEngine::Move(pass));
        mPassPlanDirty = true;
        return true;
    }

    void FBaseRenderer::MarkPassPlanDirty() { mPassPlanDirty = true; }

    void FBaseRenderer::EnsureBuiltinPassesRegistered() {
        if (mBuiltinPassesRegistered) {
            return;
        }

        mBuiltinPasses.Clear();
        RegisterBuiltinPasses();
        mBuiltinPassesRegistered = true;
        mPassPlanDirty           = true;
    }

    void FBaseRenderer::SetPluginPassRegistrations(
        const TVector<FRendererPassRegistration>& registrations) {
        mPluginPasses.Clear();
        mPluginPasses.Reserve(registrations.Size());
        for (const auto& registration : registrations) {
            FRegisteredPass pass{};
            pass.mRegistration      = registration;
            pass.mFromPlugin        = true;
            pass.mRegistrationOrder = mRegistrationOrderCursor++;
            mPluginPasses.PushBack(AltinaEngine::Move(pass));
        }
        mPassPlanDirty = true;
    }

    void FBaseRenderer::ClearPluginPassRegistrations() {
        mPluginPasses.Clear();
        mPassPlanDirty = true;
    }

    void FBaseRenderer::BuildResolvedPassPlan() {
        EnsureBuiltinPassesRegistered();
        if (!mPassPlanDirty) {
            return;
        }

        mResolvedPasses.Clear();

        TVector<FRegisteredPass> combined{};
        combined.Reserve(mBuiltinPasses.Size() + mPluginPasses.Size());
        for (const auto& builtin : mBuiltinPasses) {
            combined.PushBack(builtin);
        }
        for (const auto& plugin : mPluginPasses) {
            combined.PushBack(plugin);
        }

        const u32   combinedCount = static_cast<u32>(combined.Size());
        TVector<u8> bPassValid{};
        bPassValid.Resize(combinedCount);
        for (u32 i = 0U; i < combinedCount; ++i) {
            bPassValid[i] = 1U;
        }

        THashMap<FString, u32> passIndexById{};
        passIndexById.Reserve(combinedCount);

        auto RejectPass = [&](FRegisteredPass& pass, u32 index, const char* reason) {
            bPassValid[index] = 0U;
            if (pass.mFromPlugin) {
                LogWarningCat(kRendererPassCategory, "Plugin pass '{}' rejected: {}.",
                    pass.mRegistration.mPassId.ToView(), reason);
            } else {
                DebugAssert(false, TEXT("Rendering.BaseRenderer"),
                    "Builtin pass '{}' rejected: {}.", pass.mRegistration.mPassId.ToView(), reason);
            }
        };

        for (u32 i = 0U; i < combinedCount; ++i) {
            auto& pass = combined[i];
            if (pass.mRegistration.mPassId.IsEmptyString()) {
                RejectPass(pass, i, "empty pass id");
                continue;
            }
            if (!pass.mRegistration.mExecute) {
                RejectPass(pass, i, "empty pass callback");
                continue;
            }
            if (passIndexById.Contains(pass.mRegistration.mPassId)) {
                RejectPass(pass, i, "duplicate pass id");
                continue;
            }
            passIndexById[pass.mRegistration.mPassId] = i;
        }

        for (u32 i = 0U; i < combinedCount; ++i) {
            if (bPassValid[i] == 0U) {
                continue;
            }

            auto& pass = combined[i];
            if (pass.mRegistration.mAnchorOrder == ERendererPassAnchorOrder::None) {
                continue;
            }
            if (pass.mRegistration.mAnchorPassId.IsEmptyString()) {
                RejectPass(pass, i, "anchor order is set but anchor id is empty");
                continue;
            }

            const u32* anchorIndexPtr = passIndexById.Find(pass.mRegistration.mAnchorPassId);
            if (anchorIndexPtr == nullptr) {
                RejectPass(pass, i, "anchor pass is missing");
                continue;
            }

            const u32 anchorIndex = *anchorIndexPtr;
            if (bPassValid[anchorIndex] == 0U) {
                RejectPass(pass, i, "anchor pass is invalid");
                continue;
            }

            const auto anchorSet = combined[anchorIndex].mRegistration.mPassSet;
            if (anchorSet != pass.mRegistration.mPassSet) {
                RejectPass(pass, i, "anchor pass is in different pass set");
                continue;
            }
        }

        for (u32 setOrdinal = static_cast<u32>(ERendererPassSet::Prepass);
            setOrdinal <= static_cast<u32>(ERendererPassSet::PostProcess); ++setOrdinal) {
            const auto   currentSet = static_cast<ERendererPassSet>(setOrdinal);

            TVector<u32> setIndices{};
            setIndices.Reserve(combinedCount);
            for (u32 i = 0U; i < combinedCount; ++i) {
                if (bPassValid[i] == 0U) {
                    continue;
                }
                if (combined[i].mRegistration.mPassSet == currentSet) {
                    setIndices.PushBack(i);
                }
            }

            const u32 setCount = static_cast<u32>(setIndices.Size());
            if (setCount == 0U) {
                continue;
            }

            THashMap<u32, u32> setGlobalToLocal{};
            setGlobalToLocal.Reserve(setCount);
            for (u32 localIndex = 0U; localIndex < setCount; ++localIndex) {
                setGlobalToLocal[setIndices[localIndex]] = localIndex;
            }

            TVector<u8> inserted{};
            inserted.Resize(setCount);
            for (u32 i = 0U; i < setCount; ++i) {
                inserted[i] = 0U;
            }

            TVector<u32> orderedGlobals{};
            orderedGlobals.Reserve(setCount);
            u32 insertedCount = 0U;
            while (insertedCount < setCount) {
                bool bProgress = false;

                for (u32 localIndex = 0U; localIndex < setCount; ++localIndex) {
                    if (inserted[localIndex] != 0U) {
                        continue;
                    }

                    const u32   globalIndex  = setIndices[localIndex];
                    const auto& registration = combined[globalIndex].mRegistration;

                    if (registration.mAnchorOrder == ERendererPassAnchorOrder::None) {
                        orderedGlobals.PushBack(globalIndex);
                        inserted[localIndex] = 1U;
                        ++insertedCount;
                        bProgress = true;
                        continue;
                    }

                    const u32* anchorGlobalIndexPtr =
                        passIndexById.Find(registration.mAnchorPassId);
                    if (anchorGlobalIndexPtr == nullptr) {
                        continue;
                    }
                    const u32  anchorGlobalIndex = *anchorGlobalIndexPtr;

                    const u32* anchorLocalIndexPtr = setGlobalToLocal.Find(anchorGlobalIndex);
                    if (anchorLocalIndexPtr == nullptr) {
                        continue;
                    }
                    const u32 anchorLocalIndex = *anchorLocalIndexPtr;
                    if (inserted[anchorLocalIndex] == 0U) {
                        continue;
                    }

                    u32       anchorOrderIndex    = 0U;
                    bool      bFoundAnchorInOrder = false;
                    const u32 orderedCount        = static_cast<u32>(orderedGlobals.Size());
                    for (u32 orderedIndex = 0U; orderedIndex < orderedCount; ++orderedIndex) {
                        if (orderedGlobals[orderedIndex] == anchorGlobalIndex) {
                            anchorOrderIndex    = orderedIndex;
                            bFoundAnchorInOrder = true;
                            break;
                        }
                    }
                    if (!bFoundAnchorInOrder) {
                        continue;
                    }

                    u32 insertIndex = anchorOrderIndex;
                    if (registration.mAnchorOrder == ERendererPassAnchorOrder::After) {
                        insertIndex = anchorOrderIndex + 1U;
                        while (insertIndex < orderedGlobals.Size()) {
                            const auto& nextPass =
                                combined[orderedGlobals[insertIndex]].mRegistration;
                            if (nextPass.mAnchorOrder == ERendererPassAnchorOrder::After
                                && nextPass.mAnchorPassId == registration.mAnchorPassId) {
                                ++insertIndex;
                                continue;
                            }
                            break;
                        }
                    }

                    InsertU32At(orderedGlobals, insertIndex, globalIndex);
                    inserted[localIndex] = 1U;
                    ++insertedCount;
                    bProgress = true;
                }

                if (!bProgress) {
                    for (u32 localIndex = 0U; localIndex < setCount; ++localIndex) {
                        if (inserted[localIndex] != 0U) {
                            continue;
                        }
                        const u32 unresolvedGlobalIndex = setIndices[localIndex];
                        auto&     unresolvedPass        = combined[unresolvedGlobalIndex];
                        if (unresolvedPass.mFromPlugin) {
                            LogWarningCat(kRendererPassCategory,
                                "Plugin pass '{}' rejected: unresolved anchor dependency.",
                                unresolvedPass.mRegistration.mPassId.ToView());
                            bPassValid[unresolvedGlobalIndex] = 0U;
                            continue;
                        }

                        DebugAssert(false, TEXT("Rendering.BaseRenderer"),
                            "Builtin pass '{}' has unresolved anchor dependency.",
                            unresolvedPass.mRegistration.mPassId.ToView());
                        orderedGlobals.PushBack(unresolvedGlobalIndex);
                    }
                    break;
                }
            }

            for (const u32 orderedGlobal : orderedGlobals) {
                if (orderedGlobal < combinedCount && bPassValid[orderedGlobal] != 0U) {
                    mResolvedPasses.PushBack(combined[orderedGlobal]);
                }
            }
        }

        mPassPlanDirty = false;
    }

    void FBaseRenderer::Render(RenderCore::FFrameGraph& graph) {
        BuildResolvedPassPlan();
        for (auto& pass : mResolvedPasses) {
            if (pass.mRegistration.mExecute) {
                pass.mRegistration.mExecute(graph);
            }
        }
    }

    auto FBaseRenderer::GetResolvedPassIds() -> TVector<FString> {
        BuildResolvedPassPlan();
        TVector<FString> outPassIds{};
        outPassIds.Reserve(mResolvedPasses.Size());
        for (const auto& pass : mResolvedPasses) {
            outPassIds.PushBack(pass.mRegistration.mPassId);
        }
        return outPassIds;
    }

    void FRendererBuilder::ClearPassProviders() { mPassProviders.Clear(); }

    void FRendererBuilder::AddPassProvider(IRendererPassProvider* provider) {
        if (provider == nullptr) {
            return;
        }
        mPassProviders.PushBack(provider);
    }

    auto FRendererBuilder::RegisterPluginPass(const FRendererPassRegistration& registration)
        -> bool {
        mCollectedPluginPasses.PushBack(registration);
        return true;
    }

    void FRendererBuilder::ApplyToRenderer(FBaseRenderer& renderer) {
        mCollectedPluginPasses.Clear();
        for (auto* provider : mPassProviders) {
            if (provider == nullptr) {
                continue;
            }
            provider->RegisterPasses(*this);
        }
        renderer.SetPluginPassRegistrations(mCollectedPluginPasses);
    }
} // namespace AltinaEngine::Rendering
