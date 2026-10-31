// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include "core/geometry.h"
#include "adapters/win_capture.h"
#include "compat/qt_widgets_compat.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

using aitsnap::core::Rect;
using aitsnap::core::intersect;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static LRESULT CALLBACK paintPattern(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT a{0, 0, 32, 64}, b{32, 0, 64, 64};
        auto red = CreateSolidBrush(RGB(255, 0, 0));
        auto green = CreateSolidBrush(RGB(0, 255, 0));
        FillRect(dc, &a, red); FillRect(dc, &b, green);
        DeleteObject(red); DeleteObject(green); EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
static BOOL CALLBACK monitorProbe(HMONITOR monitor, HDC, LPRECT bounds, LPARAM count)
{
    MONITORINFOEXW info{}; info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return FALSE;
    auto hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"AITSnapPixelProbe", L"Synthetic pixel probe", WS_POPUP,
        bounds->left + 80, bounds->top + 80, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) return FALSE;
    HWND foreground = GetForegroundWindow();
    ShowWindow(hwnd, SW_SHOWNOACTIVATE); UpdateWindow(hwnd); DwmFlush(); Sleep(150);
    RECT actual{}; GetWindowRect(hwnd, &actual);
    DWORD cloaked{}; const auto cloakResult = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    DWORD affinity{}; const auto affinityResult = GetWindowDisplayAffinity(hwnd, &affinity);
    RECT physical{}; const auto physicalResult = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &physical, sizeof(physical));
    using GetDpi = UINT(WINAPI*)(HWND); const auto getDpi = reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    HDC ownDc = GetDC(hwnd); const auto ownRed = ownDc ? GetPixel(ownDc, 8, 8) : CLR_INVALID; if (ownDc) ReleaseDC(hwnd, ownDc);
    HDC screen = GetDC(nullptr), memory = CreateCompatibleDC(screen);
    auto bitmap = CreateCompatibleBitmap(screen, 64, 64);
    auto previous = SelectObject(memory, bitmap);
    auto start = std::chrono::steady_clock::now();
    bool ok = BitBlt(memory, 0, 0, 64, 64, screen, bounds->left + 80, bounds->top + 80,
                     SRCCOPY | CAPTUREBLT) != FALSE;
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    const auto redPixel = GetPixel(memory, 8, 8), greenPixel = GetPixel(memory, 48, 48);
    ok = ok && redPixel == RGB(255, 0, 0) && greenPixel == RGB(0, 255, 0);
    std::cout << "PATTERN_WINDOW visible=" << IsWindowVisible(hwnd) << " actual_rect=" << actual.left << ',' << actual.top << ',' << actual.right - actual.left << ',' << actual.bottom - actual.top
              << " red_colorref=" << redPixel << " green_colorref=" << greenPixel << " own_red_colorref=" << ownRed << " cloak_hr=" << cloakResult << " cloaked=" << cloaked << " affinity_ok=" << affinityResult << " affinity=" << affinity << '\n';
    std::cout << "PHYSICAL_WINDOW hr=" << physicalResult << " rect=" << physical.left << ',' << physical.top << ',' << physical.right - physical.left << ',' << physical.bottom - physical.top << " dpi=" << (getDpi ? getDpi(hwnd) : 0) << " process_dpi_aware=" << IsProcessDPIAware() << '\n';
    bool focus = foreground == GetForegroundWindow();
    SelectObject(memory, previous); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr, screen);
    DestroyWindow(hwnd);
    std::cout << "MONITOR rect=" << bounds->left << ',' << bounds->top << ','
              << bounds->right - bounds->left << ',' << bounds->bottom - bounds->top
              << " pattern=" << ok << " focus_preserved=" << focus << " bitblt_us=" << us << '\n';
    if (!ok || !focus) return FALSE;
    ++*reinterpret_cast<int*>(count);
    return TRUE;
}
int main(int argc, char** argv)
{
    aitsnap::compat::configureHighDpi();
    QApplication app(argc, argv);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); GetStartupInfoW(&startup);
    std::cout << "STARTUP flags=" << startup.dwFlags << " show=" << startup.wShowWindow << '\n';
    DWORD session{}; ProcessIdToSessionId(GetCurrentProcessId(), &session);
    wchar_t station[256]{}; DWORD length{}; GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, station, sizeof(station), &length);
    LPWSTR stateBuffer{}; DWORD stateBytes{}; int state = -1;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSConnectState, &stateBuffer, &stateBytes)) { if (stateBytes >= sizeof(WTS_CONNECTSTATE_CLASS)) state = int(*reinterpret_cast<WTS_CONNECTSTATE_CLASS*>(stateBuffer)); WTSFreeMemory(stateBuffer); }
    std::cout << "DESKTOP session=" << session << " console_session=" << WTSGetActiveConsoleSessionId() << " wts_state=" << state << " station=" << QString::fromWCharArray(station).toStdString() << " foreground=" << reinterpret_cast<std::uintptr_t>(GetForegroundWindow()) << '\n';
    LPWSTR infoBuffer{}; DWORD infoBytes{}; LONG sessionFlags = -1;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &infoBuffer, &infoBytes)) {
        if (infoBytes >= sizeof(WTSINFOEXW)) { const auto* info = reinterpret_cast<const WTSINFOEXW*>(infoBuffer); if (info->Level == 1) sessionFlags = info->Data.WTSInfoExLevel1.SessionFlags; }
        WTSFreeMemory(infoBuffer);
    }
    std::cout << "SESSION_FLAGS=" << sessionFlags << " (modern Windows LOCK=0 UNLOCK=1)\n";
    try {
        check(intersect({-1920, 0, 1920, 1080}, {-100, 50, 200, 100}) == Rect{-100, 50, 100, 100}, "negative intersection");
        check(!intersect({0, 0, 10, 10}, {10, 0, 10, 10}), "half-open edge");
        check(!intersect({0, 0, 0, 10}, {0, 0, 10, 10}), "invalid rectangle");
        QGraphicsScene scene;
        scene.setSceneRect(0, 0, 128, 64);
        scene.addRect(0, 0, 64, 64, Qt::NoPen, QBrush(Qt::red));
        scene.addRect(64, 0, 64, 64, Qt::NoPen, QBrush(Qt::green));
        QImage image(128, 64, QImage::Format_ARGB32);
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; ++i) {
            image.fill(Qt::black); QPainter painter(&image); scene.render(&painter);
        }
        check(image.pixelColor(8, 8) == QColor(Qt::red) && image.pixelColor(80, 8) == QColor(Qt::green), "scene pixels");
        std::cout << "GEOMETRY_SCENE_PASS Qt=" << qVersion() << " render_n=100 total_us="
                  << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() << '\n';
        if (app.arguments().contains(QStringLiteral("--native"))) {
            if (const auto status = aitsnap::interactiveDesktopStatus(); status) { std::cout << "NATIVE_HOLD " << status.code << ' ' << status.message << '\n'; return 2; }
            WNDCLASSW wc{}; wc.lpfnWndProc = paintPattern; wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"AITSnapPixelProbe";
            check(RegisterClassW(&wc) != 0, "register probe window");
            int count = 0;
            check(EnumDisplayMonitors(nullptr, nullptr, monitorProbe, reinterpret_cast<LPARAM>(&count)) != FALSE, "native pixel probe");
            check(count > 0, "no monitors");
            std::cout << "NATIVE_PIXEL_PASS monitor_count=" << count << '\n';
        }
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
