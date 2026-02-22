#include "AssetToolIO.h"

#include <fstream>
#include <sstream>

namespace AltinaEngine::Tools::AssetPipeline {
    auto ReadFileText(const std::filesystem::path& path, std::string& outText) -> bool {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        outText = stream.str();
        return true;
    }

    auto ReadFileBytes(const std::filesystem::path& path, std::vector<u8>& outBytes) -> bool {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        file.seekg(0, std::ios::end);
        const auto endPos = file.tellg();
        if (endPos < 0) {
            return false;
        }
        const auto size = static_cast<size_t>(endPos);
        file.seekg(0, std::ios::beg);

        outBytes.resize(size);
        if (size > 0U) {
            file.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(size));
        }
        return file.good() || file.eof();
    }

    auto WriteTextFile(const std::filesystem::path& path, const std::string& text) -> bool {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        if (!text.empty()) {
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
        }
        return file.good();
    }

    auto WriteBytesFile(const std::filesystem::path& path, const std::vector<u8>& bytes) -> bool {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        if (!bytes.empty()) {
            file.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        return file.good();
    }
} // namespace AltinaEngine::Tools::AssetPipeline
