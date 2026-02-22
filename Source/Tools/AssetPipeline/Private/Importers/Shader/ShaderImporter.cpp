#include "Importers/Shader/ShaderImporter.h"

#include "AssetToolIO.h"

#include <sstream>
#include <string>

namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        auto ToLowerAscii(char value) -> char {
            if (value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }
            return value;
        }

        void ToLowerAscii(std::string& value) {
            for (char& character : value) {
                character = ToLowerAscii(character);
            }
        }

        auto ExtractIncludePath(const std::string& line, std::string& outPath) -> bool {
            const auto firstNonSpace = line.find_first_not_of(" \t");
            if (firstNonSpace == std::string::npos) {
                return false;
            }
            if (line.compare(firstNonSpace, 8, "#include") != 0) {
                return false;
            }

            const auto hashPos = firstNonSpace;

            const auto quotePos = line.find_first_of("\"<", hashPos);
            if (quotePos == std::string::npos) {
                return false;
            }

            const char endChar = (line[quotePos] == '<') ? '>' : '"';
            const auto endPos  = line.find(endChar, quotePos + 1);
            if (endPos == std::string::npos || endPos <= quotePos + 1) {
                return false;
            }

            outPath = line.substr(quotePos + 1, endPos - quotePos - 1);
            return !outPath.empty();
        }

        auto PreprocessShaderText(const std::string& text, const std::filesystem::path& currentDir,
            const std::vector<std::filesystem::path>& includeDirs,
            std::vector<std::filesystem::path>& includeStack, std::string& outText) -> bool {
            std::istringstream stream(text);
            std::string        line;
            while (std::getline(stream, line)) {
                std::string includePath;
                if (ExtractIncludePath(line, includePath)) {
                    std::filesystem::path resolved;

                    auto TryResolve = [&](const std::filesystem::path& dir) -> bool {
                        std::filesystem::path candidate = dir / includePath;
                        std::error_code       ec;
                        if (std::filesystem::exists(candidate, ec)) {
                            resolved = candidate;
                            return true;
                        }
                        return false;
                    };

                    if (!TryResolve(currentDir)) {
                        for (const auto& dir : includeDirs) {
                            if (TryResolve(dir)) {
                                break;
                            }
                        }
                    }

                    if (resolved.empty()) {
                        return false;
                    }

                    resolved = resolved.lexically_normal();
                    for (const auto& stackEntry : includeStack) {
                        if (stackEntry == resolved) {
                            return false;
                        }
                    }

                    std::string includeText;
                    if (!ReadFileText(resolved, includeText)) {
                        return false;
                    }

                    includeStack.push_back(resolved);
                    if (!PreprocessShaderText(includeText, resolved.parent_path(), includeDirs,
                            includeStack, outText)) {
                        return false;
                    }
                    includeStack.pop_back();
                    outText.push_back('\n');
                } else {
                    outText.append(line);
                    outText.push_back('\n');
                }
            }
            return true;
        }
    } // namespace

    auto CookShader(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        const std::filesystem::path& repoRoot, std::vector<u8>& outCooked,
        Asset::FShaderDesc& outDesc) -> bool {
        std::string                        text(sourceBytes.begin(), sourceBytes.end());

        std::vector<std::filesystem::path> includeDirs;
        includeDirs.push_back(sourcePath.parent_path());

        std::filesystem::path shaderRoot = repoRoot / "Source" / "Shader";
        std::error_code       ec;
        if (std::filesystem::exists(shaderRoot, ec)) {
            includeDirs.push_back(shaderRoot);
        }

        std::vector<std::filesystem::path> includeStack;
        includeStack.push_back(sourcePath);

        std::string output;
        if (!PreprocessShaderText(
                text, sourcePath.parent_path(), includeDirs, includeStack, output)) {
            return false;
        }

        outCooked.assign(output.begin(), output.end());

        std::string ext = sourcePath.extension().string();
        ToLowerAscii(ext);
        if (ext == ".slang") {
            outDesc.Language = Asset::kShaderLanguageSlang;
        } else {
            outDesc.Language = Asset::kShaderLanguageHlsl;
        }

        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline
