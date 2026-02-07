#include "ShaderCompilerUtils.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

#include "Base/AltinaBase.h"

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #ifdef TEXT
        #undef TEXT
    #endif
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #define TEXT(str) L##str
    #else
        #define TEXT(str) str
    #endif
#endif

namespace AltinaEngine::ShaderCompiler::Detail {
    namespace {
        template <typename CharT>
        auto ToWideStringImpl(const Core::Container::TBasicString<CharT>& value) -> std::wstring {
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                return std::wstring(value.GetData(), value.Length());
            } else {
#if AE_PLATFORM_WIN
                if (value.Length() == 0) {
                    return {};
                }
                int wideCount = MultiByteToWideChar(CP_UTF8, 0, value.GetData(),
                    static_cast<int>(value.Length()), nullptr, 0);
                if (wideCount <= 0) {
                    return {};
                }
                std::wstring wide(static_cast<size_t>(wideCount), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, value.GetData(),
                    static_cast<int>(value.Length()), wide.data(), wideCount);
                return wide;
#else
                return std::wstring(value.GetData(), value.GetData() + value.Length());
#endif
            }
        }

        auto ToWideString(const FString& value) -> std::wstring {
            return ToWideStringImpl(value);
        }

        template <typename CharT>
        auto FromUtf8Impl(const std::string& value) -> Core::Container::TBasicString<CharT> {
            Core::Container::TBasicString<CharT> out;
            if (value.empty()) {
                return out;
            }
            if constexpr (std::is_same_v<CharT, wchar_t>) {
#if AE_PLATFORM_WIN
                int wideCount =
                    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        nullptr, 0);
                if (wideCount <= 0) {
                    return out;
                }
                std::wstring wide(static_cast<size_t>(wideCount), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                    wide.data(), wideCount);
                out.Append(wide.c_str(), wide.size());
#else
                out.Append(value.c_str(), value.size());
#endif
            } else {
                out.Append(value.c_str(), value.size());
            }
            return out;
        }

        auto FromUtf8(const std::string& value) -> FString {
            return FromUtf8Impl<TChar>(value);
        }

        template <typename CharT>
        auto FromPathImpl(const std::filesystem::path& path) -> Core::Container::TBasicString<CharT> {
            Core::Container::TBasicString<CharT> out;
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                const auto native = path.wstring();
                out.Append(native.c_str(), native.size());
            } else {
                const auto native = path.string();
                out.Append(native.c_str(), native.size());
            }
            return out;
        }

        auto FromPath(const std::filesystem::path& path) -> FString {
            return FromPathImpl<TChar>(path);
        }

        template <typename CharT>
        auto ToPathImpl(const Core::Container::TBasicString<CharT>& value) -> std::filesystem::path {
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                return std::filesystem::path(std::wstring(value.GetData(), value.Length()));
            } else {
                return std::filesystem::path(std::string(value.GetData(), value.Length()));
            }
        }

        auto ToPath(const FString& value) -> std::filesystem::path {
            return ToPathImpl(value);
        }

        void AppendQuotedArg(std::wstring& cmd, const std::wstring& arg) {
            bool needsQuotes = arg.find_first_of(L" \t\"") != std::wstring::npos;
            if (!needsQuotes) {
                if (!cmd.empty()) {
                    cmd.append(L" ");
                }
                cmd.append(arg);
                return;
            }

            if (!cmd.empty()) {
                cmd.append(L" ");
            }
            cmd.push_back(L'"');
            for (wchar_t ch : arg) {
                if (ch == L'"') {
                    cmd.append(L"\\\"");
                } else {
                    cmd.push_back(ch);
                }
            }
            cmd.push_back(L'"');
        }
    } // namespace

    void AppendDiagnosticLine(FString& diagnostics, const TChar* line) {
        if ((line == nullptr) || (line[0] == static_cast<TChar>(0))) {
            return;
        }
        if (!diagnostics.IsEmptyString()) {
            diagnostics.Append(TEXT("\n"));
        }
        diagnostics.Append(line);
    }

    void AppendDiagnostic(FString& diagnostics, const FString& text) {
        if (text.IsEmptyString()) {
            return;
        }
        if (!diagnostics.IsEmptyString()) {
            diagnostics.Append(TEXT("\n"));
        }
        diagnostics.Append(text.GetData(), text.Length());
    }

    auto RunProcess(const FString& exePath, const TVector<FString>& args) -> FProcessOutput {
        FProcessOutput output;
#if AE_PLATFORM_WIN
        std::wstring commandLine;
        AppendQuotedArg(commandLine, ToWideString(exePath));
        for (const auto& arg : args) {
            AppendQuotedArg(commandLine, ToWideString(arg));
        }

        SECURITY_ATTRIBUTES sa{};
        sa.nLength        = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE readPipe  = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            AppendDiagnosticLine(output.mOutput, TEXT("Failed to create process pipes."));
            return output;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW startup{};
        startup.cb          = sizeof(startup);
        startup.dwFlags     = STARTF_USESTDHANDLES;
        startup.hStdOutput  = writePipe;
        startup.hStdError   = writePipe;
        startup.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION processInfo{};
        std::wstring        mutableCmd = commandLine;
        if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                nullptr, nullptr, &startup, &processInfo)) {
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            AppendDiagnosticLine(output.mOutput, TEXT("Failed to launch compiler process."));
            return output;
        }

        CloseHandle(writePipe);

        std::string buffer;
        char        chunk[4096];
        DWORD       bytesRead = 0;
        while (ReadFile(readPipe, chunk, sizeof(chunk), &bytesRead, nullptr) && bytesRead > 0) {
            buffer.append(chunk, chunk + bytesRead);
        }
        CloseHandle(readPipe);

        WaitForSingleObject(processInfo.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);

        output.mExitCode  = static_cast<u32>(exitCode);
        output.mSucceeded = (exitCode == 0);
        output.mOutput    = FromUtf8(buffer);
