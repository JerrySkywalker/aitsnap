// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "main_window.h"
#include "adapters/win_capture.h"
#include "compat/qt_widgets_compat.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static LRESULT CALLBACK paint(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps);
        RECT a{0, 0, 100, 200}, b{100, 0, 200, 200};
        auto red = CreateSolidBrush(RGB(255, 0, 0)), green = CreateSolidBrush(RGB(0, 255, 0));
        FillRect(dc, &a, red); FillRect(dc, &b, green); DeleteObject(red); DeleteObject(green); EndPaint(hwnd, &ps); return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
int main(int argc, char** argv)
{
    compat::configureHighDpi(); QApplication app(argc, argv); app.setApplicationName("AITSnapTests"); app.setQuitOnLastWindowClosed(false);
    std::vector<HWND> windows;
    try {
        QTemporaryDir temp; check(temp.isValid(), "temp directory"); QString error;
        Settings settings = defaultSettings(); settings.outputDirectory = temp.path();
        core::CaptureProfile profile; profile.id = "synthetic"; profile.name = "Synthetic";
        profile.regions.push_back({"one", "One", {0, 0, 16, 16}, true}); settings.profiles.push_back(profile);
        const auto path = temp.filePath("settings.json"); check(saveSettings(path, settings, error), "save settings");
        Settings loaded; check(loadSettings(path, loaded, error) && loaded.profiles[0].regions[0].rect == profile.regions[0].rect, "restart persistence");
        QFile corrupt(temp.filePath("future.json")); check(corrupt.open(QIODevice::WriteOnly), "future settings file"); corrupt.write("{\"schemaVersion\":2}"); corrupt.close();
        check(!loadSettings(corrupt.fileName(), loaded, error), "future version rejected"); error.clear();
        QImage source(16, 16, QImage::Format_ARGB32); source.fill(Qt::red);
        for (const auto& format : {"PNG", "JPEG", "BMP"}) {
            auto file = temp.filePath(QString("format.") + QString(format).toLower());
            check(saveImageNew(source, file, format, error), "encode format");
            check(QImage(file).size() == source.size(), "decode format dimensions");
            check(!saveImageNew(source, file, format, error), "never overwrite"); error.clear();
        }
        check(!saveImageNew(source, temp.filePath("missing/subdir/file.png"), "PNG", error), "disk path failure explicit"); error.clear();
        core::CaptureRequest request; request.profile = profile; request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
        settings.filenameTemplate = "{region}_{unknown}"; check(expandFilename(settings, request, profile.regions[0], error).isEmpty() && !error.isEmpty(), "unknown filename token"); error.clear();
        settings.filenameTemplate = "../{region}"; check(!expandFilename(settings, request, profile.regions[0], error).contains('/'), "path traversal sanitized");
        {
            Hotkeys keys; app.installNativeEventFilter(&keys);
            check(keys.configure({"Ctrl+Alt+Shift+F13", "Ctrl+Alt+Shift+F14", "Ctrl+Alt+Shift+F15"}, error), "register hotkeys");
            check(!RegisterHotKey(nullptr, 0x4500, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F13), "hotkey collision reported by OS");
            check(keys.pause(true, error), "pause hotkeys");
            check(RegisterHotKey(nullptr, 0x4500, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F13), "paused key released");
            check(!keys.pause(false, error), "resume collision explicit"); UnregisterHotKey(nullptr, 0x4500); error.clear();
            check(keys.pause(false, error), "resume hotkeys");
            int triggered = -1; keys.triggered = [&](int mode) { triggered = mode; };
            PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY, 0x4150, 0);
            QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec();
            check(triggered == 0, "native hotkey event dispatch");
        }
        std::cout << "ADAPTER_PASS settings/migration, PNG/JPEG/BMP, no-overwrite, disk failure, filename, hotkey collision/pause/dispatch\n";
        if (app.arguments().contains("--native")) {
            WinCaptureBackend backend; profile.calibratedDisplays = backend.displays(); check(!profile.calibratedDisplays.empty(), "native displays");
            WNDCLASSW wc{}; wc.lpfnWndProc = paint; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AITSnapDesktopTest";
            check(RegisterClassW(&wc) != 0, "native pattern class");
            for (const auto& display : profile.calibratedDisplays) {
                HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"Synthetic AITSnap", WS_POPUP,
                    display.rect.x + 80, display.rect.y + 80, 200, 200, nullptr, nullptr, wc.hInstance, nullptr);
                check(window != nullptr, "native pattern window"); windows.push_back(window); ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window);
            }
            DwmFlush(); Sleep(150); profile.regions.clear();
            for (int i = 0; i < 5; ++i) {
                auto display = profile.calibratedDisplays[std::size_t(i) % profile.calibratedDisplays.size()];
                profile.regions.push_back({"r" + std::to_string(i), "Synthetic " + std::to_string(i), {display.rect.x + 90 + (i % 2) * 110, display.rect.y + 90 + i * 10, 16, 16}, true});
            }
            settings = defaultSettings(); settings.outputDirectory = temp.filePath("captures"); settings.profiles = {profile}; settings.clipboardRegion = "r1";
            settings.saveHotkey = "Ctrl+Alt+Shift+F20"; settings.copyHotkey = "Ctrl+Alt+Shift+F21"; settings.bothHotkey = "Ctrl+Alt+Shift+F22";
            check(saveSettings(path, settings, error), "synthetic main settings");
            const auto evidence = app.arguments().indexOf("--evidence");
            if (evidence >= 0 && evidence + 1 < app.arguments().size()) { settings.outputDirectory = app.arguments()[evidence + 1]; check(saveSettings(path, settings, error), "evidence config"); }
            MainWindow host(nullptr, path); QEventLoop wait;
            std::shared_ptr<CaptureOutcome> outcome;
            host.captureFinished = [&](auto result) { outcome = result; wait.quit(); };
            const auto foreground = GetForegroundWindow();
            QTimer::singleShot(0, &host, [&] { PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY, 0x4152, 0); });
            QTimer::singleShot(10000, &wait, &QEventLoop::quit); wait.exec();
            check(bool(outcome), "capture completion timeout");
            check(GetForegroundWindow() == foreground && !host.isVisible(), "background focus preserved");
            check(outcome->batch.captures.size() == 5, "five region batch");
            int saved = 0, copied = 0;
            for (const auto& capture : outcome->batch.captures) {
                check(!capture.error && capture.image.valid(), "native region acquired");
                const auto image = toQImage(capture.image); const int i = std::stoi(capture.region.id.substr(1));
                check(image.pixelColor(4, 4) == QColor(i % 2 ? Qt::green : Qt::red), "native exact region pixel");
            }
            for (const auto& output : outcome->batch.outputs) {
                if (output.error) std::cerr << output.error.code << ' ' << output.error.message << '\n';
                check(!output.error, "output failure"); if (output.status == "SAVED") ++saved; if (output.status == "COPIED") ++copied;
            }
            check(saved == 5 && copied == 1, "five files and one clipboard");
            check(QApplication::clipboard()->image().pixelColor(4, 4) == QColor(Qt::green), "selected clipboard image");
            check(QFile::exists(QDir(outcome->directory).filePath("batch.json")), "batch manifest saved");
            Settings restarted; check(loadSettings(path, restarted, error) && restarted.sequence == 1, "persistent capture sequence");
            std::cout << "NATIVE_M1_AUTOMATED_PASS displays=" << profile.calibratedDisplays.size() << " regions=5 saved=5 clipboard=1 focus=preserved elapsed_ms=" << outcome->elapsedMs << " manifest=" << outcome->directory.toStdString() << "/batch.json\n";
            if (app.arguments().contains("--benchmark")) {
                std::vector<double> elapsed, skews;
                core::CaptureRequest bench = outcome->batch.request;
                const auto run = QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString();
                for (int i = 0; i < 100; ++i) {
                    bench.requestId = "benchmark-" + run + "-" + std::to_string(i);
                    bench.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
                    auto result = captureToFiles(backend, settings, bench, true);
                    for (const auto& output : result.batch.outputs) check(!output.error && output.status == "SAVED", "benchmark output");
                    for (const auto& capture : result.batch.captures) check(!capture.error, "benchmark capture");
                    elapsed.push_back(result.elapsedMs);
                    if (!result.batch.displayFrames.empty()) {
                        auto first = result.batch.displayFrames.front().acquisitionStartNs, last = first;
                        for (const auto& frame : result.batch.displayFrames) { first = std::min(first, frame.acquisitionStartNs); last = std::max(last, frame.acquisitionStartNs); }
                        skews.push_back(double(last - first) / 1000000.0);
                    }
                }
                std::sort(elapsed.begin(), elapsed.end()); std::sort(skews.begin(), skews.end());
                std::cout << "BASELINE steady_clock n=100 regions=5 size=16x16 physical_displays=" << profile.calibratedDisplays.size()
                          << " format=PNG capture_save_p50_ms=" << elapsed[49] << " p95_ms=" << elapsed[94]
                          << " display_start_skew_p50_ms=" << skews[49] << " p95_ms=" << skews[94] << '\n';
            }
        }
        for (auto window : windows) DestroyWindow(window);
    } catch (const std::exception& e) {
        for (auto window : windows) DestroyWindow(window);
        std::cerr << "FAIL " << e.what() << '\n'; return 1;
    }
}
