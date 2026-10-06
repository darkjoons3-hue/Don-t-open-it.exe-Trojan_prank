#pragma execution_character_set("utf-8")
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")

// ============================================================
// РЕСУРСЫ
// ============================================================
#define IDR_RANSOM 1001

// ============================================================
// КОНФИГ
// ============================================================
namespace Cfg {
    constexpr int kPhase1Ms = 10000;
    constexpr int kPhase2Ms = 20000;
    constexpr int kPhase3Ms = 20000;
    constexpr int kPhase4Ms = 30000;
    constexpr int kFinalMs  = 2000;
    constexpr int kMaxPrankMs = 180000;
}

// ============================================================
// ГЛОБАЛЫ
// ============================================================
std::atomic<bool> g_stop{false};
std::atomic<bool> g_audioStop{false};
std::atomic<int>  g_audioMode{0};
std::atomic<int>  g_byteFormula{0};
std::atomic<int>  g_volume{100};

HHOOK      g_kbHook = nullptr;
HINSTANCE  g_hInst  = nullptr;

const wchar_t* IE_CLASS     = L"IEClass";
const wchar_t* RULES_CLASS  = L"RulesClass";
const wchar_t* BLOOD_CLASS  = L"BloodClass";
const wchar_t* BLOCKS_CLASS = L"BlocksClass";

std::atomic<int>  g_textStretch{0};
std::atomic<int>  g_countdown{30};
std::atomic<int>  g_magentaAlpha{0};
std::atomic<int>  g_redAlpha{0};
std::atomic<int>  g_whiteBlocks{0};
std::atomic<int>  g_vignette{0};
std::atomic<int>  g_flickerOn{0};
std::atomic<int>  g_bloodCount{0};

HWND g_fakeIe1 = nullptr, g_fakeIe2 = nullptr, g_fakeIe3 = nullptr;
HWND g_rulesWnd = nullptr;
HWND g_bloodWnd = nullptr;
HWND g_blocksWnd = nullptr;

std::atomic<bool> g_screamer{false};

// ============================================================
// FORWARD
// ============================================================
void PumpMessages();
void SleepPump(int ms);
bool IsElevated();
void RelaunchElevated();
LRESULT CALLBACK LLKeyboardProc(int, WPARAM, LPARAM);
void InstallKbHook();
void RemoveKbHook();
void SetupIFEO();
void CleanupIFEO_RunOnce();
void HideTaskbar();
void ShowTaskbar();
DWORD BytebeatSample(int idx, DWORD t);
void AudioThread();
LRESULT CALLBACK IeProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RulesProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BloodProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BlocksProc(HWND, UINT, WPARAM, LPARAM);
void RegisterAllClasses();
void Phase1_PixelShake();
void Phase2_Infection();
void Phase3_RgbChaos();
void Phase4_MrsMajor();
void DoReboot();
void WatchdogThread();

// ============================================================
// УТИЛИТЫ
// ============================================================
void PumpMessages() {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m); DispatchMessageW(&m);
    }
}
void SleepPump(int ms) {
    DWORD s = GetTickCount();
    while ((int)(GetTickCount() - s) < ms && !g_stop.load()) {
        PumpMessages(); Sleep(10);
    }
}

// ============================================================
// ПРАВА
// ============================================================
bool IsElevated() {
    HANDLE t = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return false;
    TOKEN_ELEVATION e = {}; DWORD sz = sizeof(e);
    bool ok = GetTokenInformation(t, TokenElevation, &e, sz, &sz) && e.TokenIsElevated;
    CloseHandle(t);
    return ok;
}
void RelaunchElevated() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.nShow  = SW_SHOW;
    ShellExecuteExW(&sei);
}

// ============================================================
// КЛАВИАТУРА
// ============================================================
LRESULT CALLBACK LLKeyboardProc(int n, WPARAM w, LPARAM l) {
    if (n == HC_ACTION && (w == WM_KEYDOWN || w == WM_SYSKEYDOWN ||
                           w == WM_KEYUP   || w == WM_SYSKEYUP)) return 1;
    return CallNextHookEx(g_kbHook, n, w, l);
}
void InstallKbHook() {
    g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, LLKeyboardProc, nullptr, 0);
}
void RemoveKbHook() {
    if (g_kbHook) { UnhookWindowsHookEx(g_kbHook); g_kbHook = nullptr; }
}

// ============================================================
// IFEO
// ============================================================
void SetupIFEO() {
    HKEY h;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\taskmgr.exe",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        const wchar_t* dbg = L"cmd.exe /c echo Ne tak bystro... & pause";
        RegSetValueExW(h, L"Debugger", 0, REG_SZ, (const BYTE*)dbg,
                       (DWORD)((wcslen(dbg)+1)*sizeof(wchar_t)));
        RegCloseKey(h);
    }
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\regedit.exe",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        const wchar_t* dbg = L"notepad.exe";
        RegSetValueExW(h, L"Debugger", 0, REG_SZ, (const BYTE*)dbg,
                       (DWORD)((wcslen(dbg)+1)*sizeof(wchar_t)));
        RegCloseKey(h);
    }
}
void CleanupIFEO_RunOnce() {
    HKEY h;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        const wchar_t* cmd =
            L"cmd.exe /c reg delete \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\taskmgr.exe\" /f "
            L"& reg delete \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\regedit.exe\" /f";
        RegSetValueExW(h, L"CleanIFEO", 0, REG_SZ, (const BYTE*)cmd,
                       (DWORD)((wcslen(cmd)+1)*sizeof(wchar_t)));
        RegCloseKey(h);
    }
}

