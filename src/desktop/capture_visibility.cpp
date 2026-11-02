// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "capture_visibility.h"
#include <QApplication>
#include <QWidget>
namespace aitsnap {
static bool exclusionEnabled=false;
void setOwnCaptureExclusionEnabled(bool enabled){exclusionEnabled=enabled;}
bool excludeOwnCaptureWindow(QWidget& window)
{
    // Content exclusion can blank the real window on some display drivers.
    // Compatibility is the startup default; exclusion is an explicit opt-in.
    if(QGuiApplication::platformName()!="windows")return false;
    if(!exclusionEnabled){
        // Hide must remove the actual surface before GDI acquisition, rather
        // than leaving a fading shadow after DwmFlush's next present.
        const BOOL disabled=TRUE;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(window.winId()),DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled));
        return false;
    }
    using Version=LONG(WINAPI*)(OSVERSIONINFOW*);
    const auto query=reinterpret_cast<Version>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion"));
    OSVERSIONINFOW version{};version.dwOSVersionInfoSize=sizeof(version);
    if(!query || query(&version)!=0 || version.dwMajorVersion<10 || (version.dwMajorVersion==10 && version.dwBuildNumber<19041))return false;
    const auto hwnd=reinterpret_cast<HWND>(window.winId());DWORD affinity{};
    if(GetWindowDisplayAffinity(hwnd,&affinity) && affinity==0x11)return true;
    return SetWindowDisplayAffinity(hwnd,0x11)!=FALSE;
}
std::vector<core::Rect> unexcludedOwnWindowRects()
{
    std::vector<core::Rect> result;
    for(auto* window:QApplication::topLevelWidgets())if(window->isVisible() && !excludeOwnCaptureWindow(*window)){
        RECT rect{};if(GetWindowRect(reinterpret_cast<HWND>(window->winId()),&rect))result.push_back({rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top});
    }
    return result;
}
}
