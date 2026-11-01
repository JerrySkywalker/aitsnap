// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "capture_visibility.h"
#include <QApplication>
#include <QWidget>
namespace aitsnap {
bool excludeOwnCaptureWindow(QWidget& window)
{
    if(QGuiApplication::platformName()!="windows")return false;
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