// ============================================================
// ПАНЕЛЬ ЗАДАЧ
// ============================================================
void HideTaskbar() {
    HWND t = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (t) ShowWindow(t, SW_HIDE);
}
void ShowTaskbar() {
    HWND t = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (t) ShowWindow(t, SW_SHOW);
}

// ============================================================
// BYTEBEAT
// ============================================================
DWORD BytebeatSample(int idx, DWORD t) {
    switch (idx) {
        case 0: return 128;
        case 1: return (((t >> 8) | (t >> 9) | (t * (t >> 13))) & 0xFF);
        case 2: return (((t ^ (t >> 3)) * (t >> 8)) & 0xFF);
        case 3: return ((t >> 12) & 0xFF);
        case 4: return ((t << 3) & 0xFF);
        case 5: return (((t >> 4) ^ (t >> 8) ^ (t * (t >> 12))) & 0xFF);
        default: return 128;
    }
}
void AudioThread() {
    const int SR = 15800;
    const int BUF = 1024;
    const int N = 4;
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = SR;
    wf.wBitsPerSample = 8;
    wf.nBlockAlign = 1;
    wf.nAvgBytesPerSec = SR;

    HWAVEOUT h = nullptr;
    if (waveOutOpen(&h, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;

    std::vector<unsigned char> samp(N * BUF);
    std::vector<WAVEHDR> hdr(N);
    for (int i = 0; i < N; ++i) {
        hdr[i] = {};
        hdr[i].lpData = (LPSTR)&samp[i * BUF];
        hdr[i].dwBufferLength = BUF;
        waveOutPrepareHeader(h, &hdr[i], sizeof(WAVEHDR));
    }
    DWORD t = 0; int cb = 0;
    while (!g_audioStop.load()) {
        int guard = 0;
        while (!(hdr[cb].dwFlags & WHDR_DONE)) {
            if (g_audioStop.load()) goto cleanup;
            Sleep(2);
            if (++guard > 500) break;
        }
        int mode = g_audioMode.load();
        int f = g_byteFormula.load();
        int v = g_volume.load();
        unsigned char* buf = (unsigned char*)hdr[cb].lpData;
        for (int i = 0; i < BUF; ++i) {
            DWORD raw = (mode == 1) ? 128 : BytebeatSample(f, t++);
            int s = (int)raw - 128;
            s = (s * v) / 100;
            if (s > 127) s = 127;
            if (s < -128) s = -128;
            buf[i] = (unsigned char)(s + 128);
        }
        hdr[cb].dwFlags &= ~WHDR_DONE;
        hdr[cb].dwBufferLength = BUF;
        waveOutWrite(h, &hdr[cb], sizeof(WAVEHDR));
        cb = (cb + 1) % N;
    }
cleanup:
    waveOutReset(h);
    for (int i = 0; i < N; ++i) waveOutUnprepareHeader(h, &hdr[i], sizeof(WAVEHDR));
    waveOutClose(h);
}

// ============================================================
// ЦИКЛ MESSAGEBOX
// ============================================================
DWORD WINAPI MsgBoxThread1(LPVOID) {
    const wchar_t* msg =
        L"Системный процесс завершился с ошибкой.\n\n"
        L"Код ошибки: 0xC000021A\n"
        L"Адрес: 0xFFFFF8032A41B080";
    while (!g_stop.load()) {
        MessageBoxW(nullptr, msg, L"Microsoft Windows",
                    MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TOPMOST);
    }
    return 0;
}
DWORD WINAPI MsgBoxThread2(LPVOID) {
    const wchar_t* msg =
        L"Критическая ошибка ядра системы.\n\n"
        L"Код ошибки: 0x000000EF\n"
        L"Драйвер: ntoskrnl.exe";
    while (!g_stop.load()) {
        MessageBoxW(nullptr, msg, L"Microsoft Windows",
                    MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TOPMOST);
    }
    return 0;
}

// ============================================================
// ФАЗА 1
// ============================================================
void Phase1_PixelShake() {
    CreateThread(nullptr, 0, MsgBoxThread1, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, MsgBoxThread2, nullptr, 0, nullptr);

    g_byteFormula = 1;
    g_volume = 180;
    g_audioMode = 0;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);

    const int BUFW = 480, BUFH = 270;
    HDC hSmall = CreateCompatibleDC(hScreen);
    HBITMAP bmpSmall = CreateCompatibleBitmap(hScreen, BUFW, BUFH);
    HBITMAP oldS = (HBITMAP)SelectObject(hSmall, bmpSmall);

    DWORD start = GetTickCount();
    while (!g_stop.load() && (int)(GetTickCount() - start) < Cfg::kPhase1Ms) {
        BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);

        double p = (double)(GetTickCount() - start) / Cfg::kPhase1Ms;
        int blocks = (int)(240 - 160 * p);
        if (blocks < 30) blocks = 30;

        SetStretchBltMode(hSmall, COLORONCOLOR);
        StretchBlt(hSmall, 0, 0, blocks, blocks * sh / sw,
                   hMem, 0, 0, sw, sh, SRCCOPY);
        SetStretchBltMode(hScreen, COLORONCOLOR);
        StretchBlt(hScreen, 0, 0, sw, sh,
                   hSmall, 0, 0, blocks, blocks * sh / sw, SRCCOPY);

        int dx = (rand() % 5) - 2;
        int dy = (rand() % 5) - 2;
        BitBlt(hScreen, dx, dy, sw, sh, hMem, 0, 0, SRCCOPY);

        Sleep(30);
    }

    SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);
    SelectObject(hSmall, oldS); DeleteObject(bmpSmall); DeleteDC(hSmall);
    ReleaseDC(nullptr, hScreen);
}

