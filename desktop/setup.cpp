/*
 * Plants vs. Zombies: MODDED — Windows x64 installer stub (self-extracting).
 *
 * The build pipeline (scripts/build-windows.sh + scripts/make-installer.py)
 * appends a payload archive to this executable:
 *
 *   [this PE image][data bytes][file table][footer]
 *   footer (24 bytes, at EOF):
 *     char     magic[8] = "PVZMSTP1"
 *     uint64   blobOffset   (absolute file offset of the data region)
 *     uint64   dataBytes    (size of the data region; table follows it)
 *     uint32   fileCount
 *     uint32   reserved
 *   file table entry:
 *     uint16   pathLen, then UTF-8 relative path, then uint64 size, uint64 off
 *     (off is relative to blobOffset)
 *
 * Modes:
 *   (no args)              Extract to %LOCALAPPDATA%\PVZM, create a Desktop
 *                          shortcut, offer to launch the game.
 *   --extract <dir>        Extract into <dir> (portable; used by the build
 *                          pipeline's round-trip test, works on Linux too).
 *
 * The installer installs per-user (no administrator rights required).
 * Uninstall: delete the install folder and the shortcut.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#endif

static const char kMagic[8] = {'P', 'V', 'Z', 'M', 'S', 'T', 'P', '1'};

struct Entry {
    std::string path; /* UTF-8, relative, with '/' separators */
    uint64_t size;
    uint64_t off;
};

static FILE* OpenSelf(const char* fromUtf8) {
    if (fromUtf8) return fopen(fromUtf8, "rb");
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    if (!n) return nullptr;
    char narrow[MAX_PATH * 4] = {0};
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
    return fopen(narrow, "rb");
#else
    return fopen("/proc/self/exe", "rb");
#endif
}

static bool ReadTable(FILE* f, uint64_t* blobOff, std::vector<Entry>* out) {
    if (fseek(f, 0, SEEK_END) != 0) return false;
    long end = ftell(f);
    if (end < 32) return false;
    if (fseek(f, end - 32, SEEK_SET) != 0) return false;
    char magic[8];
    uint64_t dataBytes = 0;
    uint32_t count = 0, reserved = 0;
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kMagic, 8) != 0) return false;
    if (fread(blobOff, 8, 1, f) != 1) return false;
    if (fread(&dataBytes, 8, 1, f) != 1) return false;
    if (fread(&count, 4, 1, f) != 1) return false;
    if (fread(&reserved, 4, 1, f) != 1) return false;
    if (*blobOff + dataBytes > (uint64_t)end) return false;
    if (fseek(f, (long)(*blobOff + dataBytes), SEEK_SET) != 0) return false;
    for (uint32_t i = 0; i < count; i++) {
        Entry e;
        uint16_t len = 0;
        if (fread(&len, 2, 1, f) != 1 || len == 0 || len > 1024) return false;
        std::vector<char> path(len + 1, 0);
        if (fread(path.data(), 1, len, f) != len) return false;
        e.path.assign(path.data(), len);
        if (fread(&e.size, 8, 1, f) != 1) return false;
        if (fread(&e.off, 8, 1, f) != 1) return false;
        out->push_back(e);
    }
    return true;
}

#ifdef _WIN32
static std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], n);
    return w;
}

static void MkdirsW(const std::wstring& path) {
    std::wstring cur;
    for (size_t i = 0; i < path.size(); i++) {
        wchar_t c = path[i];
        cur.push_back(c);
        if ((c == L'\\' || c == L'/') && cur.size() > 3) {
            CreateDirectoryW(cur.c_str(), nullptr);
        }
    }
    CreateDirectoryW(path.c_str(), nullptr);
}
#endif

static bool ExtractOne(FILE* self, uint64_t blobOff, const Entry& e,
                       const std::string& outDirUtf8) {
    std::string rel = e.path;
    for (size_t i = 0; i < rel.size(); i++) {
        if (rel[i] == '\\') rel[i] = '/';
    }
    if (rel.empty() || rel[0] == '/' || rel.find("..") != std::string::npos) {
        fprintf(stderr, "refusing unsafe path: %s\n", rel.c_str());
        return false;
    }
    std::string full = outDirUtf8;
#ifdef _WIN32
    std::wstring wrel = Widen(rel);
    std::wstring wfull = Widen(full) + L"\\" + wrel;
    for (size_t i = 0; i < wfull.size(); i++) {
        if (wfull[i] == L'/') wfull[i] = L'\\';
    }
    /* parent dir */
    size_t slash = wfull.find_last_of(L'\\');
    if (slash != std::wstring::npos) MkdirsW(wfull.substr(0, slash));
    HANDLE out = CreateFileW(wfull.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "cannot write: %s\n", rel.c_str());
        return false;
    }
    if (fseek(self, (long)(blobOff + e.off), SEEK_SET) != 0) {
        CloseHandle(out);
        return false;
    }
    std::vector<char> buf(1 << 20);
    uint64_t remaining = e.size;
    while (remaining) {
        size_t chunk = (size_t)(remaining < buf.size() ? remaining : buf.size());
        if (fread(buf.data(), 1, chunk, self) != chunk) {
            CloseHandle(out);
            return false;
        }
        DWORD written = 0;
        if (!WriteFile(out, buf.data(), (DWORD)chunk, &written, nullptr) ||
            written != chunk) {
            CloseHandle(out);
            return false;
        }
        remaining -= chunk;
    }
    CloseHandle(out);
