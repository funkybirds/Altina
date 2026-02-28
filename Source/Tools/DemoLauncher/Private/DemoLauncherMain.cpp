#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace {
#if defined(_WIN32)
    static void AppendQuotedArg(std::wstring& cmd, const std::wstring& arg) {
        const bool needsQuotes = (arg.find_first_of(L" \t\"") != std::wstring::npos) || arg.empty();

        if (!cmd.empty()) {
            cmd.push_back(L' ');
        }

        if (!needsQuotes) {
            cmd.append(arg);
            return;
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

    static bool FileExists(const std::wstring& path) {
        const DWORD attrs = GetFileAttributesW(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
    }

    static std::wstring GetModulePath() {
        std::wstring path;
        path.resize(32768);
        const DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (len == 0 || len >= path.size()) {
            return {};
        }
        path.resize(len);
        return path;
    }

    static void SplitDirAndFile(
        const std::wstring& fullPath, std::wstring& outDir, std::wstring& outFile) {
        const size_t slash = fullPath.find_last_of(L"\\/");
        if (slash == std::wstring::npos) {
            outDir.clear();
            outFile = fullPath;
            return;
        }
        outDir  = fullPath.substr(0, slash);
        outFile = fullPath.substr(slash + 1);
    }

    static std::wstring GetStem(const std::wstring& fileName) {
        const size_t dot = fileName.find_last_of(L'.');
        if (dot == std::wstring::npos) {
            return fileName;
        }
        return fileName.substr(0, dot);
    }
#endif
} // namespace

int wmain() {
#if !defined(_WIN32)
    std::fprintf(stderr, "AltinaEngineDemoLauncher: unsupported platform.\n");
    return 1;
#else
    const std::wstring launcherPath = GetModulePath();
    if (launcherPath.empty()) {
        std::fprintf(stderr, "AltinaEngineDemoLauncher: failed to resolve module path.\n");
        return 2;
    }

    std::wstring exeDir;
    std::wstring exeFileName;
    SplitDirAndFile(launcherPath, exeDir, exeFileName);

    const std::wstring stem        = GetStem(exeFileName);
    const std::wstring dataDirName = stem + L"_Data";

    std::wstring       dataDir = exeDir;
    if (!dataDir.empty()) {
        dataDir.push_back(L'\\');
    }
    dataDir.append(dataDirName);

    std::wstring childExePath = dataDir;
    childExePath.push_back(L'\\');
    childExePath.append(exeFileName);

    if (!FileExists(childExePath)) {
        std::fwprintf(stderr, L"AltinaEngineDemoLauncher: inner executable not found: '%ls'\n",
            childExePath.c_str());
        return 3;
    }

    int       argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        std::fprintf(stderr, "AltinaEngineDemoLauncher: failed to parse command line.\n");
        return 4;
    }

    // Build child command line: "<childExePath>" + original args (excluding argv[0]).
    std::wstring cmdLine;
    AppendQuotedArg(cmdLine, childExePath);
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }
        AppendQuotedArg(cmdLine, argv[i]);
    }
    LocalFree(argv);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION  pi{};

    // Mutable buffer required by CreateProcessW.
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    const BOOL ok = CreateProcessW(
        /*lpApplicationName*/ nullptr,
        /*lpCommandLine*/ cmdBuf.data(),
        /*lpProcessAttributes*/ nullptr,
        /*lpThreadAttributes*/ nullptr,
        /*bInheritHandles*/ FALSE,
        /*dwCreationFlags*/ 0,
        /*lpEnvironment*/ nullptr,
        /*lpCurrentDirectory*/ dataDir.c_str(),
        /*lpStartupInfo*/ &si,
        /*lpProcessInformation*/ &pi);

    if (!ok) {
        const DWORD err = GetLastError();
        std::fwprintf(stderr,
            L"AltinaEngineDemoLauncher: failed to launch inner exe (err=%lu): '%ls'\n",
            static_cast<unsigned long>(err), childExePath.c_str());
        return 5;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return static_cast<int>(exitCode);
#endif
}