#else
        (void)exePath;
        (void)args;
        AppendDiagnosticLine(output.mOutput, TEXT("Process execution not supported on this platform."));
#endif
        return output;
    }

    auto ReadFileBytes(const FString& path, TVector<u8>& outBytes) -> bool {
        outBytes.Clear();
        std::error_code ec;
        const auto      fsPath = ToPath(path);
        if (!std::filesystem::exists(fsPath, ec)) {
            return false;
        }

        std::ifstream file(fsPath, std::ios::binary);
        if (!file) {
            return false;
        }

        file.seekg(0, std::ios::end);
        const auto endPos = file.tellg();
        if (endPos < 0) {
            return false;
        }
        const auto size = static_cast<usize>(endPos);
        file.seekg(0, std::ios::beg);
        outBytes.Resize(size);
        if (size > 0) {
            file.read(reinterpret_cast<char*>(outBytes.Data()), static_cast<std::streamsize>(size));
        }
        return file.good() || file.eof();
    }

    auto ReadFileTextUtf8(const FString& path, FNativeString& outText) -> bool {
        outText.Clear();
        std::error_code ec;
        const auto      fsPath = ToPath(path);
        if (!std::filesystem::exists(fsPath, ec)) {
            return false;
        }

        std::ifstream file(fsPath, std::ios::binary);
        if (!file) {
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        if (!content.empty()) {
            outText.Append(content.c_str(), content.size());
        }
        return file.good() || file.eof();
    }

    auto BuildTempOutputPath(const FString& sourcePath, const FString& suffix,
        const FString& extension) -> FString {
        static std::atomic<u32> counter{ 0 };
        std::error_code         ec;

        std::filesystem::path dir;
        try {
            dir = std::filesystem::temp_directory_path();
        } catch (...) {
            dir = std::filesystem::current_path(ec);
        }

        dir /= "AltinaEngine";
        dir /= "ShaderCompile";
        std::filesystem::create_directories(dir, ec);

        auto basePath = ToPath(sourcePath);
        auto stem     = basePath.stem();
        if (stem.empty()) {
            stem = std::filesystem::path("shader");
        }

        const auto uniqueId = counter.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path filename = stem;
        filename += std::filesystem::path("_");
        filename += std::filesystem::path(std::to_string(uniqueId));
        filename += std::filesystem::path("_");
        filename += ToPath(suffix);
        filename += ToPath(extension);

        return FromPath(dir / filename);
    }

    void RemoveFileIfExists(const FString& path) {
        std::error_code ec;
        std::filesystem::remove(ToPath(path), ec);
    }

} // namespace AltinaEngine::ShaderCompiler::Detail
