#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #include <shellapi.h>
#endif

#include "Utility/Json.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {
    using AltinaEngine::Core::Container::FNativeString;
    using AltinaEngine::Core::Container::FNativeStringView;
    using AltinaEngine::Core::Utility::Json::EJsonType;
    using AltinaEngine::Core::Utility::Json::FindObjectValue;
    using AltinaEngine::Core::Utility::Json::FJsonDocument;
    using AltinaEngine::Core::Utility::Json::FJsonValue;
    using AltinaEngine::Core::Utility::Json::GetBoolValue;
    using AltinaEngine::Core::Utility::Json::GetNumberValue;
    using AltinaEngine::Core::Utility::Json::GetStringValue;

    struct Loc {
        std::string File;
        int         Line = 0;
        int         Col  = 0;
    };

    using ArgValue = std::variant<bool, int64_t, double, std::string>;

    struct ArgPair {
        std::string Key;
        ArgValue    Value;
    };

    struct AnnotationInfo {
        std::string              Raw;
        std::string              Tag;
        std::vector<ArgPair>     Args;
        std::vector<std::string> Errors;
    };

    struct AnnotationEntry {
        std::string              DeclKind;
        std::string              DeclNodeKind;
        std::string              DeclName;
        std::string              QualifiedName;
        std::string              OwnerName;
        std::string              OwnerQualifiedName;
        Loc                      Location;
        std::string              Annotation;
        std::string              Tag;
        std::vector<ArgPair>     Args;
        std::vector<std::string> Errors;
    };

    struct FileResult {
        std::string                  File;
        std::string                  CompilerMode;
        std::vector<std::string>     Errors;
        std::vector<AnnotationEntry> Annotations;
    };

    struct ClassRecord {
        std::string                  QualifiedName;
        std::string                  Include;
        Loc                          Location;
        bool                         HasClassAnnotation = false;
        std::string                  ClassAnnotation;
        bool                         IsAbstract = false;
        std::vector<AnnotationEntry> Properties;
        std::vector<AnnotationEntry> Methods;
    };

    struct Options {
        std::string              CompileCommands;
        std::vector<std::string> Files;
        std::string              Compiler;
        std::vector<std::string> ExtraArgs;
        bool                     IncludeHeaders = false;
        int                      MaxFiles       = 0;
        std::string              OutFile;
        std::string              ModuleName;
        std::string              ModuleRoot;
        std::string              GenCpp;
        bool                     ForbidAnnotations = false;
        bool                     Strict            = false;
        bool                     Verbose           = false;
    };

    struct CompileCommand {
        std::string              File;
        std::string              Directory;
        std::vector<std::string> Arguments;
    };

    struct ProcessResult {
        int         ExitCode = -1;
        std::string Output;
        std::string Error;
        bool        Ran = false;
    };

    static auto ToStdString(const FNativeString& value) -> std::string {
        return std::string(value.GetData(), value.Length());
    }

    static auto ToStdString(FNativeStringView view) -> std::string {
        return std::string(view.Data(), view.Length());
    }

    static auto IsSameLocation(const Loc& lhs, const Loc& rhs) -> bool {
        return lhs.Line == rhs.Line && lhs.Col == rhs.Col && lhs.File == rhs.File;
    }

    static auto ReadFileText(const std::filesystem::path& path, std::string& out) -> bool {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return false;
        }
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        out = buffer.str();
        return true;
    }

    static auto Trim(std::string_view value) -> std::string_view {
        size_t start = 0;
        size_t end   = value.size();

        while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) {
            ++start;
        }
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(start, end - start);
    }

    static auto StartsWith(std::string_view value, std::string_view prefix) -> bool {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    static auto EndsWith(std::string_view value, std::string_view suffix) -> bool {
        return value.size() >= suffix.size()
            && value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
    }

    static auto NormalizePath(const std::string& path) -> std::string {
        std::error_code       ec;
        std::filesystem::path p(path);
        auto                  abs    = std::filesystem::absolute(p, ec);
        auto                  norm   = ec ? p.lexically_normal() : abs.lexically_normal();
        std::string           result = norm.string();
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return result;
    }

    static auto IsUnderRoot(const std::string& path, const std::string& root) -> bool {
        if (root.empty()) {
            return true;
        }
        std::string normPath = NormalizePath(path);
        std::string normRoot = NormalizePath(root);
        if (!normRoot.empty()) {
            char last = normRoot.back();
            if (last != '\\' && last != '/') {
                normRoot.push_back(std::filesystem::path::preferred_separator);
            }
        }
        return normPath.rfind(normRoot, 0) == 0;
    }

    static auto IsHeaderExtension(const std::filesystem::path& path) -> bool {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx" || ext == ".inl";
    }

    static auto MakeIncludePath(const std::string& file, const std::string& moduleRoot,
        std::string& out, std::string& error) -> bool {
        if (!IsUnderRoot(file, moduleRoot)) {
            error = "Declaration is outside module root";
            return false;
        }

        std::filesystem::path p(file);
        if (!IsHeaderExtension(p)) {
            error = "Annotated declaration is not in a header file";
            return false;
        }

        std::string generic = p.generic_string();
        std::string lower   = generic;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        const char* publicTag  = "/public/";
        const char* privateTag = "/private/";
        size_t      pos        = lower.find(publicTag);
        size_t      baseLen    = 0;
        if (pos != std::string::npos) {
            baseLen = std::strlen(publicTag);
        } else {
            pos = lower.find(privateTag);
            if (pos != std::string::npos) {
                baseLen = std::strlen(privateTag);
            }
        }

        if (pos == std::string::npos || baseLen == 0) {
            error = "Header is not under a Public/ or Private/ folder";
            return false;
        }

        std::string include = generic.substr(pos + baseLen);
        if (include.empty()) {
            error = "Failed to compute include path";
            return false;
        }

        out = include;
        return true;
    }

    static auto GetEnvVar(const char* name) -> std::optional<std::string> {
#ifdef _MSC_VER
        char*  buffer = nullptr;
        size_t size   = 0;
        if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
            return std::nullopt;
        }
        std::string value(buffer);
        std::free(buffer);
        if (value.empty()) {
            return std::nullopt;
        }
        return value;
#else
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return std::nullopt;
        }
        return std::string(value);