#else
    std::string dir = full + "/" + rel;
    size_t slash = dir.find_last_of('/');
    if (slash != std::string::npos) {
        std::string parent = dir.substr(0, slash);
        std::string cur;
        for (size_t i = 0; i < parent.size(); i++) {
            cur.push_back(parent[i]);
            if ((parent[i] == '/' || i + 1 == parent.size()) && cur.size() > 1) {
                mkdir(cur.c_str(), 0755);
            }
        }
    }
    FILE* out = fopen(dir.c_str(), "wb");
    if (!out) {
        fprintf(stderr, "cannot write: %s\n", rel.c_str());
        return false;
    }
    if (fseek(self, (long)(blobOff + e.off), SEEK_SET) != 0) {
        fclose(out);
        return false;
    }
    std::vector<char> buf(1 << 20);
    uint64_t remaining = e.size;
    while (remaining) {
        size_t chunk = (size_t)(remaining < buf.size() ? remaining : buf.size());
        if (fread(buf.data(), 1, chunk, self) != chunk) {
            fclose(out);
            return false;
        }
        if (fwrite(buf.data(), 1, chunk, out) != chunk) {
            fclose(out);
            return false;
        }
        remaining -= chunk;
    }
    fclose(out);
#endif
    return true;
}

static int ExtractAll(const std::string& outDirUtf8, const char* fromUtf8) {
    FILE* self = OpenSelf(fromUtf8);
    if (!self) {
        fprintf(stderr, "cannot open self\n");
        return 2;
    }
    uint64_t blobOff = 0;
    std::vector<Entry> table;
    if (!ReadTable(self, &blobOff, &table) || table.empty()) {
        fprintf(stderr, "payload archive missing or corrupt\n");
        fclose(self);
        return 2;
    }
    for (const Entry& e : table) {
        if (!ExtractOne(self, blobOff, e, outDirUtf8)) {
            fclose(self);
            return 3;
        }
    }
    fclose(self);
    printf("extracted %zu files to %s\n", table.size(), outDirUtf8.c_str());
    return 0;
}

#ifdef _WIN32
static void CreateDesktopShortcut(const std::wstring& installDir) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&link)) ||
        !link) {
        return;
    }
    std::wstring target = installDir + L"\\PVZM.exe";
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(installDir.c_str());
    link->SetDescription(L"Plants vs. Zombies: MODDED (desktop dev build)");
    IPersistFile* pf = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&pf)) && pf) {
        wchar_t* desktop = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr,
                                           &desktop)) &&
            desktop) {
            std::wstring lnk = std::wstring(desktop) + L"\\Plants vs Zombies MODDED.lnk";
            pf->Save(lnk.c_str(), TRUE);
            CoTaskMemFree(desktop);
        }
        pf->Release();
    }
    link->Release();
    CoUninitialize();
}

static void InstallGui() {
    wchar_t base[MAX_PATH * 2] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT,
                                base))) {
        wcscpy(base, L"C:\\");
    }
    std::wstring dir = std::wstring(base) + L"PVZM";
    MkdirsW(dir);

    char dirUtf8[MAX_PATH * 4] = {0};
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, dirUtf8, sizeof(dirUtf8) - 1,
                        nullptr, nullptr);
    int rc = ExtractAll(dirUtf8, nullptr);
    if (rc != 0) {
        MessageBoxW(nullptr,
                    L"Installation failed (could not extract the game files).",
                    L"Plants vs. Zombies: MODDED — Setup", MB_ICONERROR | MB_OK);
        ExitProcess((UINT)rc);
    }
    CreateDesktopShortcut(dir);

    std::wstring msg =
        L"Plants vs. Zombies: MODDED (development build) installed to:\n\n" + dir +
        L"\n\nA shortcut was placed on your Desktop.\n\nLaunch the game now?";
    if (MessageBoxW(nullptr, msg.c_str(), L"Plants vs. Zombies: MODDED — Setup",
                    MB_ICONQUESTION | MB_YESNO) == IDYES) {
        std::wstring exe = dir + L"\\PVZM.exe";
        STARTUPINFOW si = {0};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi = {0};
        std::wstring cmd = L"\"" + exe + L"\"";
        if (CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr,
                           dir.c_str(), &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}
#endif

int main(int argc, char** argv) {
    const char* from = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            from = argv[i + 1];
        }
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--extract") == 0 && i + 1 < argc) {
            return ExtractAll(argv[i + 1], from);
        }
    }
#ifdef _WIN32
    (void)from;
    InstallGui();
    return 0;
#else
    fprintf(stderr, "usage: %s --extract <dir> [--from <installer.exe>]\n", argv[0]);
    return 1;
#endif
}

#ifdef _WIN32
/* GUI subsystem entry: forward to main() with UTF-8 args. */
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> storage;
    std::vector<char*> argv;
    storage.push_back("setup");
    for (int i = 1; i < argc; i++) {
        char buf[MAX_PATH * 4] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, buf, sizeof(buf) - 1, nullptr,
                            nullptr);
        storage.push_back(buf);
    }
    if (wargv) LocalFree(wargv);
    for (auto& s : storage) argv.push_back(&s[0]);
    return main((int)argv.size(), argv.data());
}
#endif
