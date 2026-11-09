// SPDX-License-Identifier: Apache-2.0
#include "adapters/capture_files.h"
#include "core/scene_geometry.h"
#include "compat/qt_widgets_compat.h"
#include "desktop/appearance.h"
#include "desktop/scene_editor.h"
#include <QApplication>
#include <QAction>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <array>
#include <cmath>
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

static void distributionButtons(const SceneTemplate& scene, const CaptureProfile& profile)
{
    auto fixture = scene;
    fixture.layers.push_back(fixture.layers[0]);
    fixture.layers.back().id = "c";
    fixture.layers.back().zIndex = 2;
    const std::array<Destination, 3> positions{{{0, 0, 20, 30, 0}, {25, 40, 30, 20, 0}, {100, 120, 10, 25, 0}}};
    for (std::size_t i = 0; i < positions.size(); ++i) {
        fixture.layers[i].destination = positions[i];
        fixture.layers[i].preserveAspect = false;
    }
    bool bothAxesPass = true;
    for (const bool horizontal : {true, false}) {
        const auto label = horizontal ? uiText("水平分布") : uiText("垂直分布");
        const auto distributionButton = [&](const SceneEditor& editor) {
            for (auto* candidate : editor.findChildren<QPushButton*>())
                if (candidate->text() == label) return candidate;
            throw std::runtime_error("distribution button");
        };
        auto expected = fixture;
        if (horizontal) expected.layers[1].destination.x = 45;
        else expected.layers[1].destination.y = 65;
        {
            SceneEditor editor(fixture, profile, {});
            editor.findChild<QListWidget*>("sceneLayers")->selectAll();
            distributionButton(editor)->click();
            check(sceneJson(editor.sceneTemplate()) == sceneJson(expected), "three unlocked layers distribute");
        }
        auto anchored = fixture;
        anchored.layers[0].locked = true;
        anchored.layers[2].locked = true;
        expected.layers[0].locked = true;
        expected.layers[2].locked = true;
        SceneEditor editor(anchored, profile, {});
        auto* list = editor.findChild<QListWidget*>("sceneLayers");
        auto* status = editor.findChild<QLabel*>("sceneEditorStatus");
        auto* button = distributionButton(editor);
        list->item(0)->setSelected(true);
        list->item(1)->setSelected(true);
        const auto previousMessage = status->text();
        button->click();
        const bool gaveGuidance = status->text() != previousMessage &&
            sceneJson(editor.sceneTemplate()) == sceneJson(anchored);
        list->selectAll();
        button->click();
        const bool moved = sceneJson(editor.sceneTemplate()) == sceneJson(expected);
        if (moved) {
            button->click();
            editor.findChild<QPushButton*>("undoScene")->click();
            check(sceneJson(editor.sceneTemplate()) == sceneJson(anchored), "one undo restores distribution");
        }
        std::cout << (horizontal ? "HORIZONTAL" : "VERTICAL") << " distribution: three-unlocked=1 two-object guidance="
                  << gaveGuidance << " fixed-endpoint movement=" << moved << '\n';
        bothAxesPass = bothAxesPass && gaveGuidance && moved;
    }
    check(bothAxesPass, "distribution must guide two-object selections and respect fixed endpoints");

    auto segmented = fixture;
    segmented.layers[2].locked = true;
    segmented.layers.push_back(fixture.layers[0]);
    segmented.layers.back().id = "d";
    segmented.layers.back().destination.x = 160;
    segmented.layers.push_back(fixture.layers[0]);
    segmented.layers.back().id = "e";
    segmented.layers.back().destination.x = 200;
    distributeLayers(segmented, {0, 1, 2, 3, 4}, true);
    check(segmented.layers[1].destination.x == 45 && segmented.layers[2].destination.x == 100 &&
          segmented.layers[3].destination.x == 145, "an interior locked object splits the equal-gap intervals");
}