// ============================================================
// ФАЗА 2
// ============================================================
void Phase2_Infection() {
    PlaySoundW(MAKEINTRESOURCEW(IDR_RANSOM), g_hInst,
               SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);

    g_audioMode = 1;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);

    std::vector<std::wstring> logLines;
    const wchar_t* files[] = {
        L"C:\\Windows\\System32\\kernel32.dll",
        L"C:\\Windows\\System32\\ntdll.dll",
        L"C:\\Windows\\System32\\svchost.exe",
        L"C:\\Windows\\System32\\user32.dll",
        L"C:\\Windows\\System32\\gdi32.dll",
        L"C:\\Users\\Admin\\Documents\\passwords.txt",
        L"C:\\Users\\Admin\\Documents\\photo.jpg",
        L"C:\\Windows\\System32\\winlogon.exe",
        L"C:\\Windows\\System32\\lsass.exe",
        L"C:\\Windows\\System32\\services.exe"
    };
    for (int i = 0; i < 40; ++i) {
        std::wstring line = L"[14:32:";
        wchar_t buf[8]; swprintf_s(buf, L"%02d", rand() % 60);
        line += buf; line += L"] ";
        line += files[rand() % 10];
        line += L"  — ";
        line += (rand() % 2 ? L"INFECTED" : L"ACCESSING");
        logLines.push_back(line);
    }

    DWORD start = GetTickCount();
    int lastLog = 0;
    while (!g_stop.load() && (int)(GetTickCount() - start) < Cfg::kPhase2Ms) {
        DWORD elapsed = GetTickCount() - start;

        int alpha = (int)(50.0 * elapsed / Cfg::kPhase2Ms);
        g_redAlpha = alpha;

        HDC hMem = CreateCompatibleDC(hScreen);
        HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
        HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);
        BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);

        HDC hRed = CreateCompatibleDC(hScreen);
        HBITMAP bRed = CreateCompatibleBitmap(hScreen, sw, sh);
        HBITMAP oldR = (HBITMAP)SelectObject(hRed, bRed);
        HBRUSH redBrush = CreateSolidBrush(RGB(120, 0, 0));
        RECT full = { 0, 0, sw, sh };
        FillRect(hRed, &full, redBrush);
        DeleteObject(redBrush);

        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
        AlphaBlend(hMem, 0, 0, sw, sh, hRed, 0, 0, sw, sh, bf);

        // ============ ЧЕРЕП ============
        {
            int cx = sw / 2;
            int cy = sh / 2;
            int R = (sw < sh ? sw : sh) / 6;

            for (int i = 25; i > 0; --i) {
                HBRUSH auraB = CreateSolidBrush(RGB(150, 0, 0));
                HPEN auraP = CreatePen(PS_SOLID, 1, RGB(150, 0, 0));
                HGDIOBJ ao = SelectObject(hMem, auraB);
                HGDIOBJ ap = SelectObject(hMem, auraP);
                int rr = R + i * 4;
                Ellipse(hMem, cx - rr, cy - rr - 30, cx + rr, cy + rr + 30);
                SelectObject(hMem, ao);
                SelectObject(hMem, ap);
                DeleteObject(auraB);
                DeleteObject(auraP);
            }

            HBRUSH skullBrush = CreateSolidBrush(RGB(225, 225, 220));
            HPEN   skullPen   = CreatePen(PS_SOLID, 3, RGB(70, 70, 70));
            HGDIOBJ so = SelectObject(hMem, skullBrush);
            HGDIOBJ sp = SelectObject(hMem, skullPen);
            Ellipse(hMem, cx - R, cy - R - 20, cx + R, cy + R - 40);
            RoundRect(hMem, cx - R + 30, cy + R - 70,
                      cx + R - 30, cy + R + 40, 25, 25);
            SelectObject(hMem, so);
            SelectObject(hMem, sp);
            DeleteObject(skullBrush);
            DeleteObject(skullPen);

            HBRUSH eyeHole = CreateSolidBrush(RGB(5, 0, 0));
            HPEN   eyePen  = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            HGDIOBJ eo = SelectObject(hMem, eyeHole);
            HGDIOBJ ep = SelectObject(hMem, eyePen);
            int eyeW = R / 2;
            int eyeH = R / 2 + 15;
            int eyeY = cy - R / 3;
            Ellipse(hMem, cx - R/2 - eyeW/2, eyeY,
                          cx - R/2 + eyeW/2, eyeY + eyeH);
            Ellipse(hMem, cx + R/2 - eyeW/2, eyeY,
                          cx + R/2 + eyeW/2, eyeY + eyeH);
            SelectObject(hMem, eo);
            SelectObject(hMem, ep);
            DeleteObject(eyeHole);
            DeleteObject(eyePen);

            HBRUSH pupil = CreateSolidBrush(RGB(255, 0, 0));
            HGDIOBJ po = SelectObject(hMem, pupil);
            HPEN np = (HPEN)SelectObject(hMem, GetStockObject(NULL_PEN));
            int pR = R / 8;
            int pY = eyeY + eyeH / 2;
            Ellipse(hMem, cx - R/2 - pR, pY - pR, cx - R/2 + pR, pY + pR);
            Ellipse(hMem, cx + R/2 - pR, pY - pR, cx + R/2 + pR, pY + pR);
            SelectObject(hMem, po);
            SelectObject(hMem, np);
            DeleteObject(pupil);

            HBRUSH noseB = CreateSolidBrush(RGB(10, 0, 0));
            HGDIOBJ no = SelectObject(hMem, noseB);
            POINT nosePts[3] = {
                { cx, cy + R / 4 },
                { cx - R / 7, cy + R / 2 },
                { cx + R / 7, cy + R / 2 }
            };
            Polygon(hMem, nosePts, 3);
            SelectObject(hMem, no);
            DeleteObject(noseB);

            HPEN toothPen = CreatePen(PS_SOLID, 3, RGB(60, 60, 60));
            HGDIOBJ to = SelectObject(hMem, toothPen);
            int teethY1 = cy + R;
            int teethY2 = cy + R + 30;
            for (int i = -3; i <= 3; ++i) {
                int x = cx + i * (R / 4);
                MoveToEx(hMem, x, teethY1, nullptr);
                LineTo(hMem, x, teethY2);
            }
            SelectObject(hMem, to);
            DeleteObject(toothPen);

            HPEN crackPen = CreatePen(PS_SOLID, 2, RGB(70, 70, 70));
            HGDIOBJ co = SelectObject(hMem, crackPen);
            MoveToEx(hMem, cx - R/3, cy - R - 15, nullptr);
            LineTo(hMem, cx - R/4, cy - R/2);
            LineTo(hMem, cx - R/3, cy - R/6);
            LineTo(hMem, cx - R/5, cy + R/8);
            SelectObject(hMem, co);
            DeleteObject(crackPen);
        }
        // ============ /ЧЕРЕП ============

        HFONT f = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
        HGDIOBJ of = SelectObject(hMem, f);
        SetBkMode(hMem, TRANSPARENT);
        int linesPerSec = 8;
        int linesToShow = (int)(elapsed / (1000 / linesPerSec));
        if (linesToShow > (int)logLines.size()) linesToShow = (int)logLines.size();
        for (int i = 0; i < linesToShow; ++i) {
            int y = 20 + i * 22;
            if (y > sh - 30) break;
            bool infected = logLines[i].find(L"INFECTED") != std::wstring::npos;
            SetTextColor(hMem, infected ? RGB(255, 0, 0) : RGB(0, 255, 0));
            TextOutW(hMem, 20, y, logLines[i].c_str(), (int)logLines[i].size());
            if (infected && i > lastLog && (i % 3 == 0)) {
                HBRUSH fl = CreateSolidBrush(RGB(255, 0, 0));
                RECT rf = { 0, 0, sw, sh };
                FillRect(hMem, &rf, fl);
                DeleteObject(fl);
            }
            if (i > lastLog) lastLog = i;
        }
        SelectObject(hMem, of); DeleteObject(f);

        BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);

        SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);
        SelectObject(hRed, oldR); DeleteObject(bRed); DeleteDC(hRed);

        static int opened = 0;
        int shouldOpen = (int)(elapsed / 4000);
        if (shouldOpen > opened && opened < 5) {
            const wchar_t* urls[] = {
                L"https://www.google.com/search?q=%D1%81%D0%BA%D0%B0%D1%87%D0%B0%D1%82%D1%8C+%D0%B1%D0%B5%D1%81%D0%BF%D0%BB%D0%B0%D1%82%D0%BD%D0%BE+%D1%82%D1%80%D0%BE%D1%8F%D0%BD",
                L"https://www.google.com/search?q=%D0%BA%D0%B0%D0%BA+%D1%83%D0%B4%D0%B0%D0%BB%D0%B8%D1%82%D1%8C+%D0%B2%D0%B8%D1%80%D1%83%D1%81",
                L"https://www.google.com/search?q=%D0%BC%D0%BE%D0%B9+%D0%BA%D0%BE%D0%BC%D0%BF%D1%8C%D1%8E%D1%82%D0%B5%D1%80+%D0%B7%D0%B0%D1%80%D0%B0%D0%B6%D1%91%D0%BD",
                L"https://www.google.com/search?q=%D0%B1%D0%B5%D1%81%D0%BF%D0%BB%D0%B0%D1%82%D0%BD%D1%8B%D0%B9+%D0%B0%D0%BD%D1%82%D0%B8%D0%B2%D0%B8%D1%80%D1%83%D1%81",
                L"https://www.google.com/search?q=don%27t+open+it+exe"
            };
            ShellExecuteW(nullptr, L"open", urls[opened], nullptr, nullptr, SW_SHOW);
            opened++;
        }

        Sleep(60);
    }

    PlaySoundW(nullptr, nullptr, 0);
    ReleaseDC(nullptr, hScreen);
}

