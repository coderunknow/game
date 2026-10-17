/*
 * Plants vs. Zombies: MODDED — desktop host (Windows x64)
 *
 * Builds a native Win32 window backed by the Microsoft Edge WebView2 Runtime
 * and loads the locally bundled game through a virtual host name mapping
 * (pvzm.game -> the application folder). No web server is started and no
 * network access is required for gameplay: every game file (HTML, JS modules,
 * images, audio, fonts) is served from disk by the WebView2 runtime.
 *
 * Command line:
 *   (no args)          Launch the game window.
 *   --selftest         No GUI: verify the installation layout and the presence
 *                      of the WebView2 Runtime. Exit 0 on success.
 *   --smoke <file>     Launch, load the game page and drive the real Soak-shroom
 *                      regression scenario inside the packaged application
 *                      (boot -> level 1-1 -> plant Soak-shroom on the top row and
 *                      a lower row -> spawn zombies -> verify bullets, damage,
 *                      loop survival). Writes a JSON result to <file> and exits
 *                      (0 = pass). Used by CI on a real Windows runner.
 *
 * Build (from Linux or Windows, via zig): see scripts/build-windows.sh and
 * desktop/README.md.
 *
 * WebView2Loader.dll (redistributable, MIT — see desktop/THIRD-PARTY-NOTICES.txt)
 * must sit next to this executable.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "WebView2.h"

static std::wstring g_exeDir;
static HWND g_hwnd = nullptr;
static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;
static bool g_smoke = false;
static std::wstring g_smokePath;
static bool g_done = false;
static int g_stage = 0;
static int g_retries = 0;
static EventRegistrationToken g_navToken;

static void SmokeFinish(const char* json, int exitCode);

/* ------------------------------------------------------------------ utils */

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    std::wstring p(buf, n ? n : 0);
    size_t slash = p.find_last_of(L'\\');
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

static bool ExistsW(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

static bool IsDirW(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring Join(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

static void Mkdirs(const std::wstring& path) {
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

static std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    wchar_t* p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p)) && p) {
        out = p;
        CoTaskMemFree(p);
    }
    return out;
}

/* WebView2 Runtime presence: best-effort registry probe so --selftest can
 * report a clear message before any window is created. */
