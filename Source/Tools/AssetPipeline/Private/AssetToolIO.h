#pragma once

#include "Types/Aliases.h"

#include <filesystem>
#include <string>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto ReadFileText(const std::filesystem::path& path, std::string& outText) -> bool;
    auto ReadFileBytes(const std::filesystem::path& path, std::vector<u8>& outBytes) -> bool;
    auto WriteTextFile(const std::filesystem::path& path, const std::string& text) -> bool;
    auto WriteBytesFile(const std::filesystem::path& path, const std::vector<u8>& bytes) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