// ============================================================
// ФАЗА 3
// ============================================================
void Phase3_RgbChaos() {
    g_audioMode = 0;
    g_byteFormula = 2;
    g_volume = 140;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);

    const int BUFW = 320, BUFH = 180;
    HDC hSmall = CreateCompatibleDC(hScreen);
    HBITMAP bmpSmall = CreateCompatibleBitmap(hScreen, BUFW, BUFH);
    HBITMAP oldS = (HBITMAP)SelectObject(hSmall, bmpSmall);

    std::vector<unsigned char> pixels(BUFW * BUFH * 4);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = BUFW;
    bi.bmiHeader.biHeight = -BUFH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    DWORD start = GetTickCount();
    int lastIe = 0;
    while (!g_stop.load() && (int)(GetTickCount() - start) < Cfg::kPhase3Ms) {
        DWORD elapsed = GetTickCount() - start;

        BitBlt(hSmall, 0, 0, BUFW, BUFH, hScreen, 0, 0, SRCCOPY);
        GetDIBits(hSmall, bmpSmall, 0, BUFH, pixels.data(), &bi, DIB_RGB_COLORS);
        for (int i = 0; i < BUFW * BUFH; ++i) {
            int ch = rand() % 3;
            unsigned char v = pixels[i*4 + (2-ch)];
            pixels[i*4+0] = (ch == 2) ? v : 0;
            pixels[i*4+1] = (ch == 1) ? v : 0;
            pixels[i*4+2] = (ch == 0) ? v : 0;
        }
        SetDIBits(hSmall, bmpSmall, 0, BUFH, pixels.data(), &bi, DIB_RGB_COLORS);
        SetStretchBltMode(hScreen, COLORONCOLOR);
        StretchBlt(hScreen, 0, 0, sw, sh, hSmall, 0, 0, BUFW, BUFH, SRCCOPY);

        if (elapsed > 3000) {
            int alpha = (int)(60.0 * (elapsed - 3000) / (Cfg::kPhase3Ms - 3000));
            if (elapsed > Cfg::kPhase3Ms - 5000) {
                alpha = (int)(60.0 * (Cfg::kPhase3Ms - elapsed) / 5000.0);
            }
            if (alpha < 0) alpha = 0;
            if (alpha > 60) alpha = 60;
            g_magentaAlpha = alpha;

            HDC hMem = CreateCompatibleDC(hScreen);
            HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
            HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);
            BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);
            HDC hM = CreateCompatibleDC(hScreen);
            HBITMAP bM = CreateCompatibleBitmap(hScreen, sw, sh);
            HBITMAP oM = (HBITMAP)SelectObject(hM, bM);
            HBRUSH mb = CreateSolidBrush(RGB(255, 0, 255));
            RECT full = { 0, 0, sw, sh };
            FillRect(hM, &full, mb);
            DeleteObject(mb);
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
            AlphaBlend(hMem, 0, 0, sw, sh, hM, 0, 0, sw, sh, bf);
            BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);
            SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);
            SelectObject(hM, oM); DeleteObject(bM); DeleteDC(hM);
        }

        int shouldShow = (int)(elapsed / 3000);
        if (shouldShow > lastIe) {
            if (lastIe == 0)      PostMessageW(g_fakeIe1, WM_USER + 1, 0, 0);
            else if (lastIe == 1) PostMessageW(g_fakeIe2, WM_USER + 1, 0, 0);
            else if (lastIe == 2) PostMessageW(g_fakeIe3, WM_USER + 1, 0, 0);
            lastIe++;
        }

        if (elapsed > 6000) {
            g_textStretch = (int)((elapsed - 6000) / 300);
        }

        if (elapsed > 9000) {
            g_whiteBlocks = (int)((elapsed - 9000) / 500);
            if (g_whiteBlocks > 50) g_whiteBlocks = 50;
            InvalidateRect(g_blocksWnd, nullptr, FALSE);
        }

        Sleep(80);
    }

    if (g_fakeIe1) ShowWindow(g_fakeIe1, SW_HIDE);
    if (g_fakeIe2) ShowWindow(g_fakeIe2, SW_HIDE);
    if (g_fakeIe3) ShowWindow(g_fakeIe3, SW_HIDE);
    if (g_blocksWnd) ShowWindow(g_blocksWnd, SW_HIDE);

    SelectObject(hSmall, oldS); DeleteObject(bmpSmall); DeleteDC(hSmall);
    ReleaseDC(nullptr, hScreen);
}