#endif
    }

    static auto ExistsPath(const std::filesystem::path& path) -> bool {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    static auto FindClangClFromEnv() -> std::optional<std::string> {
        if (auto vcTools = GetEnvVar("VCToolsInstallDir")) {
            std::filesystem::path base(*vcTools);
            auto candidate = base / ".." / ".." / "Llvm" / "x64" / "bin" / "clang-cl.exe";
            if (ExistsPath(candidate)) {
                return candidate.lexically_normal().string();
            }
        }

        if (auto vcInstall = GetEnvVar("VCINSTALLDIR")) {
            std::filesystem::path base(*vcInstall);
            auto candidate = base / "Tools" / "Llvm" / "x64" / "bin" / "clang-cl.exe";
            if (ExistsPath(candidate)) {
                return candidate.lexically_normal().string();
            }
        }

        if (auto vsInstall = GetEnvVar("VSINSTALLDIR")) {
            std::filesystem::path base(*vsInstall);
            auto candidate = base / "VC" / "Tools" / "Llvm" / "x64" / "bin" / "clang-cl.exe";
            if (ExistsPath(candidate)) {
                return candidate.lexically_normal().string();
            }
        }

        return std::nullopt;
    }

#ifdef _WIN32
    static auto Utf8ToWide(const std::string& value) -> std::wstring {
        if (value.empty()) {
            return std::wstring();
        }
        int size = MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (size <= 0) {
            return std::wstring();
        }
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
        return out;
    }

    static auto WideToUtf8(const std::wstring& value) -> std::string {
        if (value.empty()) {
            return std::string();
        }
        int size = WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) {
            return std::string();
        }
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(),
            size, nullptr, nullptr);
        return out;
    }

    static auto QuoteArg(const std::wstring& arg) -> std::wstring {
        if (arg.empty()) {
            return L"\"\"";
        }

        bool needsQuotes = arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
        if (!needsQuotes) {
            return arg;
        }

        std::wstring out;
        out.push_back(L'\"');
        size_t backslashes = 0;
        for (wchar_t ch : arg) {
            if (ch == L'\\') {
                ++backslashes;
                continue;
            }
            if (ch == L'\"') {
                out.append(backslashes * 2 + 1, L'\\');
                out.push_back(L'\"');
                backslashes = 0;
                continue;
            }
            if (backslashes > 0) {
                out.append(backslashes, L'\\');
                backslashes = 0;
            }
            out.push_back(ch);
        }
        if (backslashes > 0) {
            out.append(backslashes * 2, L'\\');
        }
        out.push_back(L'\"');
        return out;
    }

    static auto BuildCommandLine(const std::vector<std::string>& args) -> std::wstring {
        std::wstring commandLine;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                commandLine.push_back(L' ');
            }
            commandLine += QuoteArg(Utf8ToWide(args[i]));
        }
        return commandLine;
    }

    static auto RunProcess(const std::vector<std::string>& args,
        const std::filesystem::path&                       workingDir) -> ProcessResult {
        ProcessResult       result;

        SECURITY_ATTRIBUTES sa  = {};
        sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle       = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE readPipe  = nullptr;
        HANDLE writePipe = nullptr;

        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            result.Error = "Failed to create pipe";
            return result;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        std::wstring cmdLine = BuildCommandLine(args);
        std::wstring workDir = Utf8ToWide(workingDir.string());

        STARTUPINFOW si = {};
        si.cb           = sizeof(STARTUPINFOW);
        si.dwFlags      = STARTF_USESTDHANDLES;
        si.hStdOutput   = writePipe;
        si.hStdError    = writePipe;
        si.hStdInput    = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi = {};

        BOOL ok = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
            nullptr, workDir.empty() ? nullptr : workDir.c_str(), &si, &pi);

        if (!ok) {
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            result.Error = "Failed to launch compiler";
            return result;
        }

        CloseHandle(writePipe);

        std::string     output;
        constexpr DWORD kBufferSize = 4096;
        char            buffer[kBufferSize];
        DWORD           bytesRead = 0;

        while (ReadFile(readPipe, buffer, kBufferSize, &bytesRead, nullptr) && bytesRead > 0) {
            output.append(buffer, buffer + bytesRead);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(readPipe);

        result.ExitCode = static_cast<int>(exitCode);
        result.Output   = output;
        result.Ran      = true;
        return result;
    }

    static auto SplitCommandLine(const std::string& command) -> std::vector<std::string> {
        std::vector<std::string> args;
        if (command.empty()) {
            return args;
        }

        std::wstring wcmd = Utf8ToWide(command);
        int          argc = 0;
        LPWSTR*      argv = CommandLineToArgvW(wcmd.c_str(), &argc);
        if (!argv) {
            return args;
        }

        args.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            args.push_back(WideToUtf8(argv[i]));
        }
        LocalFree(argv);
        return args;
    }
#else
    static auto RunProcess(const std::vector<std::string>& args,
        const std::filesystem::path&                       workingDir) -> ProcessResult {
        ProcessResult result;
        result.Error = "Process execution not implemented on this platform";
        return result;
    }

    static auto SplitCommandLine(const std::string& command) -> std::vector<std::string> {
        std::vector<std::string> args;
        std::istringstream       iss(command);
        std::string              token;
        while (iss >> token) {
            args.push_back(token);
        }
        return args;
    }
