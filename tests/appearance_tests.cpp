// SPDX-License-Identifier: Apache-2.0
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "desktop/scene_layer_item.h"
#include "main_window.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QDir>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QTableWidget>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
static void settle() { for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); QThread::msleep(10); } }
int main(int argc, char** argv)
{
    QApplication app(argc, argv); app.setApplicationName("AITSnapAppearanceTests");
    if (app.platformName() == "offscreen") { QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf"); app.setFont(QFont("Segoe UI", 10)); }
    try {
        QTemporaryDir temp; check(temp.isValid(), "temporary directory");
        Settings settings = defaultSettings(); settings.outputDirectory = temp.path();
        settings.saveHotkey = "Ctrl+Alt+Shift+F20"; settings.copyHotkey = "Ctrl+Alt+Shift+F21"; settings.bothHotkey = "Ctrl+Alt+Shift+F22";
        core::CaptureProfile profile; profile.id = "unchanged-profile"; profile.name = "Fixture";
        profile.regions = {{"unchanged-id-1", "Voltage", {0, 0, 32, 16}, true}, {"unchanged-id-2", "Log", {32, 0, 32, 16}, true}};
        settings.profiles.push_back(profile); QString error;
        const auto path = temp.filePath("settings.json"); check(saveSettings(path, settings, error), "fixture save");
        // A real legacy v3 input, preserving its exact bytes before the first v4 write.
        QFile f(path); check(f.open(QIODevice::ReadOnly), "legacy read"); auto old = QJsonDocument::fromJson(f.readAll()).object(); f.close();
        old["schemaVersion"] = 3; old.remove("appearance"); auto profiles = old["profiles"].toArray(); auto p = profiles[0].toObject(); p.remove("nextRegionNumber");
        auto regions = p["regions"].toArray(); for (int i = 0; i < regions.size(); ++i) { auto r = regions[i].toObject(); r.remove("displayNumber"); regions[i] = r; } p["regions"] = regions; profiles[0] = p; old["profiles"] = profiles;
        const auto originalBytes = QJsonDocument(old).toJson(); check(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "legacy write"); f.write(originalBytes); f.close();
        Settings loaded; check(loadSettings(path, loaded, error), "legacy load");
        check(loaded.language == "system" && loaded.colorMode == "system", "legacy appearance defaults");
        check(loaded.profiles[0].regions[0].id == "unchanged-id-1" && loaded.profiles[0].regions[1].displayNumber == 2, "stable migration identities");
        loaded.profiles[0].regions.erase(loaded.profiles[0].regions.begin() + 1);
        loaded.profiles[0].regions.push_back({"new-region", "Third", {0, 16, 32, 16}, true}); core::numberRegions(loaded.profiles[0]);
        check(loaded.profiles[0].regions.back().displayNumber == 3, "deleted region number is not reused");
        check(saveSettings(path, loaded, error), "migrate save");
        const auto backups = QDir(temp.path()).entryList({"settings.json.v3-preserved-*"}, QDir::Files); check(backups.size() == 1, "one exact legacy backup");
        QFile backup(temp.filePath(backups.front())); check(backup.open(QIODevice::ReadOnly) && backup.readAll() == originalBytes, "legacy bytes preserved");
        installLanguage("en-US"); applyColorMode("light");
        const auto batchDirectory = temp.filePath("saved-batches/legacy-batch"); check(QDir().mkpath(batchDirectory), "gallery fixture directory");
        QImage galleryImage(40, 20, QImage::Format_ARGB32); galleryImage.fill(Qt::green);
        check(saveImageNew(galleryImage, batchDirectory + "/source.png", "PNG", error), "gallery image");
        QJsonObject manifest{{"batchId", "gallery-fixture"}, {"artifacts", QJsonArray{
            QJsonObject{{"kind", "ORIGINAL"}, {"saveStatus", "SAVED"}, {"relativePath", "source.png"}},
            QJsonObject{{"kind", "ORIGINAL"}, {"saveStatus", "SAVED"}, {"relativePath", "../../settings.json"}}
        }}};
        check(saveBytesNew(QJsonDocument(manifest).toJson(), batchDirectory + "/batch.json", error), "gallery manifest");
        loaded.outputDirectory = temp.filePath("saved-batches"); check(saveSettings(path, loaded, error), "gallery configuration");
        core::SceneTemplate fixtureScene; fixtureScene.id = "two-source-scene"; fixtureScene.name = "Comparison"; fixtureScene.width = 64; fixtureScene.height = 16;
        for (int i = 0; i < 2; ++i) { core::Layer l; l.id = "layer" + std::to_string(i); l.sourceRegionId = loaded.profiles[0].regions[std::size_t(i)].id; l.destination = {double(i * 32), 0, 32, 16, 0}; fixtureScene.layers.push_back(l); }
        loaded.scenes.push_back(fixtureScene); loaded.profiles[0].sceneIds = {fixtureScene.id}; loaded.profiles[0].outputMode = "both"; loaded.selectedSceneIds = QStringList{QString::fromStdString(fixtureScene.id)}; loaded.outputMode = "both";
        check(saveSettings(path, loaded, error), "linked output fixture");
        check(uiText("截图并保存") == "Capture and save", "English catalog actually loaded");
        {
            MainWindow window(nullptr, path, "-");
            auto* mode = window.findChild<QComboBox*>("appearanceColorMode"); auto* language = window.findChild<QComboBox*>("appearanceLanguage");
            check(mode && language, "appearance controls present");
            check(!window.windowIcon().pixmap(16).isNull() && !window.windowIcon().pixmap(256).isNull(), "shipped multi-size icon loaded by Qt");
            auto* nav = window.findChild<QListWidget*>("workspaceNavigation"); auto* sourceTabs = window.findChild<QTabWidget*>("sourceViews");
            check(nav && nav->count() == 5 && sourceTabs, "visual navigation present");
            auto* overviewSources = window.findChild<QListWidget*>("overviewSources"); auto* outputs = window.findChild<QListWidget*>("overviewOutputs"); auto* table = window.findChild<QTableWidget*>("regions");
            check(overviewSources && outputs && outputs->count() == 3 && overviewSources->count() == 2, "next outputs enumerate two originals and one scene");
            outputs->setCurrentRow(2); settle();
            check(overviewSources->item(0)->data(linkedItemRole).toBool() && overviewSources->item(1)->data(linkedItemRole).toBool() && overviewSources->item(0)->background().style() == Qt::NoBrush, "scene output highlights both actual source cards");
            overviewSources->setCurrentRow(1); settle(); check(table->currentRow() == 1 && window.findChild<QListWidget*>("sourceCards")->currentRow() == 1, "source selection synchronizes views");
            window.findChild<QPushButton*>("applySettings")->click(); settle(); check(table->currentRow() == 1, "apply preserves selected region identity");
            nav->setCurrentRow(1); check(sourceTabs->currentIndex() == 0, "source default is display layout");
            sourceTabs->setCurrentIndex(1); nav->setCurrentRow(0); nav->setCurrentRow(1); check(sourceTabs->currentIndex() == 0, "source reentry resets to display layout");
            nav->setCurrentRow(3); check(window.findChild<QListWidget*>("batchCards")->count() == 1 && window.findChild<QListWidget*>("batchImages")->count() == 1, "gallery follows portable manifest paths and excludes invalid artifacts");
            check(window.findChild<QPushButton*>("captureSave")->text() == "Capture and save", "actual button localized");
            if(app.platformName()=="offscreen") {
                window.resize(1500,960);window.show();settle();
                auto* left=window.findChild<QWidget*>("workbenchSources");auto* center=window.findChild<QWidget*>("workbenchLayout");auto* right=window.findChild<QWidget*>("workbenchOutputs");
                check(left && center && right && left->geometry().top()==center->geometry().top() && left->height()==center->height() && center->height()==right->height(),"three complete panel bounds align");
                check(left->property("card").toBool() && center->property("card").toBool() && right->property("card").toBool(),"three panels share theme surface");
                check(window.findChild<QLabel*>("workbenchSourcesTitle")->height()==window.findChild<QLabel*>("workbenchLayoutTitle")->height() && window.findChild<QLabel*>("workbenchOutputsTitle")->height()==window.findChild<QLabel*>("workbenchLayoutTitle")->height(),"common title height");
                window.hide();
            }
            mode->setCurrentIndex(mode->findData("dark")); app.processEvents();
            check(activeColorMode() == "dark" && window.palette().color(QPalette::Window).lightness() < 90 && window.palette().color(QPalette::Text).lightness() > 190, "dark widgets have contrasting palette");
            auto* status = window.findChild<QLabel*>("captureStatus"); const auto darkStatus = status->palette().color(QPalette::WindowText);
            mode->setCurrentIndex(mode->findData("light")); settle(); check(status->palette().color(QPalette::WindowText).lightness() < 150 && status->palette().color(QPalette::WindowText) != darkStatus, "status color follows palette change");
            const int evidence = app.arguments().indexOf("--evidence");
            if (evidence >= 0 && evidence + 1 < app.arguments().size()) {
                check(app.platformName() == "offscreen", "component evidence must not operate the desktop");
                const auto folder = app.arguments()[evidence + 1]; check(QDir().mkpath(folder), "evidence folder");
                QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf"); window.resize(1500, 960); window.show();
                for (const auto& theme : {"light", "dark"}) {
                    mode->setCurrentIndex(mode->findData(theme));
                    for (int page = 0; page < 5; ++page) { nav->setCurrentRow(page); settle(); check(window.grab().save(folder + QString("/component-%1-%2.png").arg(theme).arg(page)), "offscreen component image"); }
                }
                window.hide();
            }
            mode->setCurrentIndex(mode->findData("dark")); settle();
            language->setCurrentIndex(language->findData("en-US")); window.findChild<QPushButton*>("applySettings")->click();
        }
        check(loadSettings(path, loaded, error) && loaded.language == "en-US" && loaded.colorMode == "dark", "chosen appearance persisted");
        installLanguage(loaded.language); applyColorMode(loaded.colorMode); check(uiText("外观") == "Appearance", "restart language resolution");
        QImage source(32, 16, QImage::Format_ARGB32); source.fill(QColor("#d92c41")); const QImage original = source;
        core::SceneTemplate scene; scene.id = "s"; scene.name = "Scene"; scene.width = 32; scene.height = 16;
        core::Layer layer; layer.id = "l"; layer.sourceRegionId = "r"; layer.destination = {0, 0, 32, 16, 0}; scene.layers.push_back(layer);
        SceneSources sources{{"r", {source, "artifact"}}}; const auto dark = renderScene(scene, sources);
        applyColorMode("light"); const auto light = renderScene(scene, sources);
        check(dark.image == light.image && source == original, "theme never changes captured or exported pixels");
        LayerItem item(layer, source, false, "#1 Voltage"); bool independent = false;
        for (auto* child : item.childItems()) independent |= bool(child->flags() & QGraphicsItem::ItemIgnoresTransformations);
        check(independent, "identity text is independent of image transforms");
        installLanguage("zh-CN"); check(uiText("外观") == QString::fromUtf8("外观"), "Chinese fallback");
        QDialogButtonBox chinese(QDialogButtonBox::Cancel); check(chinese.button(QDialogButtonBox::Cancel)->text().contains(QString::fromUtf8("取消")), "Chinese standard dialog buttons translated");
        installLanguage("en-US"); QDialogButtonBox english(QDialogButtonBox::Cancel); check(english.button(QDialogButtonBox::Cancel)->text().contains("Cancel"), "English standard dialog buttons restored");
        auto draft = loaded; draft.profiles[0].regions.clear(); draft.profiles[0].sceneIds.clear(); draft.profiles[0].outputMode = "originals";
        draft.scenes.clear(); draft.selectedSceneIds.clear(); draft.outputMode = "originals"; draft.clipboardRegion.clear();
        const auto emptyPath = temp.filePath("empty-settings.json");
        check(saveSettings(emptyPath, draft, error), "empty profile persistence");
        Settings empty; check(loadSettings(emptyPath, empty, error) && empty.profiles[0].regions.empty(), "empty profile roundtrip");
        {
            MainWindow window(nullptr, emptyPath, "-"); settle();
            check(!window.findChild<QPushButton*>("captureSave")->isEnabled() && !window.findChild<QPushButton*>("captureCopy")->isEnabled(), "empty-state capture actions disabled");
            window.findChild<QPushButton*>("applySettings")->click(); settle();
            check(loadSettings(emptyPath, empty, error) && empty.profiles[0].regions.empty(), "apply empty draft does not create fake regions");
            const auto sequence = empty.sequence; bool captured = false; window.captureFinished = [&](auto) { captured = true; };
            window.triggerCapture(0); settle();
            check(loadSettings(emptyPath, empty, error) && empty.sequence == sequence && !captured, "programmatic capture also rejects empty profile before admission");
            window.findChild<QPushButton*>("addRegion")->click(); settle();
            check(window.findChild<QPushButton*>("captureSave")->isEnabled(), "adding source makes actions available");
            window.findChild<QPushButton*>("applySettings")->click();
            check(loadSettings(emptyPath, empty, error) && empty.profiles[0].regions.size() == 1 && empty.profiles[0].regions[0].displayNumber >= 4, "empty draft can add a source without number reuse");
        }
        std::cout << "APPEARANCE_IDENTITY_MIGRATION_PASS\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