// ============================================================
// ФАЗА 4
// ============================================================
void Phase4_MrsMajor() {
    HideTaskbar();

    g_byteFormula = 3;
    g_volume = 50;
    g_audioMode = 0;

    PostMessageW(g_rulesWnd, WM_USER + 1, 0, 0);
    ShowWindow(g_bloodWnd, SW_SHOW);
    InvalidateRect(g_bloodWnd, nullptr, FALSE);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);

    DWORD start = GetTickCount();
    int lastCount = 30;
    while (!g_stop.load() && (int)(GetTickCount() - start) < Cfg::kPhase4Ms) {
        DWORD elapsed = GetTickCount() - start;

        int remain = 30 - (int)(elapsed / 1000);
        if (remain < 0) remain = 0;
        if (remain != lastCount) {
            lastCount = remain;
            g_countdown = remain;
            Beep(2000, 50);
            InvalidateRect(g_rulesWnd, nullptr, FALSE);
        }
        if (remain <= 10) {
            static DWORD lastTick = 0;
            if (GetTickCount() - lastTick > 500) {
                Beep(2500, 40);
                lastTick = GetTickCount();
            }
        }

        g_volume = 50 + (int)(150.0 * elapsed / Cfg::kPhase4Ms);
        if (elapsed > Cfg::kPhase4Ms - 5000) {
            g_byteFormula = 4;
        }

        g_redAlpha = (int)(15 + 35.0 * elapsed / Cfg::kPhase4Ms);

        int p = (int)(elapsed * 100 / Cfg::kPhase4Ms);
        g_magentaAlpha = 10 + p * 40 / 100;
        g_whiteBlocks = p * 30 / 100;
        g_vignette = p;
        g_flickerOn = (p > 50);

        if (g_flickerOn.load() && (rand() % 3 == 0)) {
            HDC hMem = CreateCompatibleDC(hScreen);
            HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
            HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);
            BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);
            HBRUSH bb = CreateSolidBrush(RGB(0,0,0));
            RECT full = { 0, 0, sw, sh };
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 15, 0 };
            HDC hB = CreateCompatibleDC(hScreen);
            HBITMAP bB = CreateCompatibleBitmap(hScreen, sw, sh);
            HBITMAP oB = (HBITMAP)SelectObject(hB, bB);
            FillRect(hB, &full, bb);
            AlphaBlend(hMem, 0, 0, sw, sh, hB, 0, 0, sw, sh, bf);
            BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);
            SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);
            SelectObject(hB, oB); DeleteObject(bB); DeleteDC(hB);
            DeleteObject(bb);
        }

        if ((int)(elapsed / 2000) > g_bloodCount.load()) {
            g_bloodCount = (int)(elapsed / 2000) + 3;
            InvalidateRect(g_bloodWnd, nullptr, FALSE);
        }
        InvalidateRect(g_bloodWnd, nullptr, FALSE);

        {
            HDC hMem = CreateCompatibleDC(hScreen);
            HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
            HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);
            BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);
            HDC hR = CreateCompatibleDC(hScreen);
            HBITMAP bR = CreateCompatibleBitmap(hScreen, sw, sh);
            HBITMAP oR = (HBITMAP)SelectObject(hR, bR);
            HBRUSH rb = CreateSolidBrush(RGB(120, 0, 0));
            RECT full = { 0, 0, sw, sh };
            FillRect(hR, &full, rb);
            DeleteObject(rb);
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)g_redAlpha.load(), 0 };
            AlphaBlend(hMem, 0, 0, sw, sh, hR, 0, 0, sw, sh, bf);
            BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);
            SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);
            SelectObject(hR, oR); DeleteObject(bR); DeleteDC(hR);
        }

        Sleep(100);
    }

    // СКРИМЕР
    g_screamer = true;
    PlaySoundW(nullptr, nullptr, 0);
    Beep(80, 500);
    g_volume = 200;
    g_byteFormula = 5;
    g_audioMode = 0;
    SleepPump(500);

    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);

    HBRUSH rb = CreateSolidBrush(RGB(200, 0, 0));
    RECT full = { 0, 0, sw, sh };
    FillRect(hMem, &full, rb);
    DeleteObject(rb);

    HBRUSH bb = CreateSolidBrush(RGB(0, 0, 0));
    HGDIOBJ ob = SelectObject(hMem, bb);
    HPEN np = (HPEN)SelectObject(hMem, GetStockObject(NULL_PEN));
    int cx = sw / 2, cy = sh / 2;
    Ellipse(hMem, cx - 200, cy - 250, cx + 200, cy + 250);
    Rectangle(hMem, cx - 250, cy + 100, cx + 250, cy + 400);
    HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hMem, wb);
    Ellipse(hMem, cx - 120, cy - 80, cx - 30, cy + 30);
    Ellipse(hMem, cx + 30, cy - 80, cx + 120, cy + 30);
    SelectObject(hMem, ob); SelectObject(hMem, np);
    DeleteObject(bb); DeleteObject(wb);

    SetBkMode(hMem, TRANSPARENT);
    SetTextColor(hMem, RGB(255, 255, 255));
    HFONT f = CreateFontW(72, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, 0, 0, L"Impact");
    HGDIOBJ of = SelectObject(hMem, f);
    const wchar_t* txt = L"ТЫ УМРЁШЬ";
    RECT tr = { 0, cy + 300, sw, cy + 450 };
    DrawTextW(hMem, txt, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hMem, of); DeleteObject(f);

    BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);

    SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);

    SleepPump(500);
    g_audioMode = 1;
    ReleaseDC(nullptr, hScreen);
}

