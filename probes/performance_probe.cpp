// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <dwmapi.h>
#include "adapters/win_capture.h"
#include "adapters/capture_files.h"
#include "adapters/report_queue.h"
#include "core/scene_geometry.h"
#include "compat/qt_widgets_compat.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static QJsonObject summary(std::vector<double> values) { if (values.empty()) return {{"n", 0}, {"status", "NOT_MEASURED"}}; std::sort(values.begin(), values.end()); auto p = [&](double q) { return values[std::size_t(std::ceil(q * values.size())) - 1]; }; return {{"n", int(values.size())}, {"p50Ms", p(.50)}, {"p95Ms", p(.95)}, {"maxMs", values.back()}, {"clock", "monotonic"}}; }
static LRESULT CALLBACK paint(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_PAINT) { PAINTSTRUCT ps{}; const auto dc = BeginPaint(window, &ps); RECT bounds{}; GetClientRect(window, &bounds);
        for (int y = 0; y < bounds.bottom; y += 32) for (int x = 0; x < bounds.right; x += 32) { auto brush = CreateSolidBrush(RGB((x / 32 * 31) % 256, (y / 32 * 47) % 256, ((x + y) / 32 * 17) % 256)); RECT tile{x, y, std::min<LONG>(x + 32, bounds.right), std::min<LONG>(y + 32, bounds.bottom)}; FillRect(dc, &tile, brush); DeleteObject(brush); }
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255)); TextOutW(dc, 16, 16, L"PUBLIC SYNTHETIC AITSNAP BENCH", 29); EndPaint(window, &ps); return 0;
    } return DefWindowProcW(window, message, w, l);
}
int main(int argc, char** argv)
{
    compat::configureHighDpi(); QApplication app(argc, argv); app.setApplicationName("AITSnapBenchmark"); std::vector<HWND> windows;
    try {
        check(app.arguments().size() == 2, "new benchmark root required"); if (const auto status = interactiveDesktopStatus(); status) { std::cout << "NATIVE_HOLD " << status.code << '\n'; return 2; }
        const auto root = QDir(app.arguments()[1]).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); check(QDir().mkpath(root), "benchmark output"); QString error;
        WinCaptureBackend backend; const auto displays = backend.displays(); check(!displays.empty(), "benchmark displays");
        WNDCLASSW wc{}; wc.lpfnWndProc = paint; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AITSnapPerformancePatterns"; check(RegisterClassW(&wc) != 0, "benchmark pattern class");
        std::vector<core::Rect> patterns;
        for (const auto& d : displays) { const int width = std::min(1280, d.rect.width), height = std::min(720, d.rect.height); const core::Rect rectangle{d.rect.x + (d.rect.width - width) / 2, d.rect.y + (d.rect.height - height) / 2, width, height}; patterns.push_back(rectangle);
            const auto window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"Public synthetic performance", WS_POPUP, rectangle.x, rectangle.y, width, height, nullptr, nullptr, wc.hInstance, nullptr); check(window != nullptr, "benchmark window"); windows.push_back(window); ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window);
        } DwmFlush(); QThread::msleep(150);
        core::CaptureProfile single; single.id = "bench-single"; single.name = "Synthetic single"; single.calibratedDisplays = displays; single.regions = {{"single", "Synthetic 1280x720", patterns.front(), true}};
        core::CaptureProfile multi; multi.id = "bench-multi"; multi.name = "Synthetic five sources"; multi.calibratedDisplays = displays;
        QJsonArray sizes;
        for (int i = 0; i < 5; ++i) { auto rectangle = patterns[std::size_t(i) % patterns.size()]; rectangle.width = std::min(640, rectangle.width); rectangle.height = std::min(400, rectangle.height); multi.regions.push_back({"r" + std::to_string(i), "Synthetic region " + std::to_string(i), rectangle, true}); sizes.append(QJsonArray{rectangle.width, rectangle.height}); }
        Settings settings = defaultSettings(); settings.outputDirectory = root + "/captures"; settings.profiles = {single};
        std::vector<double> singleTimes, captureTimes, renderTimes, totalTimes, skews; std::vector<CaptureOutcome> reports;
        int files = 0;
        auto run = [&](const core::CaptureProfile& profile, int index, bool scene) {
            core::CaptureRequest request; request.requestId = std::string(scene ? "multi-" : "single-") + std::to_string(index) + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); request.profile = profile; request.context.testId = "PUBLIC-SYNTHETIC-PERF"; settings.sequence = index + 1;
            auto outcome = captureToFiles(backend, settings, request, true); for (const auto& c : outcome.batch.captures) check(!c.error && c.image.valid(), "benchmark source acquisition"); for (const auto& o : outcome.batch.outputs) { check(!o.error && o.status == "SAVED", "benchmark image output"); ++files; } check(writeBatchManifest(outcome, error), "benchmark manifest"); return outcome;
        };
        for (int i = 0; i < 20; ++i) singleTimes.push_back(run(single, i, false).elapsedMs);
        settings.profiles = {multi}; settings.outputMode = "both"; settings.scenes = {core::presetScene(multi, "bench-scene", "Synthetic 1920x1080", 1920, 1080, core::LayoutPreset::Grid)}; settings.selectedSceneIds = QStringList{"bench-scene"};
        const auto gdiBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (int i = 0; i < 100; ++i) {
            auto outcome = run(multi, i, true); captureTimes.push_back(outcome.captureSaveMs); renderTimes.push_back(outcome.renderMs); totalTimes.push_back(outcome.elapsedMs);
            if (!outcome.batch.displayFrames.empty()) { auto first = outcome.batch.displayFrames.front().acquisitionStartNs, last = first; for (const auto& frame : outcome.batch.displayFrames) { first = std::min(first, frame.acquisitionStartNs); last = std::max(last, frame.acquisitionStartNs); } skews.push_back(double(last - first) / 1000000.0); }
            if (i % 10 == 0) { for (auto& capture : outcome.batch.captures) capture.image = {}; outcome.composites.clear(); reports.push_back(std::move(outcome)); }
        }
        const auto gdiAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        PROCESS_MEMORY_COUNTERS memory{}; memory.cb = sizeof(memory); check(GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)), "benchmark process memory");
        QJsonObject receipt{{"status", "CAPTURE_SCENE_PASS"}, {"qt", qVersion()}, {"nativeDisplays", int(displays.size())}, {"sourceSizesPx", sizes}, {"singleSizePx", QJsonArray{patterns.front().width, patterns.front().height}}, {"sceneSizePx", QJsonArray{1920, 1080}}, {"singleCaptureSave", summary(singleTimes)}, {"multiCaptureSave", summary(captureTimes)}, {"sceneRender", summary(renderTimes)}, {"captureAndScene", summary(totalTimes)}, {"displayStartSkew", summary(skews)}, {"captureProcessPeakWorkingSetBytes", qint64(memory.PeakWorkingSetSize)}, {"gdiBefore", int(gdiBefore)}, {"gdiAfter", int(gdiAfter)}, {"imageFiles", files}, {"source", "owned synthetic pattern windows; no real AIT data"}};
        check(saveBytesNew(QJsonDocument(receipt).toJson(), root + "/capture-scene-metrics.json", error), "capture metrics saved before independent Word phase");
        std::cout << "CAPTURE_SCENE_BENCH_PASS " << QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        ReportQueue queue(root + "/report-jobs", QCoreApplication::applicationDirPath() + "/aitsnap-word-bridge.exe"); ReportOptions options; options.documentPath = root + "/synthetic performance report.docx";
        for (const auto& report : reports) check(queue.enqueue(report, options, error), "benchmark Word job enqueue");
        const auto until = QDateTime::currentMSecsSinceEpoch() + 300000; bool committed = false;
        while (QDateTime::currentMSecsSinceEpoch() < until) { QCoreApplication::processEvents(); int count = 0; for (const auto& job : queue.jobs()) { check(job["state"] != "FAILED" && job["state"] != "UNCERTAIN", "Word benchmark requires receipt review"); if (job["state"] == "COMMITTED") ++count; } if (count == int(reports.size())) { committed = true; break; } QThread::msleep(20); }
        check(committed, "Word benchmark bounded deadline"); std::vector<double> workerTimes, executionTimes; qint64 helperPeak = 0, wordPeak = 0; int wordMemorySamples = 0;
        for (const auto& job : queue.jobs()) { const auto ext = job["extensions"].toObject(), result = ext["lastResult"].toObject(); workerTimes.push_back(ext["workerMonotonicMs"].toDouble()); executionTimes.push_back(result["wordExecutionMonotonicMs"].toDouble()); helperPeak = std::max(helperPeak, qint64(result["helperPeakWorkingSetBytes"].toDouble())); if (result.contains("wordProcessPeakWorkingSetBytes")) { wordPeak = std::max(wordPeak, qint64(result["wordProcessPeakWorkingSetBytes"].toDouble())); ++wordMemorySamples; } }
        receipt["status"] = "PASS"; receipt["wordWorker"] = summary(workerTimes); receipt["wordExecution"] = summary(executionTimes); receipt["helperPeakWorkingSetBytes"] = helperPeak; if (wordMemorySamples) receipt["wordProcessPeakWorkingSetBytes"] = wordPeak; receipt["wordMemoryStatus"] = wordMemorySamples ? "MEASURED_OWN_APPLICATION" : "NOT_MEASURED"; receipt["wordMemorySamples"] = wordMemorySamples; receipt["document"] = options.documentPath; receipt["wordItemsPerJob"] = 6;
        check(saveBytesNew(QJsonDocument(receipt).toJson(), root + "/benchmark.json", error), "combined benchmark receipt");
        for (const auto window : windows) DestroyWindow(window); std::cout << "BENCHMARK_PASS " << QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString() << " evidence=" << root.toStdString() << '\n';
    } catch (const std::exception& error) { for (const auto window : windows) DestroyWindow(window); std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
