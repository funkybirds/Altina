#include "TestHarness.h"

#include "GeometryProcessing/GeometryProcessingModule.h"

TEST_CASE("GeometryProcessing module symbols are linkable") {
    AltinaEngine::GeometryProcessing::FGeometryProcessingModule::LogHelloWorld();
    REQUIRE(true);
}