// ============================================================
// ОКНО IE
// ============================================================
static std::wstring StretchText(const std::wstring& base, int n) {
    std::wstring out;
    int added = 0;
    for (wchar_t c : base) {
        out += c;
        if (c == L' ' && added < n) {
            for (int k = 0; k < 3 && added < n; ++k) { out += L' '; ++added; }
        }
    }
    return out;
}

LRESULT CALLBACK IeProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static int ieIndex = 0;
    if (m == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)l;
        ieIndex = (int)(INT_PTR)cs->lpCreateParams;
    }
    if (m == WM_USER + 1) { ShowWindow(h, SW_SHOWNA); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(dc, &rc, bg); DeleteObject(bg);
        HBRUSH tb = CreateSolidBrush(RGB(30, 90, 180));
        RECT tr = { 0, 0, rc.right, 60 }; FillRect(dc, &tr, tb); DeleteObject(tb);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
        HFONT f = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
        HGDIOBJ of = SelectObject(dc, f);
        const wchar_t* addr = (ieIndex == 2) ? L"http://go.microsoft.com/fwlink/p/?LinkId=255141"
                                             : L"http://www.example.com";
        TextOutW(dc, 40, 20, addr, (int)wcslen(addr));
        SetTextColor(dc, RGB(0,0,0));
        HFONT fb = CreateFontW(20, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
        SelectObject(dc, fb);
        std::wstring base;
        if (ieIndex == 0) base = L"Не удалось подключиться к сети Интернет.\nПроверьте кабель и попробуйте снова.";
        else if (ieIndex == 1) base = L"Страница не может быть отображена.\nКод ошибки: 0x800C0005";
        else base = L"Использовать рекомендуемые параметры безопасности.\nОтправлять запрос Do Not Track на сайты.\nСкачать бесплатно троян без СМС и регистрации.";
        std::wstring stretched = StretchText(base, g_textStretch.load());
        RECT cr = { 40, 80, rc.right - 40, rc.bottom - 40 };
        DrawTextW(dc, stretched.c_str(), -1, &cr, DT_LEFT | DT_WORDBREAK);
        SelectObject(dc, of); DeleteObject(f); DeleteObject(fb);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_TIMER) { InvalidateRect(h, nullptr, FALSE); return 0; }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
// ОКНО ПРАВИЛ
// ============================================================
LRESULT CALLBACK RulesProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 1) { ShowWindow(h, SW_SHOW); SetTimer(h, 1, 100, nullptr); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(20, 0, 0));
        FillRect(dc, &rc, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);

        int sx = (rand() % 3) - 1;
        int sy = (rand() % 3) - 1;

        HFONT f = CreateFontW(18, 0, 0, 0, FW_BOLD, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
        HGDIOBJ of = SelectObject(dc, f);
        SetTextColor(dc, RGB(220, 50, 50));
        const wchar_t* title = L"ДОБРО ПОЖАЛОВАТЬ.";
        TextOutW(dc, 30 + sx, 20 + sy, title, (int)wcslen(title));

        SetTextColor(dc, RGB(200, 200, 200));
        const wchar_t* rules[] = {
            L"ПРАВИЛА ПРОСТЫ:",
            L"  1. Не закрывай это окно.",
            L"  2. Не открывай диспетчер задач.",
            L"  3. Не отводи взгляд от экрана.",
            L"  4. Не смотри назад.",
            L"",
            L"У тебя есть время."
        };
        for (int i = 0; i < 7; ++i) {
            TextOutW(dc, 30 + sx, 60 + i * 26 + sy, rules[i], (int)wcslen(rules[i]));
        }

        wchar_t tb[32];
        int rem = g_countdown.load();
        swprintf_s(tb, L"%02d:%02d", rem / 60, rem % 60);
        HFONT ft = CreateFontW(42, 0, 0, 0, FW_BOLD, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
        SelectObject(dc, ft);
        SetTextColor(dc, rem <= 5 ? RGB(255, 0, 0) : RGB(220, 50, 50));
        RECT tr = { 30, rc.bottom - 80, rc.right - 30, rc.bottom - 20 };
        DrawTextW(dc, tb, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, of); DeleteObject(f); DeleteObject(ft);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_TIMER) { InvalidateRect(h, nullptr, FALSE); return 0; }
    if (m == WM_CLOSE) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        HDC hScreen = GetDC(nullptr);
        HBRUSH rb = CreateSolidBrush(RGB(255, 0, 0));
        RECT full = { 0, 0, sw, sh };
        FillRect(hScreen, &full, rb);
        DeleteObject(rb);
        Sleep(150);
        ReleaseDC(nullptr, hScreen);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
// ОКНО КРОВИ
// ============================================================
struct BloodDrop { int x, y, w, len, speed; };
std::vector<BloodDrop> g_drops;

LRESULT CALLBACK BloodProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        SetBkMode(dc, TRANSPARENT);
        int target = g_bloodCount.load();
        while ((int)g_drops.size() < target) {
            BloodDrop d;
            d.x = rand() % rc.right;
            d.y = 0;
            d.w = 5 + rand() % 15;
            d.len = 0;
            d.speed = 1 + rand() % 3;
            g_drops.push_back(d);
        }
        for (auto& d : g_drops) {
            HBRUSH rb = CreateSolidBrush(RGB(120, 0, 0));
            RECT dr = { d.x, 0, d.x + d.w, d.len };
            FillRect(dc, &dr, rb);
            HBRUSH r2 = CreateSolidBrush(RGB(160, 0, 0));
            Ellipse(dc, d.x - 2, d.len - 8, d.x + d.w + 2, d.len + 8);
            DeleteObject(rb); DeleteObject(r2);
            d.len += d.speed;
            if (d.len > rc.bottom) d.len = rc.bottom;
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_TIMER) { InvalidateRect(h, nullptr, FALSE); return 0; }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
// ОКНО БЕЛЫХ БЛОКОВ
// ============================================================
LRESULT CALLBACK BlocksProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int n = g_whiteBlocks.load();
        HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
        for (int i = 0; i < n; ++i) {
            int x = rand() % rc.right;
            int y = rand() % rc.bottom;
            int ww = 30 + rand() % 200;
            int hh = 20 + rand() % 100;
            RECT r = { x, y, x + ww, y + hh };
            FillRect(dc, &r, wb);
        }
        DeleteObject(wb);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_TIMER) { InvalidateRect(h, nullptr, FALSE); return 0; }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