static bool WebView2RuntimePresent() {
    const wchar_t* paths[] = {
        L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
    };
    HKEY hives[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (HKEY hive : hives) {
        for (const wchar_t* k : paths) {
            HKEY h = nullptr;
            if (RegOpenKeyExW(hive, k, 0, KEY_READ, &h) == ERROR_SUCCESS) {
                wchar_t pv[64] = {0};
                DWORD sz = sizeof(pv);
                LONG r = RegQueryValueExW(h, L"pv", nullptr, nullptr, (LPBYTE)pv, &sz);
                RegCloseKey(h);
                if (r == ERROR_SUCCESS && pv[0] && wcscmp(pv, L"0.0.0.0") != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------- smoke mode */

static void SmokeFinish(const char* json, int exitCode) {
    if (g_done) return;
    g_done = true;
    if (!g_smokePath.empty()) {
        HANDLE f = CreateFileW(g_smokePath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(f, json, (DWORD)strlen(json), &written, nullptr);
            WriteFile(f, "\n", 1, &written, nullptr);
            CloseHandle(f);
        }
    }
    printf("%s\n", json);
    fflush(stdout);
    PostQuitMessage(exitCode);
}

/* Smoke stages. Each stage runs a script; the reply handler schedules the
 * next stage with a timer so the game's own async boot can progress. */
enum { kStageNone = 0, kStageProbe = 1, kStageLevel = 2, kStageScenario = 3, kStageCollect = 4 };

/* Instrument the engine, then boot through the production path. */
static const wchar_t* kScriptProbe =
    L"(function(){"
    L"var r={ready:document.readyState,title:document.title,"
    L"engine:typeof oSym==='object',selectModal:typeof SelectModal==='function',"
    L"oGun:typeof oGun==='function',"
    L"oGunAttack:typeof oGun==='function'&&typeof oGun.prototype.NormalAttack==='function'};"
    L"try{window.abxz=null;"
    L"window.__loopCalls=0;window.__bulletSeen=0;window.__shootGifSeen=0;window.__consoleLog=[];"
    L"var oe=console.error;console.error=function(){try{"
    L"window.__consoleLog.push(Array.prototype.map.call(arguments,function(a){"
    L"return (a&&a.stack)?a.stack:String(a);}).join(' '));}catch(e){}return oe.apply(this,arguments);};"
    L"var orig=oZ.traversalOf.bind(oZ);"
    L"oZ.traversalOf=function(){window.__loopCalls++;return orig.apply(null,arguments);};"
    L"window.__assetStatus=null;"
    L"fetch('https://pvzm.game/game/images/Plants/SeaShroom/SeaShroom.gif',{cache:'no-store'})"
    L".then(function(x){window.__assetStatus=x.status;})"
    L".catch(function(e){window.__assetStatus='err:'+String(e);});"
    L"InitGame();r.boot='ok';}catch(e){r.boot=String(e);}"
    L"return JSON.stringify(r);})()";

/* Load level 1-1 through the production SelectModal path (level 0 must be up). */
static const wchar_t* kScriptLevel =
    L"(function(){var r={level0:!!window.$Pn};try{"
    L"if(window.$Pn){SelectModal(1);r.select='ok';}else{r.select='level0-not-ready';}"
    L"}catch(e){r.select=String(e);}return JSON.stringify(r);})()";

/* Plant Soak-shroom on the top row (5) and a lower row (3), spawn a zombie in
 * each row, and poll for the transient bullet + shoot gif. Row 5 is the case
 * that froze the whole game before the fix (synchronous top-row traversal). */
static const wchar_t* kScriptScenario =
    L"(function(){var r={};try{"
    L"if(!(window.oS&&oS.Lvl==1)){return JSON.stringify({skip:'level-not-ready'});}"
    L"function plant(row){var p=CustomSpecial(oGun,row,3);"
    L"var z=new oZombie();var html=z.CustomBirth(row,6,0,'auto');"
    L"asyncInnerHTML(html,function(frag,zo){EDPZ.appendChild(frag);zo.Birth();},z);"
    L"return {plant:p,z:z,hp0:z.HP};}"
    L"var a=plant(5);var b=plant(3);"
    L"window.__a=a;window.__b=b;"
    L"window.__poll=setInterval(function(){try{"
    L"if(document.querySelector('[id^=\"PSB\"]'))window.__bulletSeen=1;"
    L"var i=$(a.plant.id);if(i&&i.childNodes[1]&&i.childNodes[1].src.indexOf('SeaShroomShoot')>=0)"
    L"window.__shootGifSeen=1;}catch(e){}},25);"
    L"r.planted={top:a.plant.id,lower:b.plant.id};r.hp0=a.hp0;"
    L"}catch(e){r.error=String(e);}return JSON.stringify(r);})()";

/* Collect results and decide pass/fail. */
static const wchar_t* kScriptCollect =
    L"(function(){var r={};try{if(window.__poll)clearInterval(window.__poll);"
    L"var a=window.__a,b=window.__b;"
    L"r.topHP=a?a.z.HP:null;r.lowerHP=b?b.z.HP:null;r.hp0=a?a.hp0:null;"
    L"r.bulletSeen=!!window.__bulletSeen;r.shootGifSeen=!!window.__shootGifSeen;"
    L"r.loopCalls=window.__loopCalls||0;r.loopAlive=(window.__loopCalls||0)>2;"
    L"r.assetStatus=window.__assetStatus;"
    L"r.consoleLog=window.__consoleLog||[];"
    L"r.plantCanTrigger=a&&a.plant?a.plant.canTrigger:null;"
    L"}catch(e){r.error=String(e);}return JSON.stringify(r);})()";

static const wchar_t* StageScript(int stage) {
    switch (stage) {
    case kStageProbe: return kScriptProbe;
    case kStageLevel: return kScriptLevel;
    case kStageScenario: return kScriptScenario;
    case kStageCollect: return kScriptCollect;
    default: return nullptr;
    }
}

static unsigned StageDelayMs(int stage) {
    switch (stage) {
    case kStageProbe: return 400;
    case kStageLevel: return 2000;
    case kStageScenario: return 3000;
    case kStageCollect: return 9000;
    default: return 1000;
    }
}

static void ScheduleStage(int stage) {
    if (stage != g_stage) g_retries = 0;
    g_stage = stage;
    SetTimer(g_hwnd, 10, StageDelayMs(stage), nullptr);
}

/* Retry the current stage (the page boots asynchronously: engine script,
 * ES module tree, level 0 UI and level 1-1 all take time). */
static void RetryStage() {
    g_retries++;
    SetTimer(g_hwnd, 10, 600, nullptr);
}

/* --------------------------------------------------------- COM handlers */

struct EnvCompleted : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG refs = 1;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown ||
            riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r;
    }
    IFACEMETHODIMP Invoke(HRESULT result, ICoreWebView2Environment* env) override;
};

struct CtrlCompleted : ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG refs = 1;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown ||
            riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r;
    }
    IFACEMETHODIMP Invoke(HRESULT result, ICoreWebView2Controller* ctrl) override;
};

struct NavCompleted : ICoreWebView2NavigationCompletedEventHandler {
    LONG refs = 1;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown ||
            riid == IID_ICoreWebView2NavigationCompletedEventHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r;
    }
    IFACEMETHODIMP Invoke(ICoreWebView2* sender,
                          ICoreWebView2NavigationCompletedEventArgs* args) override;
};

struct ScriptCompleted : ICoreWebView2ExecuteScriptCompletedHandler {
    int stage;
    explicit ScriptCompleted(int s) : stage(s) {}
    LONG refs = 1;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2ExecuteScriptCompletedHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r;
    }
    IFACEMETHODIMP Invoke(HRESULT result, LPCWSTR jsonResultAsJson) override;
};

