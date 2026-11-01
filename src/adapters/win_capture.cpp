// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <wtsapi32.h>
#include "win_capture.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <stdexcept>

namespace aitsnap {
std::mutex& desktopCaptureMutex(){static std::mutex mutex;return mutex;}
core::Error interactiveDesktopStatus()
{
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    wchar_t inputName[256]{}, threadName[256]{}; DWORD needed{};
    const bool sameDesktop = input &&
        GetUserObjectInformationW(input, UOI_NAME, inputName, sizeof(inputName), &needed) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, threadName, sizeof(threadName), &needed) &&
        std::wcscmp(inputName, threadName) == 0;
    if (input) CloseDesktop(input);
    if (!sameDesktop) return {"DESKTOP_UNAVAILABLE", "Input desktop is secure or unavailable"};
    // LockApp can cover Default without changing the desktop name. Query the
    // actual modern session lock flag; do not mistake its background for evidence.
    using NativeVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
    const auto nativeVersion = reinterpret_cast<NativeVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW version{}; version.dwOSVersionInfoSize = sizeof(version);
    const bool modernFlags = nativeVersion && nativeVersion(&version) == 0 &&
        (version.dwMajorVersion > 6 || (version.dwMajorVersion == 6 && version.dwMinorVersion >= 2));
    LPWSTR buffer{}; DWORD bytes{}; int state = -1; LONG flags = -1;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &buffer, &bytes)) {
        if (bytes >= sizeof(WTSINFOEXW)) { const auto* info = reinterpret_cast<const WTSINFOEXW*>(buffer);
            if (info->Level == 1) { state = int(info->Data.WTSInfoExLevel1.SessionState); flags = info->Data.WTSInfoExLevel1.SessionFlags; } }
        WTSFreeMemory(buffer);
    }
    if (state >= 0 && state != WTSActive) return {"SESSION_INACTIVE", "Interactive session is disconnected or inactive"};
    // Windows 7 flags have different historical behavior; its native gate stays
    // separate and retains the secure-desktop check above.
    if (modernFlags && flags == WTS_SESSIONSTATE_LOCK) return {"SESSION_LOCKED", "Windows session is locked; capture was not performed"};
    return {};
}
static std::string utf8(const wchar_t* value)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    std::string text(std::size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, text.data(), n, nullptr, nullptr);
    if (!text.empty()) text.pop_back();
    return text;
}
static BOOL CALLBACK enumerate(HMONITOR monitor, HDC, LPRECT bounds, LPARAM data)
{
    MONITORINFOEXW info{}; info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return FALSE;
    DISPLAY_DEVICEW device{}; device.cb = sizeof(device);
    EnumDisplayDevicesW(info.szDevice, 0, &device, 0);
    DEVMODEW mode{}; mode.dmSize = sizeof(mode);
    EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode);
    auto& result = *reinterpret_cast<std::vector<core::Display>*>(data);
    result.push_back({utf8(info.szDevice) + ":" + utf8(device.DeviceID),
        {bounds->left, bounds->top, bounds->right - bounds->left, bounds->bottom - bounds->top}, int(mode.dmDisplayOrientation)});
    return TRUE;
}
std::vector<core::Display> WinCaptureBackend::displays()
{
    std::vector<core::Display> result;
    if (!EnumDisplayMonitors(nullptr, nullptr, enumerate, reinterpret_cast<LPARAM>(&result)))
        throw std::runtime_error("Display enumeration failed");
    return result;
}
core::DisplayFrame WinCaptureBackend::acquire(const core::Display& d)
{
    core::DisplayFrame frame; frame.display = d;
    if (const auto status = interactiveDesktopStatus(); status) { frame.error = status; return frame; }
    if (!d.rect.valid() || std::int64_t(d.rect.width) * d.rect.height > 100000000) {
        frame.error = {"DISPLAY_SIZE", "Invalid/oversized display"}; return frame;
    }
    HDC screen = GetDC(nullptr);
    HDC memory = screen ? CreateCompatibleDC(screen) : nullptr;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = d.rect.width; info.bmiHeader.biHeight = -d.rect.height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = memory ? CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0) : nullptr;
    HGDIOBJ previous = bitmap ? SelectObject(memory, bitmap) : nullptr;
    const auto tick = [] { return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count(); };
    SYSTEMTIME utc{}; GetSystemTime(&utc);
    char text[40]{};
    std::snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", utc.wYear, utc.wMonth,
                  utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, utc.wMilliseconds);
    frame.acquiredAtUtc = text;
    frame.acquisitionStartNs = tick();
    const bool ok = bitmap && previous && previous != HGDI_ERROR &&
        BitBlt(memory, 0, 0, d.rect.width, d.rect.height, screen, d.rect.x, d.rect.y, SRCCOPY | CAPTUREBLT);
    GdiFlush();
    frame.acquisitionEndNs = tick();
    const auto lastError = GetLastError();
    // Allocate only after acquisition; always restore GDI objects even on allocation failure.
    std::shared_ptr<std::vector<std::uint8_t>> bytes;
    try {
        if (ok) {
            const auto size = std::size_t(d.rect.width) * std::size_t(d.rect.height) * 4;
            bytes = std::make_shared<std::vector<std::uint8_t>>(size);
            std::memcpy(bytes->data(), pixels, size);
            for (std::size_t i = 3; i < size; i += 4) (*bytes)[i] = 255;
        }
    } catch (...) {
        if (previous && previous != HGDI_ERROR) SelectObject(memory, previous);
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        throw;
    }
    if (previous && previous != HGDI_ERROR) SelectObject(memory, previous);
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    if (screen) ReleaseDC(nullptr, screen);
    if (ok) frame.image = {d.rect.width, d.rect.height, bytes};
    else frame.error = {"GDI_ACQUIRE", "BitBlt unavailable, Win32 error " + std::to_string(lastError)};
    return frame;
}
} // namespace aitsnap