// РЕГИСТРАЦИЯ КЛАССОВ
// ============================================================
void RegisterAllClasses() {
    WNDCLASSW wc = {};
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    wc.lpfnWndProc = IeProc;     wc.lpszClassName = IE_CLASS;     RegisterClassW(&wc);
    wc.lpfnWndProc = RulesProc;  wc.lpszClassName = RULES_CLASS;  RegisterClassW(&wc);
    wc.lpfnWndProc = BloodProc;  wc.lpszClassName = BLOOD_CLASS;  RegisterClassW(&wc);
    wc.lpfnWndProc = BlocksProc; wc.lpszClassName = BLOCKS_CLASS; RegisterClassW(&wc);
}

// ============================================================
// WATCHDOG
// ============================================================
void WatchdogThread() {
    DWORD s = GetTickCount();
    while (true) {
        if (g_stop.load()) return;
        if ((int)(GetTickCount() - s) > Cfg::kMaxPrankMs) {
            RemoveKbHook();
            ShowTaskbar();
            g_audioStop = true;
            DoReboot();
            return;
        }
        Sleep(1000);
    }
}

// ============================================================
// РЕБУТ
// ============================================================
void DoReboot() {
    HANDLE t;
    TOKEN_PRIVILEGES tp;
    if (OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &t)) {
        LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(t, FALSE, &tp, 0, nullptr, nullptr);
        CloseHandle(t);
    }
    ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
}

