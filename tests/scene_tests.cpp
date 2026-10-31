// SPDX-License-Identifier: Apache-2.0
#include "adapters/capture_files.h"
#include "core/scene_geometry.h"
#include "compat/qt_widgets_compat.h"
#include "desktop/scene_editor.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QFileInfo>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
using namespace aitsnap::core;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct FakeCapture final : ICaptureBackend {
    std::vector<Display> displays() override { return {{"left", {-100, 0, 100, 100}, 0}, {"right", {0, 0, 100, 100}, 0}}; }
    DisplayFrame acquire(const Display& d) override {
        auto bytes = std::make_shared<std::vector<std::uint8_t>>(40000, 0);
        for (std::size_t i = 0; i < bytes->size(); i += 4) { (*bytes)[i + (d.id == "left" ? 2 : 1)] = 255; (*bytes)[i + 3] = 255; }
        return {d, {100, 100, bytes}, "2026-10-31T08:30:00.000Z", 100, 200, {}};
    }
};
static QByteArray hash(const QString& path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "hash read"); return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex(); }
int main(int argc, char** argv)
{
    compat::configureHighDpi(); QApplication app(argc, argv); app.setApplicationName("AITSnapSceneTests");
    try {
        QTemporaryDir temporary; check(temporary.isValid(), "temporary output");
        QString directory = temporary.path(); const auto flag = app.arguments().indexOf("--evidence");
        if (flag >= 0 && flag + 1 < app.arguments().size()) { directory = app.arguments()[flag + 1]; check(QDir().mkpath(directory), "evidence directory"); }
        QImage red(16, 16, QImage::Format_ARGB32), green = red; red.fill(Qt::red); green.fill(Qt::green);
        SceneSources sources{{"r0", {red, "artifact-red"}}, {"r1", {green, "artifact-green"}}};
        SceneTemplate row; row.id = "row"; row.name = "Synthetic row"; row.width = 128; row.height = 64;
        row.layers = {{"a", "r0"}, {"b", "r1"}}; row.layers[0].destination = {0, 0, 64, 64, 0}; row.layers[1].destination = {64, 0, 64, 64, 0}; row.layers[1].zIndex = 1;
        auto rendered = renderScene(row, sources);
        check(!rendered.error && rendered.complete && rendered.image.pixelColor(8, 8) == QColor(Qt::red) && rendered.image.pixelColor(80, 8) == QColor(Qt::green), "row golden pixels");
        SceneTemplate column = row; column.id = "column"; column.name = "Synthetic column"; column.width = 64; column.height = 128; column.layers[1].destination = {0, 64, 64, 64, 0};
        auto vertical = renderScene(column, sources); check(!vertical.error && vertical.image.pixelColor(8, 80) == QColor(Qt::green), "column golden pixels");
        QImage pattern(32, 16, QImage::Format_ARGB32); pattern.fill(Qt::blue); { QPainter p(&pattern); p.fillRect(0, 0, 16, 16, Qt::red); }
        sources["r0"].image = pattern;
        SceneTemplate crop = row; crop.id = "crop"; crop.name = "Crop"; crop.width = crop.height = 32; crop.layers.resize(1); crop.layers[0].destination = {0, 0, 32, 32, 0}; crop.layers[0].crop = {16, 0, 16, 16};
        check(renderScene(crop, sources).image.pixelColor(8, 8) == QColor(Qt::blue), "non-destructive crop");
        crop.layers[0].crop = {}; auto aspect = renderScene(crop, sources);
        check(aspect.image.pixelColor(1, 1) == sceneColour(crop.background) && aspect.image.pixelColor(1, 16) == QColor(Qt::red), "aspect fit centred");
        crop.layers[0].preserveAspect = false; check(renderScene(crop, sources).image.pixelColor(1, 1) == QColor(Qt::red), "explicit stretch");
        crop.layers[0].sourceArtifactId = "wrong"; check(renderScene(crop, sources).error.code == "MISSING_SOURCE", "pinned artifact enforced"); crop.layers[0].sourceArtifactId.clear();
        crop.layers[0].crop = {20, 0, 20, 16}; check(renderScene(crop, sources).error.code == "SOURCE_CROP", "out-of-source crop rejected");
        auto missing = row; missing.layers[0].sourceRegionId = "absent";
        check(renderScene(missing, sources).error.code == "MISSING_SOURCE", "missing source fails");
        missing.layers[0].missingSourcePolicy = "placeholder"; auto incomplete = renderScene(missing, sources); check(!incomplete.error && !incomplete.complete && !incomplete.warnings.empty(), "placeholder labelled incomplete");
        auto alpha = row; alpha.layers.clear(); alpha.background = "#12345678"; check(renderScene(alpha, sources).image.pixelColor(0, 0).alpha() == 0x78, "trailing alpha semantics");
        check(sources["r0"].image == pattern, "renderer never mutates source image");
        auto rotated = crop; rotated.layers[0].crop = {}; rotated.layers[0].destination = {0, 8, 32, 16, 90}; rotated.layers[0].preserveAspect = false;
        const auto rotation = renderScene(rotated, sources); check(!rotation.error && rotation.image.pixelColor(16, 4) == QColor(Qt::red) && rotation.image.pixelColor(16, 28) == QColor(Qt::blue), "rotation about destination centre");
        auto overlap = row; overlap.layers[1].destination = overlap.layers[0].destination;
        check(renderScene(overlap, sources).image.pixelColor(8, 32) == QColor(Qt::green), "z order"); overlap.layers[1].visible = false;
        check(renderScene(overlap, sources).image.pixelColor(8, 32) == QColor(Qt::red), "hidden layer omitted");
        row.revisionCreatedAtUtc = "2026-10-31T08:30:00.000Z"; row.extensionsJson = "{\"publicExample\":true}";
        QString error; SceneTemplate roundtrip; check(sceneFromJson(sceneJson(row), roundtrip, error) && renderScene(roundtrip, sources).image == renderScene(row, sources).image && roundtrip.revisionCreatedAtUtc == row.revisionCreatedAtUtc && roundtrip.extensionsJson == row.extensionsJson, "scene roundtrip including metadata");
        auto unknown = sceneJson(row); unknown["futureField"] = true; check(!sceneFromJson(unknown, roundtrip, error), "unknown field rejected"); error.clear();
        CaptureProfile profile; profile.id = "synthetic-profile"; profile.name = "Synthetic sources"; profile.regions = {{"r0", "Red", {-90, 10, 16, 16}, true}, {"r1", "Green", {10, 10, 16, 16}, true}};
        FakeCapture backend; profile.calibratedDisplays = backend.displays();
        for (auto preset : {LayoutPreset::Row, LayoutPreset::Column, LayoutPreset::Grid, LayoutPreset::Hero}) check(!validateScene(presetScene(profile, "preset", "Preset", 1280, 720, preset)), "preset geometry");
        Settings settings = defaultSettings(); settings.profiles = {profile}; settings.scenes = {row, column}; settings.selectedSceneIds = QStringList{"row", "column"}; settings.outputMode = "both"; settings.format = "JPEG"; settings.outputDirectory = directory;
        CaptureRequest request; request.requestId = "synthetic-" + QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString(); request.issuedAtUtc = "2026-10-31T08:30:00.000Z"; request.profile = profile;
        auto outcome = captureToFiles(backend, settings, request, true);
        check(outcome.composites.size() == 2 && outcome.sceneSnapshots.size() == 2, "multiple composites one batch");
        int originals = 0, exports = 0, composites = 0;
        for (const auto& output : outcome.batch.outputs) {
            if (output.error) std::cerr << output.error.code << ' ' << output.error.message << '\n';
            check(!output.error && output.status == "SAVED", "batch scene output");
            if (output.kind == "ORIGINAL") { ++originals; check(QString::fromStdString(output.path).endsWith(".png"), "lossless source retained with JPEG export"); }
            if (output.kind == "IMAGE_EXPORT") ++exports;
            if (output.kind == "COMPOSITE") ++composites;
        }
        check(originals == 2 && exports == 2 && composites == 2, "original/export/composite independence");
        check(writeBatchManifest(outcome, error), "batch manifest");
        SceneSources historical; CaptureProfile historicalProfile;
        check(loadHistoricalSources(QDir(outcome.directory).filePath("batch.json"), historical, historicalProfile, error) && historical.size() == 2, "historical original hashes and sources");
        check(saveImageNew(renderScene(column, historical).image, QDir(outcome.directory).filePath("rerender-new.png"), "PNG", error), "historical unique derivation");
        SceneEditor editor(row, profile, historical);
        auto* layerList = editor.findChild<QListWidget*>("sceneLayers"); auto* view = editor.findChild<QGraphicsView*>("sceneCanvas");
        check(layerList && view, "scene editor widgets"); layerList->item(0)->setSelected(true); layerList->item(1)->setSelected(true);
        editor.findChild<QPushButton*>("groupLayers")->click(); auto grouped = editor.sceneTemplate();
        check(!grouped.layers[0].groupId.empty() && grouped.layers[0].groupId == grouped.layers[1].groupId, "group assignment");
        layerList->clearSelection(); layerList->item(0)->setSelected(true);
        QGraphicsItem* first = nullptr; for (auto* item : view->scene()->items()) if (item->data(0).toString() == "a") first = item;
        check(first != nullptr, "source layer item"); first->setPos(13, 27); auto moved = editor.sceneTemplate();
        check(moved.layers[0].destination.x == 13 && moved.layers[1].destination.x == 77 && moved.layers[1].destination.y == 27, "group move preserves partner offsets");
        editor.findChild<QDoubleSpinBox*>("layerX")->setValue(42); moved = editor.sceneTemplate();
        check(moved.layers[0].destination.x == 42 && moved.layers[1].destination.x == 106, "numeric group move");
        editor.findChild<QCheckBox*>("layerLocked")->setChecked(true); layerList->clearSelection(); layerList->item(1)->setSelected(true);
        QGraphicsItem* second = nullptr; for (auto* item : view->scene()->items()) if (item->data(0).toString() == "b") second = item;
        check(second != nullptr, "partner item"); second->setPos(second->pos() + QPointF(10, 10));
        check(editor.sceneTemplate().layers[1].destination.x == 106, "locked partner prevents group drift");
        layerList->clearSelection(); layerList->item(0)->setSelected(true); editor.findChild<QCheckBox*>("layerLocked")->setChecked(false);
        editor.findChild<QPushButton*>("duplicateLayer")->click(); const auto duplicated = editor.sceneTemplate();
        check(duplicated.layers.size() == 3 && duplicated.layers.back().sourceRegionId == "r0" && duplicated.layers.back().groupId.empty(), "source reuse without shared group mutation");
        editor.findChild<QPushButton*>("undoScene")->click(); check(editor.sceneTemplate().layers.size() == 2, "undo duplicate");
        check(editor.exportDerived(QDir(outcome.directory).filePath("widget-derived.png"), error) && QFile::exists(QDir(outcome.directory).filePath("widget-derived.png.derivation.json")), "editor export with derivation provenance");
        for (const auto& output : outcome.batch.outputs) if (output.kind == "ORIGINAL") check(hash(QString::fromStdString(output.path)).toStdString() == output.sha256, "original SHA unchanged after two scenes and rerender");
        const auto settingsFile = QDir(directory).filePath("scene-settings.json"); check(saveSettings(settingsFile, settings, error), "settings v2 save"); Settings loaded;
        check(loadSettings(settingsFile, loaded, error) && loaded.scenes.size() == 2 && loaded.outputMode == "both", "settings v2 scene roundtrip");
        QFile savedSettings(settingsFile); check(savedSettings.open(QIODevice::ReadOnly), "settings read"); auto legacy = QJsonDocument::fromJson(savedSettings.readAll()).object(); savedSettings.close();
        legacy["schemaVersion"] = 1; legacy.remove("scenes"); legacy.remove("selectedSceneIds"); legacy.remove("outputMode");
        const auto legacyPath = QDir(directory).filePath("legacy-settings-" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json"); const auto legacyBytes = QJsonDocument(legacy).toJson();
        check(saveBytesNew(legacyBytes, legacyPath, error), "legacy fixture"); Settings migrated;
        check(loadSettings(legacyPath, migrated, error) && saveSettings(legacyPath, migrated, error), "v1 to v2 migration");
        const auto backups = QDir(directory).entryList({QFileInfo(legacyPath).fileName() + ".v1-preserved-*"}, QDir::Files);
        check(backups.size() == 1, "v1 backup retained"); QFile backup(QDir(directory).filePath(backups[0])); check(backup.open(QIODevice::ReadOnly) && backup.readAll() == legacyBytes, "v1 original bytes preserved");
        check(saveBytesNew(QJsonDocument(sceneJson(row)).toJson(), QDir(outcome.directory).filePath("scene-template.json"), error), "scene fixture export");
        std::cout << "SCENE_PASS golden row/column/crop/aspect/alpha missing/pinned/version sources=immutable scenes=2 historical=PASS manifest=" << outcome.directory.toStdString() << "/batch.json\n";
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
