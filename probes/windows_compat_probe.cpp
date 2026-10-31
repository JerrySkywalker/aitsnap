// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors
// Capability probe only: no capture, canvas, tray icon, Word activation or files.
#include "compat/qt_widgets_compat.h"

#include <QApplication>
#include <QGuiApplication>
#include <QSystemTrayIcon>
#include <QtGlobal>

#include <windows.h>
#include <objbase.h>

#include <array>
#include <concepts>
#include <iostream>
#include <span>
#include <string_view>

static_assert(__cplusplus >= 202002L);
static_assert(std::integral<int>);

int main(int argc, char* argv[])
{
    bool requireWin7 = false;
    for(int i = 1; i < argc; ++i) {
        if(std::string_view(argv[i]) == "--require-win7") {
            requireWin7 = true;
        } else {
            std::cerr << "Usage: aitsnap-compat-probe [--require-win7]\n";
            return 64;
        }
    }

    // Resolve the native version query at runtime; do not use manifest-sensitive
    // GetVersionEx or report a compatibility-mode version as native evidence.
    OSVERSIONINFOEXW os{};
    os.dwOSVersionInfoSize = sizeof(os);
    using QueryVersion = LONG (WINAPI*)(OSVERSIONINFOEXW*);
    const auto query = reinterpret_cast<QueryVersion>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if(!query || query(&os) != 0) {
        std::cerr << "NATIVE_OS_QUERY=FAILED\n";
        return 3;
    }
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    const char* osArch = system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
        ? "x64" : system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL
        ? "x86" : "OTHER";
    const bool win7Sp1 = os.dwMajorVersion == 6 && os.dwMinorVersion == 1
        && os.wServicePackMajor >= 1 && os.wProductType == VER_NT_WORKSTATION;
    std::cout << "NATIVE_OS=" << os.dwMajorVersion << '.' << os.dwMinorVersion
              << '.' << os.dwBuildNumber << " SP" << os.wServicePackMajor << '\n'
              << "NATIVE_OS_ARCH=" << osArch << '\n'
              << "PROCESS_BITS=" << sizeof(void*) * 8 << '\n'
              << "WIN7_SP1_NATIVE=" << (win7Sp1 ? "YES" : "NO") << '\n';
    if(requireWin7 && !win7Sp1) {
        std::cout << "WIN7_RUNTIME=NOT_TESTED_ON_THIS_HOST\n";
        return 2;
    }

    constexpr std::array values{1, 2, 3};
    const std::span<const int> view(values);
    if(view.size() != 3 || view[2] != 3) {
        return 4;
    }

    aitsnap::compat::configureHighDpi();
    QApplication app(argc, argv);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comReady = SUCCEEDED(com);
    if(comReady) {
        CoUninitialize();
    }
    std::cout << "CXX20_SUBSET=PASS (span, concepts, language mode)\n"
              << "QT_COMPILED=" << QT_VERSION_STR << '\n'
              << "QT_RUNTIME=" << qVersion() << '\n'
              << "SCREEN_COUNT=" << QGuiApplication::screens().size() << '\n'
              << "TRAY_AVAILABLE=" << (QSystemTrayIcon::isSystemTrayAvailable() ? "YES" : "NO") << '\n'
              << "COM_STA=" << (comReady ? "READY" : "FAILED") << '\n'
              << "WORD_AUTOMATION=NOT_ATTEMPTED\n"
              << "WIN7_RUNTIME=" << (win7Sp1
                  ? (comReady ? "MINIMAL_NATIVE_PROBE_PASS" : "MINIMAL_NATIVE_PROBE_FAIL")
                  : "NOT_TESTED_ON_THIS_HOST") << '\n';
    return comReady ? 0 : 5;
}