/* Privacy-minded environment options: disable background networking and
 * component updates of the WebView2 browser process. Gameplay itself never
 * uses the network (all files come from the mapped folder). */
struct EnvOptions : ICoreWebView2EnvironmentOptions {
    LONG refs = 1;
    std::wstring args = L"--disable-background-networking --disable-component-update "
                        L"--disable-domain-reliability --no-first-run";
    std::wstring ver = L"999.9999.9999.9999";

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2EnvironmentOptions) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r;
    }
    IFACEMETHODIMP get_AdditionalBrowserArguments(LPWSTR* out) override {
        *out = (LPWSTR)CoTaskMemAlloc((args.size() + 1) * sizeof(wchar_t));
        if (!*out) return E_OUTOFMEMORY;
        memcpy(*out, args.c_str(), (args.size() + 1) * sizeof(wchar_t));
        return S_OK;
    }
    IFACEMETHODIMP put_AdditionalBrowserArguments(LPCWSTR) override { return E_NOTIMPL; }
    IFACEMETHODIMP get_Language(LPWSTR* out) override { *out = nullptr; return S_OK; }
    IFACEMETHODIMP put_Language(LPCWSTR) override { return E_NOTIMPL; }
    IFACEMETHODIMP get_TargetCompatibleBrowserVersion(LPWSTR* out) override {
        *out = (LPWSTR)CoTaskMemAlloc((ver.size() + 1) * sizeof(wchar_t));
        if (!*out) return E_OUTOFMEMORY;
        memcpy(*out, ver.c_str(), (ver.size() + 1) * sizeof(wchar_t));
        return S_OK;
    }
    IFACEMETHODIMP put_TargetCompatibleBrowserVersion(LPCWSTR) override { return E_NOTIMPL; }
    IFACEMETHODIMP get_AllowSingleSignOnUsingOSPrimaryAccount(BOOL* out) override {
        *out = FALSE; return S_OK;
    }
    IFACEMETHODIMP put_AllowSingleSignOnUsingOSPrimaryAccount(BOOL) override { return E_NOTIMPL; }
};