// ============================================================
// ТОЧКА ВХОДА
// ============================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    SetProcessDPIAware();
    g_hInst = hInst;
    srand((unsigned)time(nullptr));

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DontOpenItMutex_v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    if (!IsElevated()) {
        RelaunchElevated();
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 0;
    }

    int r = MessageBoxW(nullptr,
        L"ВНИМАНИЕ!\n\n"
        L"Это БЕЗОБИДНЫЙ ПРАНК.\n"
        L"Ничего не удаляется, данные не крадутся.\n"
        L"Будет блокировка клавиатуры, эффекты и ребут в конце.\n\n"
        L"СОХРАНИ ВСЁ ОТКРЫТОЕ!\n\n"
        L"Запустить?",
        L"Don't open it.exe",
        MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
    if (r != IDYES) {
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 0;
    }

    InstallKbHook();
    SetupIFEO();
    CleanupIFEO_RunOnce();
    RegisterAllClasses();

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    g_fakeIe1 = CreateWindowExW(WS_EX_TOPMOST, IE_CLASS, L"Ошибка сети — Internet Explorer",
        WS_OVERLAPPEDWINDOW, 100, 100, 700, 450, nullptr, nullptr, g_hInst, (LPVOID)(INT_PTR)0);
    g_fakeIe2 = CreateWindowExW(WS_EX_TOPMOST, IE_CLASS, L"Страница не может быть отображена",
        WS_OVERLAPPEDWINDOW, 250, 180, 700, 450, nullptr, nullptr, g_hInst, (LPVOID)(INT_PTR)1);
    g_fakeIe3 = CreateWindowExW(WS_EX_TOPMOST, IE_CLASS, L"Настройка Internet Explorer 11",
        WS_OVERLAPPEDWINDOW, 400, 260, 700, 450, nullptr, nullptr, g_hInst, (LPVOID)(INT_PTR)2);
    SetTimer(g_fakeIe1, 1, 300, nullptr);
    SetTimer(g_fakeIe2, 1, 300, nullptr);
    SetTimer(g_fakeIe3, 1, 300, nullptr);

    g_rulesWnd = CreateWindowExW(WS_EX_TOPMOST, RULES_CLASS, L"Mrs. Major — Правила",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, sw/2 - 250, sh/2 - 200,
        500, 400, nullptr, nullptr, g_hInst, nullptr);

    g_bloodWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        BLOOD_CLASS, L"", WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, g_hInst, nullptr);
    SetLayeredWindowAttributes(g_bloodWnd, RGB(0,0,0), 0, LWA_COLORKEY);
    SetTimer(g_bloodWnd, 1, 80, nullptr);

    g_blocksWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        BLOCKS_CLASS, L"", WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, g_hInst, nullptr);
    SetTimer(g_blocksWnd, 1, 200, nullptr);

    std::thread tAudio(AudioThread);
    std::thread tWatch(WatchdogThread);

    Phase1_PixelShake();
    Phase2_Infection();
    Phase3_RgbChaos();
    Phase4_MrsMajor();

    g_audioMode = 1;
    g_stop = true;
    Sleep(2000);

    RemoveKbHook();
    ShowTaskbar();
    g_audioStop = true;
    if (tAudio.joinable()) tAudio.join();
    if (tWatch.joinable()) tWatch.join();

    DoReboot();
    if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    return 0;
}