static void backgroundPresets(SceneTemplate scene, const CaptureProfile& profile, const QString& directory)
{
    scene.layers.clear();
    SceneEditor editor(scene, profile, {});
    auto* field = editor.findChild<QLineEdit*>("sceneBackground");
    auto* canvas = editor.findChild<QGraphicsView*>("sceneCanvas")->scene();
    const auto verify = [&](const char* colour, const QColor& pixel, const QString& name) {
        check(field->text() == QString::fromLatin1(colour) && editor.sceneTemplate().background == colour &&
              canvas->backgroundBrush().color() == pixel, "background field, saved model and canvas agree");
        const auto path = QDir(directory).filePath(name + ".png");
        QString error;
        check(editor.exportDerived(path, error) && QImage(path).pixelColor(0, 0) == pixel, "exported PNG background pixel");
    };
    editor.findChild<QPushButton*>("sceneBackgroundBlack")->click();
    verify("#000000", QColor(Qt::black), "black");
    editor.findChild<QPushButton*>("sceneBackgroundWhite")->click();
    verify("#FFFFFF", QColor(Qt::white), "white");
    editor.findChild<QPushButton*>("sceneBackgroundTransparent")->click();
    verify("#00000000", QColor(0, 0, 0, 0), "transparent");
    editor.findChild<QPushButton*>("undoScene")->click();
    check(field->text() == "#FFFFFF" && canvas->backgroundBrush().color() == QColor(Qt::white),
          "undo restores the previous background in the preview and field");
    field->setText("#12345680");
    field->editingFinished();
    verify("#12345680", QColor(0x12, 0x34, 0x56, 0x80), "custom-rgba");
}

