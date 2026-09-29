// Small Windows-only launcher. The actual app and Qt stay in the C++/QML app.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <fdi.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

static std::wstring wide(const char* text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (!count) throw std::runtime_error("Invalid file name");
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), count);
    result.pop_back();
    return result;
}

static std::string utf8(const fs::path& path) {
    auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

static std::string_view resource(int id) {
    auto entry = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    auto bytes = entry ? LoadResource(nullptr, entry) : nullptr;
    auto data = bytes ? LockResource(bytes) : nullptr;
    if (!data) throw std::runtime_error("Missing bundled files");
    return {static_cast<const char*>(data), SizeofResource(nullptr, entry)};
}

static void* DIAMONDAPI allocate(ULONG size) { return std::malloc(size); }
static void DIAMONDAPI release(void* memory) { std::free(memory); }
static INT_PTR DIAMONDAPI openCab(char* name, int, int) {
    try {
        return reinterpret_cast<INT_PTR>(CreateFileW(wide(name).c_str(), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    } catch (...) { return -1; }
}
static UINT DIAMONDAPI readFile(INT_PTR file, void* data, UINT size) {
    DWORD read = 0;
    return ReadFile(reinterpret_cast<HANDLE>(file), data, size, &read, nullptr) ? read : UINT(-1);
}
static UINT DIAMONDAPI writeFile(INT_PTR file, void* data, UINT size) {
    DWORD written = 0;
    return WriteFile(reinterpret_cast<HANDLE>(file), data, size, &written, nullptr) ? written : UINT(-1);
}
static int DIAMONDAPI closeFile(INT_PTR file) {
    return CloseHandle(reinterpret_cast<HANDLE>(file)) ? 0 : -1;
}
static long DIAMONDAPI seekFile(INT_PTR file, long distance, int origin) {
    LARGE_INTEGER offset{}, position{};
    offset.QuadPart = distance;
    return SetFilePointerEx(reinterpret_cast<HANDLE>(file), offset, &position, origin)
        ? static_cast<long>(position.QuadPart) : -1;
}

static fs::path relativePath(const char* name) {
    fs::path path(wide(name));
    if (path.empty() || path.is_absolute() || path.has_root_name())
        throw std::runtime_error("Invalid archive path");
    for (const auto& part : path)
        if (part == L"..") throw std::runtime_error("Invalid archive path");
    return path;
}

static INT_PTR DIAMONDAPI extractFile(FDINOTIFICATIONTYPE type, PFDINOTIFICATION notice) {
    try {
        if (type == fdintCOPY_FILE) {
            const auto& folder = *static_cast<fs::path*>(notice->pv);
            auto path = folder / relativePath(notice->psz1);
            fs::create_directories(path.parent_path());
            return reinterpret_cast<INT_PTR>(CreateFileW(path.c_str(), GENERIC_WRITE, 0,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        }
        if (type == fdintCLOSE_FILE_INFO) return closeFile(notice->hf) == 0;
        if (type == fdintNEXT_CABINET) return -1;
        return 0;
    } catch (...) { return -1; }
}

static bool complete(const fs::path& folder) {
    if (!fs::is_regular_file(folder / L".ready")) return false;
    // Recover automatically if a cache cleaner removed any supporting file.
    std::istringstream manifest(std::string(resource(102)));
    std::string line;
    std::error_code error;
    while (std::getline(manifest, line)) {
        auto tab = line.find('\t');
        if (tab == std::string::npos) return false;
        auto path = folder / relativePath(line.c_str() + tab + 1);
        auto size = fs::file_size(path, error);
        if (error || size != std::stoull(line.substr(0, tab))) return false;
    }
    return true;
}

static void unpack(const fs::path& folder) {
    fs::create_directories(folder);
    fs::remove(folder / L".ready");
    auto cabPath = folder / L"payload.cab";
    auto bytes = resource(101);
    std::ofstream cab(cabPath, std::ios::binary | std::ios::trunc);
    cab.write(bytes.data(), bytes.size());
    cab.close();
    if (!cab) throw std::runtime_error("Could not write bundled files");
    ERF error{};
    auto decoder = FDICreate(allocate, release, openCab, readFile, writeFile, closeFile,
        seekFile, cpuUNKNOWN, &error);
    if (!decoder) throw std::runtime_error("Could not open bundled files");
    auto directory = utf8(folder) + "\\";
    char name[] = "payload.cab";
    auto destination = folder;
    bool success = FDICopy(decoder, name, directory.data(), 0, extractFile, nullptr, &destination);
    FDIDestroy(decoder);
    fs::remove(cabPath);
    if (!success) throw std::runtime_error("Could not unpack bundled files");
    std::ofstream marker(folder / L".ready");
    marker << "ready";
    marker.close();
    if (!marker || !complete(folder)) throw std::runtime_error("Bundled files are incomplete");
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    HANDLE mutex = nullptr;
    bool locked = false;
    try {
        SetDllDirectoryW(L"");
        PWSTR local = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local)))
            throw std::runtime_error("Could not find the user cache folder");
        fs::path folder = fs::path(local) / L"FlintAutoClicker" / L"Runtime" / wide(BUNDLE_ID);
        CoTaskMemFree(local);
        auto mutexName = L"Local\\FlintAutoClicker-unpack-" + wide(BUNDLE_ID);
        mutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        if (!mutex) throw std::runtime_error("Could not prepare the app");
        auto wait = WaitForSingleObject(mutex, 120000);
        locked = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        if (!locked) throw std::runtime_error("Another launch is still preparing the app. Try again shortly.");
        if (!complete(folder)) unpack(folder);
        ReleaseMutex(mutex);
        locked = false;
        CloseHandle(mutex);
        mutex = nullptr;
        auto executable = folder / L"FlintAutoClicker.exe";
        std::wstring command = L"\"" + executable.wstring() + L"\" " + arguments;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
            0, nullptr, folder.c_str(), &startup, &process))
            throw std::runtime_error("Could not open FlintAutoClicker");
        CloseHandle(process.hThread);
        // Keep the launcher alive so callers can observe the app's exit status.
        SetProcessWorkingSetSize(GetCurrentProcess(), SIZE_T(-1), SIZE_T(-1));
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hProcess);
        return static_cast<int>(code);
    } catch (const std::exception& error) {
        if (locked) ReleaseMutex(mutex);
        if (mutex) CloseHandle(mutex);
        auto message = wide(error.what()) + L".\n\nPlease try again or download a fresh copy.";
        MessageBoxW(nullptr, message.c_str(), L"FlintAutoClicker", MB_OK | MB_ICONERROR);
        return 1;
    }
}
