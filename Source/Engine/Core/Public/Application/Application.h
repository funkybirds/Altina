#pragma once

#include "CoreMinimal.h"

namespace AltinaEngine
{

    class AE_CORE_API FApplication
    {
    public:
        explicit FApplication(const FStartupParameters& InStartupParams);

        void               Initialize();
        void               Tick(float InDeltaTime);
        void               Shutdown();

        [[nodiscard]] bool IsRunning() const noexcept;

    private:
        FStartupParameters mStartupParameters;
        bool               mIsRunning = false;
    };

} // namespace AltinaEngine
