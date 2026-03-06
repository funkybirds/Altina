#include "Launch/GameClient.h"

#include "Utility/EngineConfig/EngineConfig.h"

namespace AltinaEngine::Launch {
    namespace {
        class FGameHostHooks final : public IRuntimeHostHooks {
        public:
            FGameHostHooks(FGameClient& inClient, FEngineLoop& inEngineLoop)
                : Client(inClient), EngineLoop(inEngineLoop) {}

            auto OnPreInit(IRuntimeSession& session) -> bool override {
                (void)session;
                return Client.OnPreInit(EngineLoop);
            }

            auto OnInit(IRuntimeSession& session) -> bool override {
                (void)session;
                return Client.OnInit(EngineLoop);
            }

            auto OnHostFrame(IRuntimeSession& session, const FFrameContext& frameContext)
                -> bool override {
                (void)session;
                return Client.OnTick(EngineLoop, frameContext.DeltaSeconds);
            }

            auto OnAfterFrame(IRuntimeSession& session, const FFrameContext& frameContext)
                -> void override {
                (void)session;
                (void)frameContext;
            }

            auto OnShutdown(IRuntimeSession& session) -> void override {
                (void)session;
                Client.OnShutdown(EngineLoop);
            }

        private:
            FGameClient& Client;
            FEngineLoop& EngineLoop;
        };
    } // namespace

    auto RunGameHost(FGameClient& client, const FStartupParameters& startupParameters) -> int {
        Core::Utility::EngineConfig::InitializeGlobalConfig(startupParameters);

        auto  sessionOwner = CreateDefaultRuntimeSession(startupParameters);
        auto* engineLoop   = dynamic_cast<FEngineLoop*>(sessionOwner.Get());
        if (engineLoop == nullptr) {
            return 1;
        }

        FGameHostHooks             hooks(client, *engineLoop);

        FHostApplicationLoopConfig config{};
        config.FixedDeltaSeconds = client.GetFixedDeltaTimeSeconds();
        config.SleepPerFrame     = true;
        config.SleepMilliseconds = client.GetSleepMilliseconds();

        FHostApplicationLoop loop{};
        return loop.Run(*engineLoop, hooks, config);
    }

    auto RunGameClient(FGameClient& client, const FStartupParameters& startupParameters) -> int {
        return RunGameHost(client, startupParameters);
    }
} // namespace AltinaEngine::Launch