/* ---------------------------------------------------------------- window */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (g_controller) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_controller->put_Bounds(rc);
        }
        return 0;
    case WM_TIMER:
        if (g_smoke && wp == 10) {
            KillTimer(hwnd, 10);
            const wchar_t* s = StageScript(g_stage);
            if (s && g_webview) {
                g_webview->ExecuteScript(s, new ScriptCompleted(g_stage));
            }
            return 0;
        }
        if (g_smoke && wp == 99) { /* watchdog */
            KillTimer(hwnd, 99);
            char buf[128];
            snprintf(buf, sizeof buf, "{\"ok\":false,\"stage\":%d,\"reason\":\"timeout\"}", g_stage);
            SmokeFinish(buf, 5);
            return 0;
        }
        return 0;
    case WM_DESTROY:
        if (g_controller) {
            g_controller->Close();
            g_controller = nullptr;
            g_webview = nullptr;
        }
        PostQuitMessage(g_smoke && !g_done ? 3 : 0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

IFACEMETHODIMP EnvCompleted::Invoke(HRESULT result, ICoreWebView2Environment* env) {
    if (FAILED(result) || !env) {
        char buf[256];
        snprintf(buf, sizeof buf, "{\"ok\":false,\"stage\":\"environment\",\"hr\":%lu}",
                 (unsigned long)result);
        if (g_smoke) { SmokeFinish(buf, 2); return S_OK; }
        MessageBoxW(nullptr,
                    L"The Microsoft Edge WebView2 Runtime could not be started.\n\n"
                    L"Plants vs. Zombies: MODDED (desktop) uses the WebView2 Runtime "
                    L"that is preinstalled with Windows 11 and updated with Microsoft "
                    L"Edge on Windows 10.\n\n"
                    L"Please install/update Microsoft Edge or the free WebView2 "
                    L"Runtime from Microsoft, then start the game again.",
                    L"Plants vs. Zombies: MODDED — WebView2 required", MB_ICONERROR | MB_OK);
        ShellExecuteW(nullptr, L"open",
                      L"https://developer.microsoft.com/microsoft-edge/webview2/",
                      nullptr, nullptr, SW_SHOWNORMAL);
        PostQuitMessage(2);
        return S_OK;
    }
    env->CreateCoreWebView2Controller(g_hwnd, new CtrlCompleted());
    return S_OK;
}

IFACEMETHODIMP CtrlCompleted::Invoke(HRESULT result, ICoreWebView2Controller* ctrl) {
    if (FAILED(result) || !ctrl) {
        char buf[256];
        snprintf(buf, sizeof buf, "{\"ok\":false,\"stage\":\"controller\",\"hr\":%lu}",
                 (unsigned long)result);
        if (g_smoke) { SmokeFinish(buf, 2); return S_OK; }
        MessageBoxW(nullptr, L"Could not create the browser view.",
                    L"Plants vs. Zombies: MODDED", MB_ICONERROR | MB_OK);
        PostQuitMessage(2);
        return S_OK;
    }

    g_controller = ctrl;
    ctrl->get_CoreWebView2(&g_webview);
    if (!g_webview) {
        if (g_smoke) SmokeFinish("{\"ok\":false,\"stage\":\"corewebview\"}", 2);
        PostQuitMessage(2);
        return S_OK;
    }

    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(TRUE);
        /* DevTools stay enabled in this development release so testers can
         * report console errors easily. */
        settings->put_AreDevToolsEnabled(TRUE);
        settings->put_AreDefaultContextMenusEnabled(TRUE);
        settings->Release();
    }

    /* Register the smoke navigation handler BEFORE navigating (no race). */
    if (g_smoke) {
        g_webview->add_NavigationCompleted(new NavCompleted(), &g_navToken);
        SetTimer(g_hwnd, 99, 120000, nullptr); /* watchdog */
    }

    /* Serve the bundled game from disk under a secure origin so ES modules
     * load exactly like on the website, with zero network involvement. */
    ICoreWebView2_3* wv3 = nullptr;
    if (SUCCEEDED(g_webview->QueryInterface(IID_ICoreWebView2_3, (void**)&wv3)) && wv3) {
        wv3->SetVirtualHostNameToFolderMapping(
            L"pvzm.game", g_exeDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
        wv3->Release();
        g_webview->Navigate(L"https://pvzm.game/game/index.html");
    } else {
        /* Very old runtime without ICoreWebView2_3: fall back to file://
         * (ES module loading may fail there; --selftest reports the version). */
        std::wstring url = L"file:///" + g_exeDir + L"/game/index.html";
        for (size_t i = 0; i < url.size(); i++) {
            if (url[i] == L'\\') url[i] = L'/';
        }
        g_webview->Navigate(url.c_str());
    }

    RECT rc;
    GetClientRect(g_hwnd, &rc);
    g_controller->put_Bounds(rc);
    g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    return S_OK;
}

IFACEMETHODIMP NavCompleted::Invoke(ICoreWebView2*,
                                    ICoreWebView2NavigationCompletedEventArgs* args) {
    BOOL ok = FALSE;
    if (args) args->get_IsSuccess(&ok);
    if (!ok) {
        SmokeFinish("{\"ok\":false,\"stage\":\"navigation\"}", 2);
        return S_OK;
    }
    ScheduleStage(kStageProbe);
    return S_OK;
}

IFACEMETHODIMP ScriptCompleted::Invoke(HRESULT result, LPCWSTR jsonResultAsJson) {
    char body[4096] = {0};
    if (FAILED(result) || !jsonResultAsJson) {
        snprintf(body, sizeof body, "{\"ok\":false,\"stage\":%d,\"hr\":%lu}",
                 stage, (unsigned long)result);
        SmokeFinish(body, 2);
        return S_OK;
    }
    char narrow[3072] = {0};
    WideCharToMultiByte(CP_UTF8, 0, jsonResultAsJson, -1, narrow, sizeof(narrow) - 1,
                        nullptr, nullptr);

    if (stage != kStageCollect) {
        /* Record the stage output, then continue. */
        static char trace[2048] = {0};
        size_t used = strlen(trace);
        if (used < sizeof(trace) - 1) {
            snprintf(trace + used, sizeof(trace) - used, "\"s%d\":%s,", stage, narrow);
        }
        if (stage == kStageProbe) {
            bool engine = strstr(narrow, "\"engine\":true") != nullptr;
            bool oGun = strstr(narrow, "\"oGun\":true") != nullptr;
            if (!engine || !oGun) {
                /* Page may still be loading scripts/modules: retry. */
                if (g_retries < 25) { RetryStage(); return S_OK; }
                snprintf(body, sizeof body,
                         "{\"ok\":false,\"stage\":%d,\"reason\":\"engine-or-oGun-missing\",\"page\":%s}",
                         stage, narrow);
                SmokeFinish(body, 4);
                return S_OK;
            }
        } else if (stage == kStageLevel) {
            bool level0 = strstr(narrow, "\"level0\":true") != nullptr;
            bool selected = strstr(narrow, "\"select\":\"ok\"") != nullptr;
            if (!level0 || !selected) {
                if (g_retries < 25) { RetryStage(); return S_OK; }
                snprintf(body, sizeof body,
                         "{\"ok\":false,\"stage\":%d,\"reason\":\"level-1-1-not-ready\",\"detail\":%s}",
                         stage, narrow);
                SmokeFinish(body, 4);
                return S_OK;
            }
        } else if (stage == kStageScenario) {
            bool planted = strstr(narrow, "\"planted\"") != nullptr;
            bool errored = strstr(narrow, "\"skip\"") != nullptr ||
                           strstr(narrow, "\"error\"") != nullptr;
            if (!planted || errored) {
                if (g_retries < 20) { RetryStage(); return S_OK; }
                snprintf(body, sizeof body,
                         "{\"ok\":false,\"stage\":%d,\"reason\":\"scenario-not-planted\",\"detail\":%s}",
                         stage, narrow);
                SmokeFinish(body, 4);
                return S_OK;
            }
        }
        ScheduleStage(stage + 1);
        return S_OK;
    }

    /* Final stage: decide. */
    bool bullet = strstr(narrow, "\"bulletSeen\":true") != nullptr;
    bool gif = strstr(narrow, "\"shootGifSeen\":true") != nullptr;
    bool loop = strstr(narrow, "\"loopAlive\":true") != nullptr;
    bool asset = strstr(narrow, "\"assetStatus\":200") != nullptr;
    bool refErr = strstr(narrow, "is not defined") != nullptr;
    /* damage: topHP or lowerHP must be below hp0 (270 for a plain zombie) */
    bool damaged = false;
    const char* hp0p = strstr(narrow, "\"hp0\":");
    if (hp0p) {
        double hp0 = atof(hp0p + 6);
        for (const char* key : { "\"topHP\":", "\"lowerHP\":" }) {
            const char* p = strstr(narrow, key);
            if (p) {
                double v = atof(p + strlen(key));
                if (hp0 > 0 && v >= 0 && v < hp0) damaged = true;
            }
        }
    }
    bool pass = bullet && gif && loop && asset && damaged && !refErr;
    snprintf(body, sizeof body,
             "{\"ok\":%s,\"checks\":{\"bullet\":%s,\"shootGif\":%s,\"loopAlive\":%s,"
             "\"localAsset\":%s,\"zombieDamaged\":%s,\"noReferenceError\":%s},"
             "\"detail\":%s}",
             pass ? "true" : "false", bullet ? "true" : "false", gif ? "true" : "false",
             loop ? "true" : "false", asset ? "true" : "false",
             damaged ? "true" : "false", refErr ? "false" : "true", narrow);
    SmokeFinish(body, pass ? 0 : 4);
    return S_OK;
}

/* ----------------------------------------------------------------- entry */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    g_exeDir = ExeDir();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; i++) {
        if (wcscmp(argv[i], L"--smoke") == 0 && i + 1 < argc) {
            g_smoke = true;
            g_smokePath = argv[i + 1];
        } else if (wcscmp(argv[i], L"--selftest") == 0) {
            /* No GUI at all: validate the install layout + runtime presence.
             * Optional following argument = path to write the JSON result to.
             * (This is a GUI-subsystem executable, so stdout is not attached to
             * a console when launched from a shell/CI — the file is the reliable
             * channel; we still print for interactive use.) */
            std::wstring outFile;
            if (i + 1 < argc && wcsncmp(argv[i + 1], L"--", 2) != 0) {
                outFile = argv[i + 1];
            }
            std::wstring game = Join(g_exeDir, L"game");
            bool layout = IsDirW(game) &&
                          ExistsW(Join(game, L"index.html")) &&
                          ExistsW(Join(game, L"js\\CPlants.js")) &&
                          ExistsW(Join(game, L"js\\plants\\oGun.js")) &&
                          ExistsW(Join(game, L"images\\Plants\\SeaShroom\\SeaShroom.gif")) &&
                          ExistsW(Join(g_exeDir, L"WebView2Loader.dll"));
            bool wv2 = WebView2RuntimePresent();
            char json[256];
            snprintf(json, sizeof json, "{\"layout\":%s,\"webview2Runtime\":%s}",
                     layout ? "true" : "false", wv2 ? "true" : "false");
            if (!outFile.empty()) {
                g_smokePath = outFile; /* reuse the JSON writer */
                g_done = false;
                HANDLE f = CreateFileW(outFile.c_str(), GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (f != INVALID_HANDLE_VALUE) {
                    DWORD written = 0;
                    WriteFile(f, json, (DWORD)strlen(json), &written, nullptr);
                    WriteFile(f, "\n", 1, &written, nullptr);
                    CloseHandle(f);
                }
                g_smokePath.clear();
            }
            printf("%s\n", json);
            fflush(stdout);
            return (layout && wv2) ? 0 : 1;
        }
    }
    if (argv) LocalFree(argv);

    if (!IsDirW(Join(g_exeDir, L"game"))) {
        MessageBoxW(nullptr,
                    L"The 'game' folder was not found next to PVZM.exe.\n"
                    L"Please extract the FULL portable ZIP (all folders) before "
                    L"starting the game.",
                    L"Plants vs. Zombies: MODDED — files missing", MB_ICONERROR | MB_OK);
        return 1;
    }

    SetProcessDPIAware();

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PVZMDesktopHost";
    if (!RegisterClassExW(&wc)) return 1;

    int w = 1280, h = 800;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    g_hwnd = CreateWindowExW(0, wc.lpszClassName,
                             L"Plants vs. Zombies: MODDED (v0.1.0-dev1)",
                             WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr,
                             hInst, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, nCmdShow);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    std::wstring userData = Join(KnownFolder(FOLDERID_LocalAppData), L"PVZM\\WebView2");
    Mkdirs(userData);

    EnvOptions* opts = new EnvOptions();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, userData.c_str(), opts,
                                                          new EnvCompleted());
    opts->Release();
    if (FAILED(hr)) {
        if (g_smoke) {
            char buf[128];
            snprintf(buf, sizeof buf, "{\"ok\":false,\"stage\":\"create\",\"hr\":%lu}",
                     (unsigned long)hr);
            SmokeFinish(buf, 2);
        } else {
            MessageBoxW(nullptr,
                        L"The Microsoft Edge WebView2 Runtime is required.\n"
                        L"Please install it from Microsoft, then try again.",
                        L"Plants vs. Zombies: MODDED — WebView2 required",
                        MB_ICONERROR | MB_OK);
        }
        return 2;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
