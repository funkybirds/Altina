#include "Base/AltinaBase.h"
#include "Launch/DemoRuntime.h"

using namespace AltinaEngine;

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    Launch::FDemoProjectDescriptor descriptor{};
    descriptor.DemoName       = TEXT("Minimal");
    descriptor.DemoModulePath = TEXT("AltinaDemoMinimal.dll");
    return Launch::RunDemoHost(descriptor, startupParams);
}