#endif

    static auto FindCompileCommands(const std::string& path) -> std::optional<std::string> {
        if (path.empty()) {
            std::filesystem::path candidate =
                std::filesystem::current_path() / "compile_commands.json";
            if (std::filesystem::exists(candidate)) {
                return candidate.string();
            }
            return std::nullopt;
        }

        std::filesystem::path input(path);
        if (std::filesystem::is_directory(input)) {
            auto candidate = input / "compile_commands.json";
            if (std::filesystem::exists(candidate)) {
                return candidate.string();
            }
            return std::nullopt;
        }

        if (!std::filesystem::exists(input)) {
            return std::nullopt;
        }
        return input.string();
    }

    static auto GetStringField(const FJsonValue& object, const char* key, std::string& out)
        -> bool {
        const FJsonValue* value = FindObjectValue(object, key);
        if (value == nullptr || value->Type != EJsonType::String) {
            return false;
        }
        out = ToStdString(value->String);
        return true;
    }

    static auto GetBoolField(const FJsonValue& object, const char* key, bool& out) -> bool {
        const FJsonValue* value = FindObjectValue(object, key);
        if (value == nullptr || value->Type != EJsonType::Bool) {
            return false;
        }
        out = value->Bool;
        return true;
    }

    static auto GetObjectField(const FJsonValue& object, const char* key) -> const FJsonValue* {
        const FJsonValue* value = FindObjectValue(object, key);
        if (value == nullptr || value->Type != EJsonType::Object) {
            return nullptr;
        }
        return value;
    }

    static auto GetArrayField(const FJsonValue& object, const char* key) -> const FJsonValue* {
        const FJsonValue* value = FindObjectValue(object, key);
        if (value == nullptr || value->Type != EJsonType::Array) {
            return nullptr;
        }
        return value;
    }

    static auto ParseCompileCommands(
        const std::string& text, std::vector<CompileCommand>& out, std::string& error) -> bool {
        FJsonDocument doc;
        if (!doc.Parse(FNativeStringView(text.data(), text.size()))) {
            error = "Failed to parse compile_commands.json: " + ToStdString(doc.GetError());
            return false;
        }

        const FJsonValue* root = doc.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Array) {
            error = "compile_commands.json root is not an array";
            return false;
        }

        for (const FJsonValue* entry : root->Array) {
            if (entry == nullptr || entry->Type != EJsonType::Object) {
                continue;
            }

            CompileCommand cmd;
            if (!GetStringField(*entry, "file", cmd.File)) {
                continue;
            }
            GetStringField(*entry, "directory", cmd.Directory);

            const FJsonValue* arguments = GetArrayField(*entry, "arguments");
            if (arguments != nullptr) {
                for (const FJsonValue* arg : arguments->Array) {
                    if (arg != nullptr && arg->Type == EJsonType::String) {
                        cmd.Arguments.push_back(ToStdString(arg->String));
                    }
                }
            } else {
                std::string command;
                if (!GetStringField(*entry, "command", command)) {
                    continue;
                }
                cmd.Arguments = SplitCommandLine(command);
            }

            if (!cmd.Arguments.empty()) {
                out.push_back(std::move(cmd));
            }
        }

        return true;
    }

    static auto StripOutputArgs(const std::vector<std::string>& args, const std::string& sourceFile)
        -> std::vector<std::string> {
        std::vector<std::string> out;
        bool                     skipNext   = false;
        std::string              sourceNorm = NormalizePath(sourceFile);

        for (const auto& arg : args) {
            if (skipNext) {
                skipNext = false;
                continue;
            }

            if (arg == "-c" || arg == "/c") {
                continue;
            }

            if (arg == "-o" || arg == "/Fo" || arg == "/Fa" || arg == "/Fd" || arg == "/Fe"
                || arg == "/Fp" || arg == "/Fi" || arg == "/FR") {
                skipNext = true;
                continue;
            }

            if (StartsWith(arg, "-o") && arg.size() > 2) {
                continue;
            }
            if (StartsWith(arg, "/Fo") || StartsWith(arg, "/Fa") || StartsWith(arg, "/Fd")
                || StartsWith(arg, "/Fe") || StartsWith(arg, "/Fp") || StartsWith(arg, "/Fi")
                || StartsWith(arg, "/FR")) {
                continue;
            }

            if (NormalizePath(arg) == sourceNorm) {
                continue;
            }

            out.push_back(arg);
        }

        return out;
    }

    static auto ResolveCompiler(const Options& options) -> std::string {
        if (!options.Compiler.empty()) {
            return options.Compiler;
        }
        if (auto clang = FindClangClFromEnv()) {
            return *clang;
        }
        return "clang-cl";
    }

    static auto CompilerMode(const std::string& compiler) -> std::string {
        std::string base = std::filesystem::path(compiler).filename().string();
        std::transform(base.begin(), base.end(), base.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (base.find("clang-cl") != std::string::npos) {
            return "clang-cl";
        }
        return "clang";
    }

    static auto BuildCompilerCommand(const CompileCommand& entry, const Options& options,
        std::string& outMode, std::string& error) -> std::vector<std::string> {
        if (entry.Arguments.empty()) {
            error = "Empty compile command";
            return {};
        }

        std::string compiler  = ResolveCompiler(options);
        std::string baseName  = std::filesystem::path(compiler).filename().string();
        std::string lowerName = baseName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lowerName.find("clang") == std::string::npos) {
            error = "Compiler is not clang/clang-cl. Pass --compiler clang-cl.";
            return {};
        }

        outMode = CompilerMode(compiler);

        std::vector<std::string> baseArgs(entry.Arguments.begin() + 1, entry.Arguments.end());
        baseArgs = StripOutputArgs(baseArgs, entry.File);

        std::vector<std::string> astArgs;
        if (outMode == "clang-cl") {
            astArgs = {
                "/clang:-Xclang",
                "/clang:-ast-dump=json",
                "/clang:-fsyntax-only",
                "/clang:-Wno-unknown-attributes",
            };
        } else {
            astArgs = {
                "-Xclang",
                "-ast-dump=json",
                "-fsyntax-only",
                "-Wno-unknown-attributes",
            };
        }

        std::vector<std::string> cmd;
        cmd.reserve(1 + baseArgs.size() + astArgs.size() + options.ExtraArgs.size() + 1);
        cmd.push_back(compiler);
        cmd.insert(cmd.end(), baseArgs.begin(), baseArgs.end());
        cmd.insert(cmd.end(), astArgs.begin(), astArgs.end());
        cmd.insert(cmd.end(), options.ExtraArgs.begin(), options.ExtraArgs.end());
        cmd.push_back(entry.File);
        return cmd;
    }

    static auto ParseValue(const std::string& token) -> ArgValue {
        if (token == "true" || token == "True" || token == "TRUE") {
            return true;
        }
        if (token == "false" || token == "False" || token == "FALSE") {
            return false;
        }

        char*     end      = nullptr;
        long long intValue = std::strtoll(token.c_str(), &end, 10);
        if (end != token.c_str() && *end == '\0') {
            return static_cast<int64_t>(intValue);
        }

        end               = nullptr;
        double floatValue = std::strtod(token.c_str(), &end);
        if (end != token.c_str() && *end == '\0') {
            return floatValue;
        }

        return token;
    }

    static auto ParseArgs(const std::string& text, std::vector<ArgPair>& outArgs,
        std::vector<std::string>& errors) -> void {
        size_t       i = 0;
        const size_t n = text.size();

        auto         skipWs = [&]() {
            while (i < n && std::isspace(static_cast<unsigned char>(text[i]))) {
                ++i;
            }
        };

        auto parseIdent = [&]() -> std::optional<std::string> {
            if (i >= n || !(std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_')) {
                return std::nullopt;
            }
            size_t start = i;
            ++i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) {
                ++i;
            }
            return text.substr(start, i - start);
        };

        auto parseString = [&](char quote) -> std::string {
            ++i;
            std::string out;
            while (i < n) {
                char ch = text[i];
                if (ch == '\\\\') {
                    ++i;
                    if (i >= n) {
                        break;
                    }
                    char esc = text[i];
                    if (esc == 'n') {
                        out.push_back('\n');
                    } else if (esc == 't') {
                        out.push_back('\t');
                    } else {
                        out.push_back(esc);
                    }
                    ++i;
                    continue;
                }
                if (ch == quote) {
                    ++i;
                    return out;
                }
                out.push_back(ch);
                ++i;
            }
            errors.push_back("Unterminated string literal");
            return out;
        };

        auto upsertArg = [&](const std::string& key, ArgValue value) {
            for (auto& arg : outArgs) {
                if (arg.Key == key) {
                    arg.Value = std::move(value);
                    return;
                }
            }
            outArgs.push_back({ key, std::move(value) });
        };

        while (true) {
            skipWs();
            if (i >= n) {
                break;
            }

            auto key = parseIdent();
            if (!key) {
                errors.push_back("Expected identifier at position " + std::to_string(i));
                break;
            }

            skipWs();
            if (i < n && text[i] == '=') {
                ++i;
                skipWs();
                if (i >= n) {
                    errors.push_back("Expected value after '='");
                    break;
                }

                ArgValue value;
                if (text[i] == '\'' || text[i] == '\"') {
                    char quote = text[i];
                    value      = parseString(quote);
                } else {
                    size_t start = i;
                    while (i < n && text[i] != ',' && text[i] != ')') {
                        if (std::isspace(static_cast<unsigned char>(text[i]))) {
                            break;
                        }
                        ++i;
                    }
                    std::string token = text.substr(start, i - start);
                    if (token.empty()) {
                        errors.push_back("Expected value after '='");
                        break;
                    }
                    value = ParseValue(token);
                }
                upsertArg(*key, std::move(value));
            } else {
                upsertArg(*key, true);
            }

            skipWs();
            if (i >= n) {
                break;
            }
            if (text[i] == ',') {
                ++i;
                continue;
            }
            errors.push_back("Unexpected character '" + std::string(1, text[i]) + "' at position "
                + std::to_string(i));
            break;
        }
    }

    static auto ParseAnnotation(const std::string& annotation) -> std::optional<AnnotationInfo> {
        std::string_view trimmed = Trim(annotation);
        if (!StartsWith(trimmed, "AE.")) {
            return std::nullopt;
        }

        size_t         lparen = trimmed.find('(');
        size_t         rparen = trimmed.rfind(')');
        AnnotationInfo info;
        info.Raw = std::string(trimmed);

        if (lparen == std::string_view::npos || rparen == std::string_view::npos
            || rparen < lparen) {
            info.Errors.push_back("Annotation does not match AE.<Tag>(...)");
            return info;
        }

        std::string_view tag = Trim(trimmed.substr(3, lparen - 3));
        if (tag.empty()) {
            info.Errors.push_back("Missing annotation tag");
        }
        info.Tag = std::string(tag);

        std::string_view argsText = trimmed.substr(lparen + 1, rparen - lparen - 1);
        if (!argsText.empty()) {
            ParseArgs(std::string(argsText), info.Args, info.Errors);
        }

        std::string_view tail = Trim(trimmed.substr(rparen + 1));
        if (!tail.empty()) {
            info.Errors.push_back("Unexpected trailing characters after ')'");
        }

        return info;
    }

    static auto MapDeclKind(const std::string& kind) -> std::string {
        if (kind == "CXXRecordDecl" || kind == "RecordDecl"
            || kind == "ClassTemplateSpecializationDecl") {
            return "class";
        }
        if (kind == "FieldDecl" || kind == "VarDecl") {
            return "property";
        }
        if (kind == "CXXMethodDecl" || kind == "FunctionDecl" || kind == "FunctionTemplateDecl"
            || kind == "CXXConstructorDecl") {
            return "function";
        }
        return std::string();
    }

    static auto CollectStrings(const FJsonValue& node, std::vector<std::string>& out) -> void {
        if (node.Type == EJsonType::String) {
            out.push_back(ToStdString(node.String));
            return;
        }
        if (node.Type == EJsonType::Array) {
            for (const FJsonValue* child : node.Array) {
                if (child) {
                    CollectStrings(*child, out);
                }
            }
            return;
        }
        if (node.Type == EJsonType::Object) {
            for (const auto& pair : node.Object) {
                if (pair.Value) {
                    CollectStrings(*pair.Value, out);
                }
            }
        }
    }

    static auto GetLocFromObject(const FJsonValue& object, Loc& loc) -> bool {
        if (object.Type != EJsonType::Object) {
            return false;
        }

        GetStringField(object, "file", loc.File);

        const FJsonValue* lineValue = FindObjectValue(object, "line");
        if (lineValue && lineValue->Type == EJsonType::Number) {
            loc.Line = static_cast<int>(lineValue->Number);
        }

        const FJsonValue* colValue = FindObjectValue(object, "col");
        if (colValue && colValue->Type == EJsonType::Number) {
            loc.Col = static_cast<int>(colValue->Number);
        }

        return !loc.File.empty();
    }

    static auto GetAttrExpansionLoc(const FJsonValue& attr) -> Loc {
        Loc loc;
        if (const FJsonValue* range = GetObjectField(attr, "range")) {
            if (const FJsonValue* begin = GetObjectField(*range, "begin")) {
                if (const FJsonValue* expansion = GetObjectField(*begin, "expansionLoc")) {
                    if (GetLocFromObject(*expansion, loc)) {
                        return loc;
                    }
                }
                if (const FJsonValue* spelling = GetObjectField(*begin, "spellingLoc")) {
                    if (GetLocFromObject(*spelling, loc)) {
                        return loc;
                    }
                }
            }
        }
        return loc;
    }

    static auto GetFileTextCached(const std::string& path, std::string& out) -> bool {
        static std::unordered_map<std::string, std::string> cache;
        std::string                                         key = NormalizePath(path);
        auto                                                it  = cache.find(key);
        if (it != cache.end()) {
            out = it->second;
            return true;
        }
        if (!ReadFileText(path, out)) {
            return false;
        }
        cache.emplace(std::move(key), out);
        return true;
    }

    static auto GetOffsetForLineCol(const std::string& text, int line, int col)
        -> std::optional<size_t> {
        if (line <= 0 || col <= 0) {
            return std::nullopt;
        }
        size_t currentLine = 1;
        size_t offset      = 0;
        while (offset < text.size() && currentLine < static_cast<size_t>(line)) {
            if (text[offset] == '\n') {
                ++currentLine;
            }
            ++offset;
        }
        if (currentLine != static_cast<size_t>(line)) {
            return std::nullopt;
        }
        size_t lineStart = offset;
        size_t pos       = lineStart + static_cast<size_t>(col - 1);
        if (pos > text.size()) {
            return std::nullopt;
        }
        return pos;
    }

    static auto IsIdentStart(char ch) -> bool {
        return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
    }

    static auto IsIdentChar(char ch) -> bool {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
    }

    static auto TryExtractMacroInvocation(
        const std::string& text, size_t offset, std::string& macro, std::string& args) -> bool {
        size_t i = offset;
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
            if (text[i] == '\n') {
                return false;
            }
            ++i;
        }
        if (i >= text.size() || !IsIdentStart(text[i])) {
            return false;
        }
        size_t identStart = i;
        ++i;
        while (i < text.size() && IsIdentChar(text[i])) {
            ++i;
        }
        macro = text.substr(identStart, i - identStart);
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i >= text.size() || text[i] != '(') {
            return false;
        }
        size_t argsStart = i + 1;
        int    depth     = 1;
        bool   inString  = false;
        char   quote     = '\0';
        for (i = argsStart; i < text.size(); ++i) {
            char ch = text[i];
            if (inString) {
                if (ch == '\\' && i + 1 < text.size()) {
                    ++i;
                    continue;
                }
                if (ch == quote) {
                    inString = false;
                }
                continue;
            }
            if (ch == '"' || ch == '\'') {
                inString = true;
                quote    = ch;
                continue;
            }
            if (ch == '(') {
                ++depth;
                continue;
            }
            if (ch == ')') {
                --depth;
                if (depth == 0) {
                    args = text.substr(argsStart, i - argsStart);
                    return true;
                }
            }
        }
        return false;
    }

    static auto GetAnnotationTextFromMacro(const FJsonValue& attr) -> std::optional<std::string> {
        Loc loc = GetAttrExpansionLoc(attr);
        if (loc.File.empty() || loc.Line <= 0 || loc.Col <= 0) {
            return std::nullopt;
        }
        std::string fileText;
        if (!GetFileTextCached(loc.File, fileText)) {
            return std::nullopt;
        }
        auto offset = GetOffsetForLineCol(fileText, loc.Line, loc.Col);
        if (!offset.has_value()) {
            return std::nullopt;
        }
        std::string macro;
        std::string args;
        if (!TryExtractMacroInvocation(fileText, *offset, macro, args)) {
            return std::nullopt;
        }
        std::string tag;
        if (macro == "ACLASS") {
            tag = "Class";
        } else if (macro == "APROPERTY") {
            tag = "Property";
        } else if (macro == "AFUNCTION") {
            tag = "Function";
        } else {
            return std::nullopt;
        }
        return "AE." + tag + "(" + args + ")";
    }

    static auto GetAnnotationText(const FJsonValue& attr) -> std::optional<std::string> {
        if (const FJsonValue* annotation = FindObjectValue(attr, "annotation")) {
            if (annotation->Type == EJsonType::String) {
                return ToStdString(annotation->String);
            }
        }
        if (const FJsonValue* value = FindObjectValue(attr, "value")) {
            if (value->Type == EJsonType::String) {
                return ToStdString(value->String);
            }
        }
        if (const FJsonValue* text = FindObjectValue(attr, "text")) {
            if (text->Type == EJsonType::String) {
                return ToStdString(text->String);
            }
        }

        std::vector<std::string> strings;
        CollectStrings(attr, strings);
        for (const auto& entry : strings) {
            if (StartsWith(entry, "AE.")) {
                return entry;
            }
        }

        if (auto fromMacro = GetAnnotationTextFromMacro(attr)) {
            return fromMacro;
        }

        return std::nullopt;
    }

    static auto GetLoc(const FJsonValue& node) -> Loc {
        Loc               loc;
        const FJsonValue* locValue = FindObjectValue(node, "loc");
        if (locValue == nullptr || locValue->Type != EJsonType::Object) {
            return loc;
        }

        GetStringField(*locValue, "file", loc.File);

        const FJsonValue* lineValue = FindObjectValue(*locValue, "line");
        if (lineValue && lineValue->Type == EJsonType::Number) {
            loc.Line = static_cast<int>(lineValue->Number);
        }

        const FJsonValue* colValue = FindObjectValue(*locValue, "col");
        if (colValue && colValue->Type == EJsonType::Number) {
            loc.Col = static_cast<int>(colValue->Number);
        }

        return loc;
    }

    static auto ShouldIncludeNode(
        const FJsonValue& node, const std::string& currentFile, bool includeHeaders) -> bool {
        bool implicit = false;
        if (GetBoolField(node, "implicit", implicit) && implicit) {
            return false;
        }
        if (GetBoolField(node, "isImplicit", implicit) && implicit) {
            return false;
        }

        std::string kind;
        GetStringField(node, "kind", kind);
        if (kind == "CXXRecordDecl" || kind == "RecordDecl") {
            bool isComplete = true;
            if (GetBoolField(node, "isCompleteDefinition", isComplete) && !isComplete) {
                return false;
            }
            bool isDef = true;
            if (GetBoolField(node, "isThisDeclarationADefinition", isDef) && !isDef) {
                return false;
            }
        }

        if (includeHeaders) {
            return true;
        }

        Loc loc = GetLoc(node);
        if (loc.File.empty()) {
            return true;
        }

        return NormalizePath(loc.File) == NormalizePath(currentFile);
    }

    static auto CollectAttrNodes(const FJsonValue& node, std::vector<const FJsonValue*>& out)
        -> void {
        for (const char* key : { "attrs", "attributes", "attr" }) {
            if (const FJsonValue* attrs = GetArrayField(node, key)) {
                for (const FJsonValue* attr : attrs->Array) {
                    if (attr && attr->Type == EJsonType::Object) {
                        out.push_back(attr);
                    }
                }
            } else if (const FJsonValue* attr = GetObjectField(node, key)) {
                out.push_back(attr);
            }
        }

        if (const FJsonValue* inner = GetArrayField(node, "inner")) {
            for (const FJsonValue* child : inner->Array) {
                if (child && child->Type == EJsonType::Object) {
                    std::string kind;
                    if (GetStringField(*child, "kind", kind) && EndsWith(kind, "Attr")) {
                        out.push_back(child);
                    }
                }
            }
        }
    }

    static auto WalkAst(const FJsonValue& node, const std::string& currentFile, bool includeHeaders,
        std::vector<AnnotationEntry>& out, std::vector<std::string>& errors,
        const std::string& currentOwnerName, const std::string& currentOwnerQualified,
        const std::string& currentNamespaceQualified) -> void {
        if (node.Type != EJsonType::Object) {
            return;
        }

        std::string kind;
        GetStringField(node, "kind", kind);
        std::string declKind = MapDeclKind(kind);

        std::string nextOwnerName          = currentOwnerName;
        std::string nextOwnerQualified     = currentOwnerQualified;
        std::string nextNamespaceQualified = currentNamespaceQualified;

        if (kind == "NamespaceDecl") {
            std::string nsName;
            std::string nsQualified;
            GetStringField(node, "name", nsName);
            GetStringField(node, "qualifiedName", nsQualified);
            if (!nsQualified.empty()) {
                nextNamespaceQualified = nsQualified;
            } else if (!nsName.empty()) {
                if (currentNamespaceQualified.empty()) {
                    nextNamespaceQualified = nsName;
                } else {
                    nextNamespaceQualified = currentNamespaceQualified + "::" + nsName;
                }
            }
        }
        if (declKind == "class") {
            std::string name;
            std::string qname;
            GetStringField(node, "name", name);
            GetStringField(node, "qualifiedName", qname);
            if (!name.empty() && (qname.empty() || qname == name)) {
                if (!currentNamespaceQualified.empty()) {
                    qname = currentNamespaceQualified + "::" + name;
                } else {
                    qname = name;
                }
            }
            if (!name.empty()) {
                nextOwnerName = name;
            }
            if (!qname.empty()) {
                nextOwnerQualified = qname;
            } else if (!name.empty()) {
                nextOwnerQualified = name;
            }
        }

        if (ShouldIncludeNode(node, currentFile, includeHeaders)) {
            if (!declKind.empty()) {
                std::vector<const FJsonValue*> attrs;
                CollectAttrNodes(node, attrs);
                for (const FJsonValue* attr : attrs) {
                    if (attr == nullptr || attr->Type != EJsonType::Object) {
                        continue;
                    }
                    std::string attrKind;
                    if (!GetStringField(*attr, "kind", attrKind) || attrKind != "AnnotateAttr") {
                        continue;
                    }

                    auto annotationText = GetAnnotationText(*attr);
                    if (!annotationText || !StartsWith(*annotationText, "AE.")) {
                        continue;
                    }

                    auto parsed = ParseAnnotation(*annotationText);
                    if (!parsed.has_value()) {
                        continue;
                    }

                    AnnotationEntry entry;
                    entry.DeclKind     = declKind;
                    entry.DeclNodeKind = kind;
                    GetStringField(node, "name", entry.DeclName);
                    GetStringField(node, "qualifiedName", entry.QualifiedName);
                    if (entry.QualifiedName.empty()) {
                        entry.QualifiedName = entry.DeclName;
                    }
                    if (declKind == "class" && !currentNamespaceQualified.empty()
                        && entry.QualifiedName == entry.DeclName) {
                        entry.QualifiedName = currentNamespaceQualified + "::" + entry.DeclName;
                    }
                    if (declKind == "class") {
                        entry.OwnerName          = entry.DeclName;
                        entry.OwnerQualifiedName = entry.QualifiedName;
                    } else {
                        entry.OwnerName          = currentOwnerName;
                        entry.OwnerQualifiedName = currentOwnerQualified;
                        if (entry.OwnerQualifiedName.empty()) {
                            entry.Errors.push_back("Missing owning class for annotated member");
                        }
                    }
                    if (!entry.OwnerQualifiedName.empty() && !entry.DeclName.empty()
                        && (entry.QualifiedName.empty() || entry.QualifiedName == entry.DeclName)) {
                        entry.QualifiedName = entry.OwnerQualifiedName + "::" + entry.DeclName;
                    }
                    entry.Location = GetLoc(node);
                    if (entry.Location.File.empty()) {
                        Loc attrLoc = GetAttrExpansionLoc(*attr);
                        if (!attrLoc.File.empty()) {
                            entry.Location = attrLoc;
                        }
                    }
                    entry.Annotation = parsed->Raw;
                    entry.Tag        = parsed->Tag;
                    entry.Args       = parsed->Args;
                    entry.Errors     = parsed->Errors;

                    if (!parsed->Tag.empty()) {
                        std::string expected = parsed->Tag;
                        std::transform(expected.begin(), expected.end(), expected.begin(),
                            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                        if (expected != declKind) {
                            entry.Errors.push_back(
                                "Annotation kind mismatch: " + parsed->Tag + " on " + kind);
                        }
                    }

                    out.push_back(std::move(entry));
                }
            }
        }

        if (const FJsonValue* inner = GetArrayField(node, "inner")) {
            for (const FJsonValue* child : inner->Array) {
                if (child) {
                    WalkAst(*child, currentFile, includeHeaders, out, errors, nextOwnerName,
                        nextOwnerQualified, nextNamespaceQualified);
                }
            }
        }
    }

    static auto ScanFile(const CompileCommand& entry, const Options& options) -> FileResult {
        FileResult result;
        result.File = entry.File;

        std::string mode;
        std::string error;
        auto        command = BuildCompilerCommand(entry, options, mode, error);
        result.CompilerMode = mode;

        if (command.empty()) {
            result.Errors.push_back(error);
            return result;
        }

        if (options.Verbose) {
            std::ostringstream cmdLine;
            for (const auto& arg : command) {
                cmdLine << arg << ' ';
            }
            std::cerr << "[refl-scan] " << cmdLine.str() << "\n";
        }

        std::filesystem::path workDir = entry.Directory.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(entry.Directory);

        ProcessResult         proc = RunProcess(command, workDir);
        if (!proc.Ran) {
            result.Errors.push_back(proc.Error);
            return result;
        }

        if (proc.ExitCode != 0) {
            result.Errors.push_back("clang failed");
            result.Errors.push_back(proc.Output);
            return result;
        }

        FJsonDocument doc;
        if (!doc.Parse(FNativeStringView(proc.Output.data(), proc.Output.size()))) {
            result.Errors.push_back("Failed to parse AST JSON: " + ToStdString(doc.GetError()));
            return result;
        }

        const FJsonValue* root = doc.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Object) {
            result.Errors.push_back("AST root is not an object");
            return result;
        }

        WalkAst(*root, entry.File, options.IncludeHeaders, result.Annotations, result.Errors,
            std::string(), std::string(), std::string());
        return result;
    }

    static auto FormatLoc(const Loc& loc) -> std::string {
        if (loc.File.empty()) {
            return std::string();
        }
        std::ostringstream oss;
        oss << loc.File;
        if (loc.Line > 0) {
            oss << ':' << loc.Line;
            if (loc.Col > 0) {
                oss << ':' << loc.Col;
            }
        }
        return oss.str();
    }

    static auto AppendError(
        std::vector<std::string>& errors, const std::string& message, const Loc& loc) -> void {
        std::string locText = FormatLoc(loc);
        if (!locText.empty()) {
            errors.push_back(message + " (" + locText + ")");
        } else {
            errors.push_back(message);
        }
    }

    static auto EscapeCppString(const std::string& value) -> std::string {
        std::string out;
        out.reserve(value.size());
        for (unsigned char ch : value) {
            switch (ch) {
                case '\\':
                    out += "\\\\";
                    break;
                case '"':
                    out += "\\\"";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (ch < 0x20) {
                        const char* hex = "0123456789ABCDEF";
                        out.push_back('\\');
                        out.push_back('x');
                        out.push_back(hex[(ch >> 4) & 0xF]);
                        out.push_back(hex[ch & 0xF]);
                    } else {
                        out.push_back(static_cast<char>(ch));
                    }
                    break;
            }
        }
        return out;
    }

    static auto SanitizeIdentifier(const std::string& value) -> std::string {
        std::string out;
        out.reserve(value.size());
        for (unsigned char ch : value) {
            if (std::isalnum(ch) || ch == '_') {
                out.push_back(static_cast<char>(ch));
            } else {
                out.push_back('_');
            }
        }
        if (out.empty() || !(std::isalpha(static_cast<unsigned char>(out[0])) || out[0] == '_')) {
            out.insert(out.begin(), '_');
        }
        return out;
    }

    static auto ToGlobalTypeName(const std::string& qualifiedName) -> std::string {
        if (qualifiedName.empty()) {
            return std::string();
        }
        if (StartsWith(qualifiedName, "::")) {
            return qualifiedName;
        }
        return "::" + qualifiedName;
    }

    static auto GenerateCpp(const Options& options, const std::vector<FileResult>& results,
        std::vector<std::string>& outErrors) -> bool {
        if (options.GenCpp.empty()) {
            return true;
        }
        if (options.ModuleName.empty()) {
            outErrors.push_back("Missing --module-name for code generation");
            return false;
        }

        std::map<std::string, ClassRecord> classes;
        for (const auto& result : results) {
            for (const auto& entry : result.Annotations) {
                if (!entry.Errors.empty()) {
                    for (const auto& err : entry.Errors) {
                        AppendError(outErrors, "Annotation error: " + err, entry.Location);
                    }
                    continue;
                }

                if (!options.ModuleRoot.empty()) {
                    if (entry.Location.File.empty()) {
                        AppendError(
                            outErrors, "Missing source location for annotation", entry.Location);
                        continue;
                    }
                    if (!IsUnderRoot(entry.Location.File, options.ModuleRoot)) {
                        continue;
                    }
                }

                if (entry.DeclKind == "class") {
                    if (entry.QualifiedName.empty()) {
                        AppendError(
                            outErrors, "Annotated class has empty qualified name", entry.Location);
                        continue;
                    }
                    auto& record = classes[entry.QualifiedName];
                    if (record.QualifiedName.empty()) {
                        record.QualifiedName = entry.QualifiedName;
                    }
                    if (record.HasClassAnnotation) {
                        if (IsSameLocation(record.Location, entry.Location)) {
                            continue;
                        }
                        if (!record.ClassAnnotation.empty()
                            && record.ClassAnnotation == entry.Annotation) {
                            continue;
                        }
                        AppendError(outErrors,
                            "Duplicate class annotation for " + entry.QualifiedName,
                            entry.Location);
                        continue;
                    }
                    record.HasClassAnnotation = true;
                    record.Location           = entry.Location;
                    record.ClassAnnotation    = entry.Annotation;
                    for (const auto& arg : entry.Args) {
                        if (arg.Key != "Abstract") {
                            continue;
                        }
                        if (const bool* flag = std::get_if<bool>(&arg.Value)) {
                            record.IsAbstract = *flag;
                        }
                        break;
                    }

                    std::string include;
                    std::string includeError;
                    if (!MakeIncludePath(
                            entry.Location.File, options.ModuleRoot, include, includeError)) {
                        AppendError(
                            outErrors, includeError + ": " + entry.QualifiedName, entry.Location);
                    } else {
                        record.Include = include;
                    }
                } else if (entry.DeclKind == "property" || entry.DeclKind == "function") {
                    if (entry.OwnerQualifiedName.empty()) {
                        AppendError(
                            outErrors, "Annotated member missing owning class", entry.Location);
                        continue;
                    }
                    auto& record = classes[entry.OwnerQualifiedName];
                    if (record.QualifiedName.empty()) {
                        record.QualifiedName = entry.OwnerQualifiedName;
                    }
                    if (entry.DeclName.empty()) {
                        AppendError(outErrors, "Annotated member has empty name", entry.Location);
                        continue;
                    }
                    if (entry.DeclKind == "property") {
                        record.Properties.push_back(entry);
                    } else {
                        record.Methods.push_back(entry);
                    }
                } else {
                    AppendError(outErrors, "Unsupported declaration kind: " + entry.DeclKind,
                        entry.Location);
                }
            }
        }

        std::set<std::string>     includeSet;
        std::vector<ClassRecord*> ordered;
        for (auto& pair : classes) {
            ClassRecord& record = pair.second;
            if (!record.HasClassAnnotation) {
                if (!record.Properties.empty() || !record.Methods.empty()) {
                    const Loc* loc = nullptr;
                    if (!record.Properties.empty()) {
                        loc = &record.Properties.front().Location;
                    } else if (!record.Methods.empty()) {
                        loc = &record.Methods.front().Location;
                    }
                    AppendError(outErrors,
                        "Annotated members belong to class without ACLASS: " + record.QualifiedName,
                        loc ? *loc : record.Location);
                }
                continue;
            }
            if (record.Include.empty()) {
                AppendError(outErrors, "Missing include path for class: " + record.QualifiedName,
                    record.Location);
                continue;
            }
            includeSet.insert(record.Include);
            ordered.push_back(&record);
        }

        std::filesystem::path outPath(options.GenCpp);
        std::error_code       ec;
        if (outPath.has_parent_path()) {
            std::filesystem::create_directories(outPath.parent_path(), ec);
            if (ec) {
                outErrors.push_back(
                    "Failed to create output directory: " + outPath.parent_path().string());
                return false;
            }
        }

        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) {
            outErrors.push_back("Failed to write generated file: " + outPath.string());
            return false;
        }

        outFile << "// Auto-generated by ReflectionScanner. Do not edit.\n";
        outFile << "// Module: " << options.ModuleName << "\n";
        outFile << "#include \"Reflection/Reflection.h\"\n";
        outFile << "#if __has_include(\"Engine/GameScene/ComponentRegistry.h\")\n";
        outFile << "#include \"Engine/GameScene/ComponentRegistry.h\"\n";
        outFile << "#include \"Engine/GameScene/World.h\"\n";
        outFile << "#include <type_traits>\n";
        outFile << "#define AE_HAS_COMPONENT_REGISTRY 1\n";
        outFile << "#else\n";
        outFile << "#define AE_HAS_COMPONENT_REGISTRY 0\n";
        outFile << "#endif\n";
        for (const auto& include : includeSet) {
            outFile << "#include \"" << include << "\"\n";
        }

        outFile << "\nnamespace AltinaEngine::Core::Reflection {\n";
        std::string funcName = "RegisterReflection_" + SanitizeIdentifier(options.ModuleName);
        outFile << "void " << funcName << "() {\n";

        for (ClassRecord* record : ordered) {
            std::string typeName = ToGlobalTypeName(record->QualifiedName);
            if (typeName.empty()) {
                continue;
            }
            outFile << "    RegisterType<" << typeName << ">();\n";

            std::sort(record->Properties.begin(), record->Properties.end(),
                [](const AnnotationEntry& lhs, const AnnotationEntry& rhs) {
                    return lhs.DeclName < rhs.DeclName;
                });
            std::unordered_set<std::string> propNames;
            for (const auto& prop : record->Properties) {
                if (!propNames.insert(prop.DeclName).second) {
                    AppendError(outErrors,
                        "Duplicate property annotation for " + record->QualifiedName
                            + "::" + prop.DeclName,
                        prop.Location);
                    continue;
                }
                outFile << "    RegisterPropertyField<&" << typeName << "::" << prop.DeclName
                        << ">(\"" << EscapeCppString(prop.DeclName) << "\");\n";
            }

            std::sort(record->Methods.begin(), record->Methods.end(),
                [](const AnnotationEntry& lhs, const AnnotationEntry& rhs) {
                    return lhs.DeclName < rhs.DeclName;
                });
            std::unordered_set<std::string> methodNames;
            for (const auto& method : record->Methods) {
                if (!methodNames.insert(method.DeclName).second) {
                    AppendError(outErrors,
                        "Overloaded or duplicate method annotations "
                        "are not supported: "
                            + record->QualifiedName + "::" + method.DeclName,
                        method.Location);
                    continue;
                }
                outFile << "    RegisterMethodField<&" << typeName << "::" << method.DeclName
                        << ">(\"" << EscapeCppString(method.DeclName) << "\");\n";
            }
        }

        outFile << "}\n";
        outFile << "} // namespace AltinaEngine::Core::Reflection\n";

        outFile << "\nnamespace AltinaEngine::GameScene {\n";
        std::string compFunc = "RegisterComponent_" + SanitizeIdentifier(options.ModuleName);
        outFile << "void " << compFunc << "() {\n";
        outFile << "#if AE_HAS_COMPONENT_REGISTRY\n";
        for (ClassRecord* record : ordered) {
            std::string typeName = ToGlobalTypeName(record->QualifiedName);
            if (typeName.empty()) {
                continue;
            }
            if (record->IsAbstract) {
                continue;
            }
            outFile << "    if constexpr (std::is_base_of_v<AltinaEngine::GameScene::FComponent, "
                    << typeName << "> && !std::is_abstract_v<" << typeName << ">) {\n";
            outFile << "        RegisterComponentType<" << typeName << ">();\n";
            outFile << "    }\n";
        }
        outFile << "#endif\n";
        outFile << "}\n";
        outFile << "} // namespace AltinaEngine::GameScene\n";
        return true;
    }

    static auto CheckForbiddenAnnotations(const Options& options,
        const std::vector<FileResult>& results, std::vector<std::string>& outErrors) -> bool {
        if (!options.ForbidAnnotations) {
            return true;
        }

        bool ok = true;
        for (const auto& result : results) {
            for (const auto& entry : result.Annotations) {
                if (!options.ModuleRoot.empty()) {
                    if (entry.Location.File.empty()) {
                        AppendError(outErrors, "Forbidden reflection annotation missing location",
                            entry.Location);
                        ok = false;
                        continue;
                    }
                    if (!IsUnderRoot(entry.Location.File, options.ModuleRoot)) {
                        continue;
                    }
                }

                std::string name = entry.QualifiedName;
                if (name.empty()) {
                    name = entry.DeclName;
                }
                if (name.empty()) {
                    name = entry.Annotation;
                }
                AppendError(outErrors, "Forbidden reflection annotation: " + name, entry.Location);
                ok = false;
            }
        }
        return ok;
    }

    static auto WriteJsonString(std::ostream& os, const std::string& value) -> void {
        os << '"';
        for (unsigned char ch : value) {
            switch (ch) {
                case '"':
                    os << "\\\"";
                    break;
                case '\\':
                    os << "\\\\";
                    break;
                case '\b':
                    os << "\\b";
                    break;
                case '\f':
                    os << "\\f";
                    break;
                case '\n':
                    os << "\\n";
                    break;
                case '\r':
                    os << "\\r";
                    break;
                case '\t':
                    os << "\\t";
                    break;
                default:
                    if (ch < 0x20) {
                        const char* hex = "0123456789ABCDEF";
                        os << "\\u00" << hex[(ch >> 4) & 0xF] << hex[ch & 0xF];
                    } else {
                        os << ch;
                    }
                    break;
            }
        }
        os << '"';
    }

    static auto WriteIndent(std::ostream& os, int indent) -> void {
        for (int i = 0; i < indent; ++i) {
            os << ' ';
        }
    }

    static auto WriteArgValue(std::ostream& os, const ArgValue& value) -> void {
        if (std::holds_alternative<bool>(value)) {
            os << (std::get<bool>(value) ? "true" : "false");
        } else if (std::holds_alternative<int64_t>(value)) {
            os << std::get<int64_t>(value);
        } else if (std::holds_alternative<double>(value)) {
            os << std::get<double>(value);
        } else {
            WriteJsonString(os, std::get<std::string>(value));
        }
    }

    static auto WriteStringArray(
        std::ostream& os, const std::vector<std::string>& values, int indent) -> void {
        os << '[';
        if (!values.empty()) {
            os << '\n';
            for (size_t i = 0; i < values.size(); ++i) {
                WriteIndent(os, indent + 2);
                WriteJsonString(os, values[i]);
                if (i + 1 < values.size()) {
                    os << ',';
                }
                os << '\n';
            }
            WriteIndent(os, indent);
        }
        os << ']';
    }

    static auto WriteArgs(std::ostream& os, const std::vector<ArgPair>& args, int indent) -> void {
        os << '{';
        if (!args.empty()) {
            os << '\n';
            for (size_t i = 0; i < args.size(); ++i) {
                WriteIndent(os, indent + 2);
                WriteJsonString(os, args[i].Key);
                os << ": ";
                WriteArgValue(os, args[i].Value);
                if (i + 1 < args.size()) {
                    os << ',';
                }
                os << '\n';
            }
            WriteIndent(os, indent);
        }
        os << '}';
    }

    static auto WriteLoc(std::ostream& os, const Loc& loc, int indent) -> void {
        os << '{' << '\n';
        WriteIndent(os, indent + 2);
        WriteJsonString(os, "file");
        os << ": ";
        WriteJsonString(os, loc.File);
        os << ",\n";
        WriteIndent(os, indent + 2);
        WriteJsonString(os, "line");
        os << ": " << loc.Line << ",\n";
        WriteIndent(os, indent + 2);
        WriteJsonString(os, "col");
        os << ": " << loc.Col << '\n';
        WriteIndent(os, indent);
        os << '}';
    }

    static auto WriteAnnotationEntry(std::ostream& os, const AnnotationEntry& entry, int indent)
        -> void {
        os << '{' << '\n';
        WriteIndent(os, indent + 2);
        WriteJsonString(os, "decl_kind");
        os << ": ";
        WriteJsonString(os, entry.DeclKind);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "decl_node_kind");
        os << ": ";
        WriteJsonString(os, entry.DeclNodeKind);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "decl_name");
        os << ": ";
        WriteJsonString(os, entry.DeclName);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "qualified_name");
        os << ": ";
        WriteJsonString(os, entry.QualifiedName);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "owner_name");
        os << ": ";
        WriteJsonString(os, entry.OwnerName);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "owner_qualified_name");
        os << ": ";
        WriteJsonString(os, entry.OwnerQualifiedName);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "loc");
        os << ": ";
        WriteLoc(os, entry.Location, indent + 2);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "annotation");
        os << ": ";
        WriteJsonString(os, entry.Annotation);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "tag");
        os << ": ";
        WriteJsonString(os, entry.Tag);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "args");
        os << ": ";
        WriteArgs(os, entry.Args, indent + 2);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "errors");
        os << ": ";
        WriteStringArray(os, entry.Errors, indent + 2);
        os << '\n';

        WriteIndent(os, indent);
        os << '}';
    }

    static auto WriteFileResult(std::ostream& os, const FileResult& result, int indent) -> void {
        os << '{' << '\n';
        WriteIndent(os, indent + 2);
        WriteJsonString(os, "file");
        os << ": ";
        WriteJsonString(os, result.File);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "compiler_mode");
        os << ": ";
        WriteJsonString(os, result.CompilerMode);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "errors");
        os << ": ";
        WriteStringArray(os, result.Errors, indent + 2);
        os << ",\n";

        WriteIndent(os, indent + 2);
        WriteJsonString(os, "annotations");
        os << ": [";
        if (!result.Annotations.empty()) {
            os << '\n';
            for (size_t i = 0; i < result.Annotations.size(); ++i) {
                WriteIndent(os, indent + 4);
                WriteAnnotationEntry(os, result.Annotations[i], indent + 4);
                if (i + 1 < result.Annotations.size()) {
                    os << ',';
                }
                os << '\n';
            }
            WriteIndent(os, indent + 2);
        }
        os << ']';
        os << '\n';

        WriteIndent(os, indent);
        os << '}';
    }

    static auto PrintUsage() -> void {
        std::cerr
            << "ReflectionScanner usage:\n"
            << "  ReflectionScanner --compile-commands <path> [options]\n\n"
            << "Options:\n"
            << "  --file <path>           Scan a specific file (repeatable)\n"
            << "  --compiler <path>       Override compiler (default: clang-cl)\n"
            << "  --extra-arg <arg>        Extra compiler argument (repeatable)\n"
            << "  --include-headers        Include declarations from headers\n"
            << "  --max-files <n>          Limit number of files scanned\n"
            << "  --out <path>             Write JSON output to file\n"
            << "  --module-name <name>     Module name for code generation\n"
            << "  --module-root <path>     Module root for filtering includes\n"
            << "  --gen-cpp <path>         Write generated C++ registration file (requires --module-name)\n"
            << "  --forbid-annotations     Fail if any reflection annotations are found under module root\n"
            << "  --strict                 Treat any scan error as failure\n"
            << "  --verbose                Print clang command lines\n";
    }

    static auto ParseOptions(int argc, char** argv, Options& options) -> bool {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--compile-commands") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.CompileCommands = argv[++i];
            } else if (arg == "--file") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.Files.push_back(argv[++i]);
            } else if (arg == "--compiler") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.Compiler = argv[++i];
            } else if (arg == "--extra-arg") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.ExtraArgs.push_back(argv[++i]);
            } else if (arg == "--include-headers") {
                options.IncludeHeaders = true;
            } else if (arg == "--max-files") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.MaxFiles = std::atoi(argv[++i]);
            } else if (arg == "--out") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.OutFile = argv[++i];
            } else if (arg == "--module-name") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.ModuleName = argv[++i];
            } else if (arg == "--module-root") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.ModuleRoot = argv[++i];
            } else if (arg == "--gen-cpp") {
                if (i + 1 >= argc) {
                    return false;
                }
                options.GenCpp = argv[++i];
            } else if (arg == "--forbid-annotations") {
                options.ForbidAnnotations = true;
            } else if (arg == "--strict") {
                options.Strict = true;
            } else if (arg == "--verbose") {
                options.Verbose = true;
            } else if (arg == "--help" || arg == "-h") {
                PrintUsage();
                return false;
            } else {
                std::cerr << "Unknown argument: " << arg << "\n";
                return false;
            }
        }
        return true;
    }

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!ParseOptions(argc, argv, options)) {
        PrintUsage();
        return 1;
    }

    auto ccPath = FindCompileCommands(options.CompileCommands);
    if (!ccPath) {
        std::cerr << "compile_commands.json not found. Use --compile-commands." << "\n";
        return 1;
    }

    std::string ccText;
    if (!ReadFileText(*ccPath, ccText)) {
        std::cerr << "Failed to read compile_commands.json: " << *ccPath << "\n";
        return 1;
    }

    std::vector<CompileCommand> commands;
    std::string                 error;
    if (!ParseCompileCommands(ccText, commands, error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!options.Files.empty()) {
        std::unordered_set<std::string> wanted;
        for (const auto& file : options.Files) {
            wanted.insert(NormalizePath(file));
        }
        std::vector<CompileCommand> filtered;
        for (const auto& entry : commands) {
            if (wanted.find(NormalizePath(entry.File)) != wanted.end()) {
                filtered.push_back(entry);
            }
        }
        commands.swap(filtered);
    }

    if (options.MaxFiles > 0 && static_cast<int>(commands.size()) > options.MaxFiles) {
        commands.resize(static_cast<size_t>(options.MaxFiles));
    }

    if (commands.empty()) {
        std::cerr << "No compile commands matched." << "\n";
        return 1;
    }

    std::vector<FileResult> results;
    results.reserve(commands.size());
    for (const auto& entry : commands) {
        results.push_back(ScanFile(entry, options));
    }

    bool hasScanErrors = false;
    for (const auto& result : results) {
        if (!result.Errors.empty()) {
            hasScanErrors = true;
        }
        for (const auto& annotation : result.Annotations) {
            if (!annotation.Errors.empty()) {
                hasScanErrors = true;
                break;
            }
        }
        if (hasScanErrors) {
            break;
        }
    }

    std::vector<std::string> genErrors;
    bool                     genOk = GenerateCpp(options, results, genErrors);
    if (!genErrors.empty()) {
        for (const auto& err : genErrors) {
            std::cerr << "[refl-gen] " << err << "\n";
        }
    }

    std::vector<std::string> forbidErrors;
    bool                     forbidOk = CheckForbiddenAnnotations(options, results, forbidErrors);
    if (!forbidErrors.empty()) {
        for (const auto& err : forbidErrors) {
            std::cerr << "[refl-forbid] " << err << "\n";
        }
    }

    std::ostringstream output;
    output << "{\n";
    WriteIndent(output, 2);
    WriteJsonString(output, "compile_commands");
    output << ": ";
    WriteJsonString(output, *ccPath);
    output << ",\n";

    WriteIndent(output, 2);
    WriteJsonString(output, "files");
    output << ": [";
    if (!results.empty()) {
        output << "\n";
        for (size_t i = 0; i < results.size(); ++i) {
            WriteIndent(output, 4);
            WriteFileResult(output, results[i], 4);
            if (i + 1 < results.size()) {
                output << ',';
            }
            output << "\n";
        }
        WriteIndent(output, 2);
    }
    output << "]\n";
    output << "}\n";

    if (!options.OutFile.empty()) {
        std::ofstream outFile(options.OutFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to write output file: " << options.OutFile << "\n";
            return 1;
        }
        outFile << output.str();
    } else {
        std::cout << output.str();
    }

    bool hasErrors = hasScanErrors || !genErrors.empty() || !forbidErrors.empty();
    if (!genOk) {
        return 1;
    }
    if (!forbidOk) {
        return 1;
    }
    if (options.Strict && hasErrors) {
        return 1;
    }
    return 0;
}