static void nativeCanvasAndView()
{
    SceneTemplate seed; seed.id = "native"; seed.name = "Native layout";
    CaptureProfile profile; profile.id = "native-profile"; profile.name = "Native sources";
    profile.regions = {{"wide", "Wide", {0, 0, 400, 200}}, {"portrait", "Portrait", {0, 0, 1080, 1920}}};
    QImage wide(3840, 2160, QImage::Format_RGB32); wide.fill(Qt::red);
    SceneEditor native(seed, profile, {{"wide", {wide, "verified-full-frame"}}}, nullptr, true);
    const auto created = native.sceneTemplate();
    check(created.width == 4964 && created.height == 2192 && created.layers[0].destination.width == 3840 &&
          created.layers[1].destination.height == 1920, "new scene fits full source pixels and region fallback");
    native.findChild<QSpinBox*>("sceneCanvasWidth")->setValue(5000);
    native.findChild<QSpinBox*>("sceneCanvasHeight")->setValue(4000);
    native.findChild<QPushButton*>("applySceneCanvas")->click();
    native.findChild<QComboBox*>("sceneLayoutPreset")->setCurrentIndex(int(LayoutPreset::Column));
    native.findChild<QPushButton*>("applyPreset")->click();
    const auto fixed = native.sceneTemplate();
    check(fixed.width == 5000 && fixed.height == 4000 && fixed.layers[0].destination.width == 3840 &&
          fixed.layers[1].destination.y > 2160 && fixed.layers[1].id == created.layers[1].id,
          "preset arranges existing native layers without changing the fixed canvas");
    native.findChild<QPushButton*>("fitSceneContent")->click();
    check(native.sceneTemplate().width == 3872 && native.sceneTemplate().height == 4124, "fit derives the export canvas from arranged layer bounds");
    native.findChild<QPushButton*>("undoScene")->click();
    check(native.sceneTemplate().width == 5000 && native.findChild<QSpinBox*>("sceneCanvasWidth")->value() == 5000,
          "undo restores both the canvas and the displayed dimensions");

    auto cropped = seed;
    cropped.layers = {{"crop", "wide"}, {"hidden", "portrait"}};
    cropped.layers[0].crop = {10, 20, 120, 200};
    cropped.layers[0].destination = {-200, -100, 120, 200, 90};
    cropped.layers[0].sourceArtifactId = "verified-full-frame";
    cropped.layers[1].visible = false;
    cropped.layers[1].destination = {90000, -90000, 1080, 1920, 0};
    check(!fitCanvasToContent(cropped) && cropped.width == 232 && cropped.height == 152 &&
          cropped.layers[0].crop.width == 120 && cropped.layers[0].sourceArtifactId == "verified-full-frame",
          "fit encloses a rotated cropped layer at a negative origin and ignores hidden layers");
    const auto render = renderScene(cropped, {{"wide", {wide, "verified-full-frame"}}});
    check(render.complete && render.image.size() == QSize(232, 152) && render.image.pixelColor(16, 16) == QColor(Qt::red) &&
          render.image.pixelColor(15, 16) == sceneColour(cropped.background), "rotated native crop uses exact fitted output pixels");

    auto locked = created;
    locked.layers[0].groupId = locked.layers[1].groupId = "pair";
    locked.layers[0].locked = true;
    const auto lockedBefore = sceneJson(locked);
    arrangeLayers(locked, LayoutPreset::Hero);
    check(sceneJson(locked) == lockedBefore, "layout respects a locked group and preserves its metadata");

    auto tooLarge = profile;
    for (auto& region : tooLarge.regions) region.rect = {0, 0, 10000, 10000};
    SceneEditor oversized(seed, tooLarge, {}, nullptr, true);
    oversized.findChild<QPushButton*>("saveScene")->click();
    check(oversized.result() != QDialog::Accepted &&
          oversized.findChild<QLabel*>("sceneEditorStatus")->text().contains("20044"),
          "oversized automatic canvas requires an explicit decision and reports the necessary dimensions");

    auto empty = seed;
    empty.width = 600; empty.height = 400; empty.background = "#e700c0";
    SceneEditor viewer(empty, profile, {});
    viewer.show();
    QApplication::processEvents();
    auto* view = viewer.findChild<QGraphicsView*>("sceneCanvas");
    viewer.findChild<QPushButton*>("fitSceneView")->click();
    const auto pngBefore = renderScene(viewer.sceneTemplate(), {}).image;
    const auto metadata = sceneJson(viewer.sceneTemplate());
    const auto screenshot = view->viewport()->grab().toImage();
    const auto inside = view->mapFromScene(QPointF(12, 200));
    const auto outside = view->mapFromScene(QPointF(-12, 200));
    const auto dpr = screenshot.devicePixelRatio();
    check(screenshot.pixelColor(inside * dpr) == QColor("#e700c0") &&
          screenshot.pixelColor(outside * dpr) != QColor("#e700c0"), "canvas has visible contrast against the workspace");
    const QPoint pointer = view->viewport()->rect().center() + QPoint(35, 25);
    const QPointF anchored = view->mapToScene(pointer);
    const auto scale = view->transform().m11();
    QWheelEvent wheel(QPointF(pointer), QPointF(view->viewport()->mapToGlobal(pointer)), {},
                      QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(view->viewport(), &wheel);
    const auto after = view->mapToScene(pointer);
    check(view->transform().m11() > scale && std::abs(after.x() - anchored.x()) * scale < 3 &&
          std::abs(after.y() - anchored.y()) * scale < 3, "wheel zoom anchors the point under the pointer");
    viewer.findChild<QPushButton*>("sceneViewActualSize")->click();
    check(view->transform().m11() == 1 && viewer.findChild<QLabel*>("sceneZoomLevel")->text() == "100%",
          "actual-size view and scale indicator");
    check(sceneJson(viewer.sceneTemplate()) == metadata && renderScene(viewer.sceneTemplate(), {}).image == pngBefore,
          "canvas editor chrome and viewport transforms do not affect exported pixels");
}

int main(int argc, char** argv)
{
    compat::configureHighDpi(); QApplication app(argc, argv); app.setApplicationName("AITSnapSceneTests");
    try {
        nativeCanvasAndView();
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
        auto opaqueMissing = missing; opaqueMissing.layers[0].sourceRegionId = "b7d4aead-9d51-4811-bf03-65e5c882b221";
        const auto opaqueResult = renderScene(opaqueMissing, sources);
        check(opaqueResult.image == incomplete.image && opaqueResult.warnings == incomplete.warnings,
              "missing source presentation independent of opaque identity");
        opaqueMissing.layers[0].missingSourcePolicy = "fail";
        check(renderScene(opaqueMissing, sources).error.message.find(opaqueMissing.layers[0].sourceRegionId) == std::string::npos,
              "missing source diagnostic does not expose opaque identity");
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
        // Missing-image layout must represent the selected region, not a fixed thumbnail.
        for (bool portrait : {false, true}) {
            auto previewProfile = profile; previewProfile.regions.resize(1);
            previewProfile.regions[0].rect = {0, 0, portrait ? 200 : 1200, portrait ? 1200 : 200};
            auto previewScene = row; previewScene.width = previewScene.height = 600; previewScene.layers.resize(1);
            previewScene.layers[0].destination = {0, 0, 600, 600, 0};
            SceneEditor emptyPreview(previewScene, previewProfile, {});
            auto* previewView = emptyPreview.findChild<QGraphicsView*>("sceneCanvas");
            QImage pixels(600, 600, QImage::Format_RGB32); pixels.fill(Qt::black);
            { QPainter painter(&pixels); previewView->scene()->render(&painter, QRectF(0,0,600,600), QRectF(0,0,600,600)); }
            check(pixels.pixelColor(portrait ? 50 : 300, portrait ? 300 : 120) == sceneColour(previewScene.background), "missing-source preview preserves selected region aspect");
            check(pixels.pixelColor(300,300) != sceneColour(previewScene.background), "missing-source placeholder remains visible");
        }
        FakeCapture backend; profile.calibratedDisplays = backend.displays();
        for (auto preset : {LayoutPreset::Row, LayoutPreset::Column, LayoutPreset::Grid, LayoutPreset::Hero}) check(!validateScene(presetScene(profile, "preset", "Preset", 1280, 720, preset)), "preset geometry");
        Settings settings = defaultSettings(); settings.profiles = {profile}; settings.scenes = {row, column}; settings.selectedSceneIds = QStringList{"row", "column"}; settings.outputMode = "both"; settings.format = "JPEG"; settings.outputDirectory = directory;
        CaptureRequest request; request.requestId = "synthetic-" + QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString(); request.issuedAtUtc = "2026-10-31T08:30:00.000Z"; request.profile = profile;
        auto outcome = captureToFiles(backend, settings, request, true);
        check(!outcome.directory.endsWith(QString::fromStdString(request.requestId)), "visible batch directory does not expose internal request id");
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
        {
            auto collisionSettings = settings; collisionSettings.filenameTemplate = "same-name"; collisionSettings.outputMode = "originals"; collisionSettings.format = "PNG";
            const auto collided = captureToFiles(backend, collisionSettings, request, true);
            check(collided.directory != outcome.directory, "same timestamp batch uses exclusive readable suffix");
            check(collided.batch.outputs.size() == 2 && collided.batch.outputs[0].status == "SAVED" && collided.batch.outputs[1].status == "SAVED", "colliding names both saved");
            check(collided.batch.outputs[0].path != collided.batch.outputs[1].path && QFileInfo(QString::fromStdString(collided.batch.outputs[1].path)).fileName() == "same-name_2.png", "readable collision suffix instead of UUID");
            check(QImage(QString::fromStdString(collided.batch.outputs[0].path)).pixelColor(1, 1) != QImage(QString::fromStdString(collided.batch.outputs[1].path)).pixelColor(1, 1), "colliding source pixels never overwritten");
        }
        check(writeBatchManifest(outcome, error), "batch manifest");
        SceneSources historical; CaptureProfile historicalProfile;
        check(loadHistoricalSources(QDir(outcome.directory).filePath("batch.json"), historical, historicalProfile, error) && historical.size() == 2, "historical original hashes and sources");
        check(saveImageNew(renderScene(column, historical).image, QDir(outcome.directory).filePath("rerender-new.png"), "PNG", error), "historical unique derivation");
        distributionButtons(row, profile);
        backgroundPresets(row, profile, directory);
        SceneEditor editor(row, profile, historical);
        auto* layerList = editor.findChild<QListWidget*>("sceneLayers"); auto* view = editor.findChild<QGraphicsView*>("sceneCanvas");
        check(layerList && view, "scene editor widgets");
        {
            auto fixture=row;
            fixture.layers[0].destination={11,27,120,60,90};
            fixture.layers[0].crop={0,0,32,16};
            fixture.layers[1].destination={160,28,80,120,0};
            SceneEditor matching(fixture,profile,historical);
            auto* list=matching.findChild<QListWidget*>("sceneLayers");
            list->item(0)->setSelected(true);list->item(1)->setSelected(true);
            const auto bounds=[&](const Layer& layer){
                const auto& image=historical.at(layer.sourceRegionId).image;
                return visibleLayerBounds(layer,image.width(),image.height());
            };
            matching.findChild<QPushButton*>("matchLayerWidth")->click();
            const auto after=matching.sceneTemplate();
            const auto initial=bounds(fixture.layers[0]), scaled=bounds(after.layers[0]), reference=bounds(after.layers[1]);
            check(std::abs(scaled.width-reference.width)<1e-8 && std::abs(scaled.x-initial.x)<1e-8 &&
                  std::abs(scaled.y-initial.y)<1e-8 && after.width==fixture.width && after.height==fixture.height,
                  "match width uses rendered size and keeps the visible anchor and export canvas");
            matching.findChild<QPushButton*>("undoScene")->click();
            check(sceneJson(matching.sceneTemplate())==sceneJson(fixture),"one undo restores the whole match-width operation");
            matching.findChild<QPushButton*>("matchLayerHeight")->click();
            const auto resized=matching.sceneTemplate();
            check(std::abs(bounds(resized.layers[0]).height-bounds(resized.layers[1]).height)<1e-8,"match height uses rendered size");
            matching.findChild<QPushButton*>("applyPreset")->click();
            const auto arranged=matching.sceneTemplate();
            check(arranged.layers[0].destination.width==resized.layers[0].destination.width &&
                  arranged.layers[0].destination.height==resized.layers[0].destination.height && arranged.width==resized.width,
                  "row placement preserves matched dimensions and fixed canvas");
        }
        layerList->item(0)->setSelected(true); layerList->item(1)->setSelected(true);
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
        const auto beforeDelete = renderScene(editor.sceneTemplate(), historical).image;
        const auto layerCount = editor.sceneTemplate().layers.size();layerList->selectAll();editor.findChild<QAction*>("layer.delete")->trigger();
        check(editor.sceneTemplate().layers.empty(), "context action deletes all selected layers");
        editor.findChild<QPushButton*>("undoScene")->click();check(editor.sceneTemplate().layers.size()==layerCount && renderScene(editor.sceneTemplate(),historical).image==beforeDelete,"undo batch deletion restores rendered scene");
        editor.findChild<QSpinBox*>("sceneCanvasWidth")->setValue(143);
        const auto derived=QDir(outcome.directory).filePath("widget-derived.png");
        check(editor.exportDerived(derived,error) && QImage(derived).width()==143,"direct export uses the valid canvas draft");
        QFile derivedReceipt(derived+".derivation.json");
        check(derivedReceipt.open(QIODevice::ReadOnly),"derivation receipt");
        const auto publication=QJsonDocument::fromJson(derivedReceipt.readAll()).object();
        derivedReceipt.close();
        const auto pending=QDir(outcome.directory).filePath(".aitsnap-"+publication["publicationId"].toString()+".pending.png");
        check(QFile::rename(derived,pending),"simulate interruption after receipt publication");
        check(editor.exportDerived(derived,error) && !QFile::exists(pending) &&
              hash(derived)==publication["imageSha256"].toString().toLatin1(),"restart completes the exact staged export");
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
