// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "main_window.h"
#include "adapters/win_capture.h"
#include "adapters/report_queue.h"
#include "adapters/context_store.h"
#include "adapters/local_host.h"
#include "desktop/region_editor.h"
#include "desktop/scene_editor.h"
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "desktop/mini_capture_window.h"
#include "adapters/live_preview.h"
#include "desktop/live_output_window.h"
#include "desktop/capture_visibility.h"
#include "core/scene_geometry.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMap>
#include <QPainter>
#include <QPointer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRunnable>
#include <QSplitter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSystemTrayIcon>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static std::string id() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
static QIcon appIcon()
{
    return QIcon(":/brand/aitsnap.ico");
}
MainWindow::MainWindow(QWidget* parent, QString path, QString pipeName, std::function<void(const QString&)> startupStage) : QMainWindow(parent), configurationPath_(path.isEmpty() ? settingsPath() : path)
{
    const auto stage=[&](const char* name){if(startupStage)startupStage(QString::fromLatin1(name));};
    stage("main-constructor-enter");
    setWindowTitle(aitsnap::uiText("AITSnap · 多屏截图")); resize(1120, 780); setWindowIcon(appIcon());
    settings_ = defaultSettings(); QString loadError;
    settings_.outputDirectory = QFileInfo(configurationPath_).absolutePath() + "/captures";
    if (QFile::exists(configurationPath_)) loadSettings(configurationPath_, settings_, loadError);
    if (settings_.profiles.empty()) {
        WinCaptureBackend backend; core::CaptureProfile p; p.id = id(); p.name = uiText("默认多屏").toStdString();
        try { p.calibratedDisplays = backend.displays(); } catch (const std::exception& e) { loadError = QString::fromUtf8(e.what()); }
        for (int i = 0; i < 5; ++i) {
            const auto b = p.calibratedDisplays.empty() ? core::Rect{0, 0, 1920, 1080} : p.calibratedDisplays[std::size_t(i) % p.calibratedDisplays.size()].rect;
            p.regions.push_back({id(), uiText("区域 %1").arg(i + 1).toStdString(), {b.x + 60 + i * 20, b.y + 60 + i * 20, std::max(1, std::min(640, b.width / 2)), std::max(1, std::min(400, b.height / 2))}, true});
        }
        core::numberRegions(p); settings_.profiles.push_back(p);
    }
    if (QFile::exists(configurationPath_ + ".before-management.json")) loadError = "Previous management operation needs recovery: " + configurationPath_ + ".before-management.json";
    configurationBlocked_ = !loadError.isEmpty();
    livePreview_=new LivePreviewService(this);livePreview_->changed=[this]{refreshWorkspace();updateLiveOutputs();};
    stage("main-settings-loaded");
    buildUi(); stage("main-ui-built");
    mini_=new MiniCaptureWindow(QFileInfo(configurationPath_).absolutePath()+"/mini-window.json",this);
    mini_->captureRequested=[this](int mode){captureActions_[mode]->trigger();};
    mini_->openMain=[this]{restoreMiniAfterCapture_=false;showNormal();raise();activateWindow();};
    mini_->pauseRequested=[this](bool paused){if(pauseCaptureAction_)pauseCaptureAction_->setChecked(paused);};
    setupTray(); stage("main-tray-created"); loadProfile(); stage("main-profile-loaded"); qApp->installNativeEventFilter(&hotkeys_);
    reportQueue_ = new ReportQueue(QFileInfo(configurationPath_).absolutePath() + "/report-jobs", QCoreApplication::applicationDirPath() + "/aitsnap-word-bridge.exe", this);
    reportQueue_->changed = [this] { refreshReportStatus(); }; refreshReportStatus();
    stage("main-report-queue-loaded");
    contextStore_ = std::make_unique<ContextStore>(QFileInfo(configurationPath_).absolutePath() + "/context");
    if (!contextStore_->diagnostic().isEmpty()) contextStatus_->setText(contextStore_->diagnostic());
    hotkeys_.triggered = [this](int mode) { if(mode>=0 && mode<3)captureActions_[mode]->trigger(); };
    hotkeys_.taskbarCreated = [this] { QTimer::singleShot(250, this, [this] { tray_->hide(); tray_->show(); }); };
    QString e;
    // Retain the requested bindings even when their first OS registration fails,
    // so pause/resume can retry them rather than reporting success for zero keys.
    hotkeys_.pause(true,e);
    if (!hotkeys_.configureBindings(hotkeyBindings(settings_), e) || !hotkeys_.pause(false,e)) setStatus(uiText("全局热键暂不可用；仍可编辑和使用鼠标截图。%1").arg(e), true);
    else setStatus(aitsnap::uiText("就绪 · %1 保存，%2 复制，%3 保存并复制").arg(settings_.saveHotkey, settings_.copyHotkey, settings_.bothHotkey));
    if (!loadError.isEmpty()) setStatus(aitsnap::uiText("配置未载入，原文件保留：") + loadError + aitsnap::uiText("。核对后应用配置。"), true);
    stage("main-context-hotkeys-loaded");
    startHost(std::move(pipeName)); stage("main-constructor-return");
}
MainWindow::~MainWindow()
{
    if(livePreview_){livePreview_->changed={};delete livePreview_;livePreview_=nullptr;}
    localHost_.reset(); // stop admission before tearing down desktop state
    captureFinished = {};
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall); // finish saved-batch receipts before receiver destruction
    if (reportQueue_) { reportQueue_->changed = {}; delete reportQueue_; reportQueue_ = nullptr; }
}
void MainWindow::setupTray()
{
    tray_ = new QSystemTrayIcon(windowIcon(), this); auto* menu = new QMenu(this);
    auto* show = menu->addAction(aitsnap::uiText("打开区域与输出设置")); connect(show, &QAction::triggered, this, [this] { showNormal(); raise(); activateWindow(); });
    auto* mini=menu->addAction(uiText("迷你截图窗"));mini->setObjectName("showMiniWindow");connect(mini,&QAction::triggered,this,[this]{showMiniWindow();});
    menu->addAction(regionEditAction_);
    for(auto* action:captureActions_)menu->addAction(action);
    profileMenu_ = menu->addMenu(aitsnap::uiText("切换截图配置")); rebuildProfileMenu();
    sceneMenu_ = menu->addMenu(aitsnap::uiText("拼图场景 / 启用切换")); rebuildSceneMenu();
    auto* sceneEdit = menu->addAction(aitsnap::uiText("打开自由拼图")); connect(sceneEdit, &QAction::triggered, this, [this] { QTimer::singleShot(180, this, [this] { editScene(false); }); });
    auto* pause = menu->addAction(aitsnap::uiText("暂停截图热键")); pause->setObjectName("pauseCapture");pause->setCheckable(true);pauseCaptureAction_=pause;
    connect(pause, &QAction::toggled, this, [this](bool checked) {
        // An OS hotkey conflict is not an explicit request to pause mouse capture.
        paused_ = checked;
        QString e; if (hotkeys_.pause(checked || editingHotkey_, e)) setStatus(checked ? aitsnap::uiText("截图已暂停") : aitsnap::uiText("截图已恢复"));
        else setStatus(uiText("全局热键暂不可用；仍可编辑和使用鼠标截图。%1").arg(e), true);
        refreshWorkspace();
    });
    auto* folder = menu->addAction(aitsnap::uiText("打开输出目录")); connect(folder, &QAction::triggered, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(settings_.outputDirectory)); });
    menu->addSeparator(); auto* quit = menu->addAction(aitsnap::uiText("退出 AITSnap")); connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    tray_->setContextMenu(menu); tray_->setToolTip("AITSnap"); tray_->show();
    connect(tray_, &QSystemTrayIcon::activated, this, [this](auto r) { if (r == QSystemTrayIcon::DoubleClick) { showNormal(); raise(); activateWindow(); } });
}
void MainWindow::rebuildProfileMenu()
{
    if (!profileMenu_) return;
    profileMenu_->clear();
    for (int i = 0; i < int(settings_.profiles.size()); ++i) {
        auto* a = profileMenu_->addAction(qs(settings_.profiles[std::size_t(i)].name)); a->setCheckable(true); a->setChecked(i == settings_.activeProfile);
        connect(a, &QAction::triggered, this, [this, i] { profiles_->setCurrentIndex(i); });
    }
}
void MainWindow::loadProfile()
{
    const auto* previous = regions_->currentRow() >= 0 ? regions_->item(regions_->currentRow(), 1) : nullptr;
    const auto selectedId = previous ? previous->data(Qt::UserRole).toString() : QString{};
    const QSignalBlocker regionBlock(regions_);
    const auto& p = settings_.profiles[std::size_t(settings_.activeProfile)]; profileName_->setText(qs(p.name)); regions_->setRowCount(0); clipboard_->clear();
    settings_.selectedSceneIds.clear(); for (const auto& id : p.sceneIds) settings_.selectedSceneIds.append(qs(id)); settings_.outputMode = qs(p.outputMode);
    const auto wantedClipboard = p.clipboardSelection.empty() ? settings_.clipboardRegion : qs(p.clipboardSelection);
    for (const auto& r : p.regions) {
        int row = regions_->rowCount(); regions_->insertRow(row);
        auto* enabled = new QTableWidgetItem; enabled->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked); regions_->setItem(row, 0, enabled);
        auto* name = new QTableWidgetItem(qs(r.name)); name->setData(Qt::UserRole, qs(r.id)); name->setData(Qt::UserRole + 1, r.displayNumber); regions_->setItem(row, 1, name);
        regions_->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(r.displayNumber)));
        const int values[]{r.rect.x, r.rect.y, r.rect.width, r.rect.height};
        for (int i = 0; i < 4; ++i) regions_->setItem(row, i + 2, new QTableWidgetItem(QString::number(values[i])));
        if (r.enabled) clipboard_->addItem(regionLabel(r), qs(r.id));
    }
    for (const auto& scene : settings_.scenes) clipboard_->addItem(aitsnap::uiText("拼图：") + qs(scene.name), "scene:" + qs(scene.id));
    int selectedRow = 0;
    for (int row = 0; row < regions_->rowCount(); ++row) if (regions_->item(row, 1)->data(Qt::UserRole).toString() == selectedId) selectedRow = row;
    if (regions_->rowCount()) regions_->setCurrentCell(selectedRow, 1);
    int index = clipboard_->findData(wantedClipboard); if (index >= 0) clipboard_->setCurrentIndex(index);
    settings_.clipboardRegion = clipboard_->currentData().toString();
    outputMode_->setCurrentIndex(std::max(0, outputMode_->findData(settings_.outputMode))); rebuildScenes(); rebuildSceneMenu();
    refreshWorkspace();
}
void MainWindow::addRegion()
{
    int row = regions_->rowCount(); regions_->insertRow(row);
    auto* enabled = new QTableWidgetItem; enabled->setCheckState(Qt::Checked); regions_->setItem(row, 0, enabled);
    auto& profile = settings_.profiles[std::size_t(settings_.activeProfile)];
    const int number = profile.nextRegionNumber++;
    auto* name = new QTableWidgetItem(aitsnap::uiText("区域 %1").arg(number)); name->setData(Qt::UserRole, QString::fromStdString(id())); name->setData(Qt::UserRole + 1, number); regions_->setItem(row, 1, name);
    regions_->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(number)));
    const int values[]{0, 0, 640, 400}; for (int i = 0; i < 4; ++i) regions_->setItem(row, i + 2, new QTableWidgetItem(QString::number(values[i])));
}
bool MainWindow::collectSettings(Settings& c, QString& e)
{
    auto& p = c.profiles[std::size_t(c.activeProfile)]; p.name = profileName_->text().trimmed().toStdString(); p.regions.clear();
    if (p.name.empty()) { e = aitsnap::uiText("请输入配置名称"); return false; }
    for (int row = 0; row < regions_->rowCount(); ++row) {
        int v[4]{};
        for (int i = 0; i < 4; ++i) { bool ok{}; const auto* item = regions_->item(row, i + 2); v[i] = item ? item->text().toInt(&ok) : 0; if (!ok) { e = aitsnap::uiText("第 %1 行需要整数物理坐标和尺寸").arg(row + 1); return false; } }
        const auto* name = regions_->item(row, 1); p.regions.push_back({name->data(Qt::UserRole).toString().toStdString(), name->text().trimmed().toStdString(), {v[0], v[1], v[2], v[3]}, regions_->item(row, 0)->checkState() == Qt::Checked, name->data(Qt::UserRole + 1).toInt()});
    }
    if (auto err = core::validateProfileDefinition(p); err) { e = qs(err.message); return false; }
    ++p.revision; c.outputDirectory = QDir(directory_->text().trimmed()).absolutePath(); c.format = format_->currentText(); c.filenameTemplate = filename_->text();
    c.saveHotkey = saveKey_->text(); c.copyHotkey = copyKey_->text(); c.bothHotkey = bothKey_->text(); c.clipboardRegion = clipboard_->currentData().toString();
    c.saveHotkeyAlternate=alternateKeys_[0]->text();c.copyHotkeyAlternate=alternateKeys_[1]->text();c.bothHotkeyAlternate=alternateKeys_[2]->text();
    for(int mode=0;mode<3;++mode){c.hotkeysEnabled[std::size_t(mode)]=hotkeyEnabled_[mode]->isChecked();}
    const QString primary[]{c.saveHotkey,c.copyHotkey,c.bothHotkey};const QString alternate[]{c.saveHotkeyAlternate,c.copyHotkeyAlternate,c.bothHotkeyAlternate};
    for(int mode=0;mode<3;++mode)if(c.hotkeysEnabled[std::size_t(mode)] && primary[mode].trimmed().isEmpty() && alternate[mode].trimmed().isEmpty()){e=uiText("动作 %1 至少需要一个快捷键，或关闭该动作的热键。").arg(mode+1);return false;}
    c.testId = testId_->text(); c.device = device_->text(); c.station = station_->text(); c.operatorName = operator_->text(); c.startInTray = startTray_->isChecked();
    c.language = language_->currentData().toString(); c.colorMode = colorMode_->currentData().toString();
    c.livePreviewEnabled=livePreviewEnabled_->isChecked();c.livePreviewFps=livePreviewRate_->currentData().toInt();
    c.outputMode = outputMode_->currentData().toString(); c.selectedSceneIds.clear();
    for (int i = 0; i < scenes_->count(); ++i) if (scenes_->item(i)->checkState() == Qt::Checked) c.selectedSceneIds.append(scenes_->item(i)->data(Qt::UserRole).toString());
    p.outputMode = c.outputMode.toStdString(); p.sceneIds.clear(); for (const auto& id : c.selectedSceneIds) p.sceneIds.push_back(id.toStdString()); p.clipboardSelection = c.clipboardRegion.toStdString();
    c.report = selectedReportOptions();
    if(c.report.enabled && !validateReportText(c.report.text,e))return false;
    core::CaptureRequest sample; sample.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); sample.profile = p;
    core::Region example{"", uiText("区域").toStdString(), {0, 0, 1, 1}, true, 1};
    expandFilename(c, sample, p.regions.empty() ? example : p.regions.front(), e); return e.isEmpty();
}
bool MainWindow::applySettings()
{
    Settings c = settings_; QString e;
    if (QFile::exists(configurationPath_ + ".before-management.json")) { setStatus("Previous management operation needs recovery: " + configurationPath_ + ".before-management.json", true); return false; }
    if (configurationBlocked_ && QFile::exists(configurationPath_)) {
        const auto backup = configurationPath_ + ".preserved-" + QString::fromStdString(id());
        if (!QFile::copy(configurationPath_, backup)) { setStatus(aitsnap::uiText("无法保留旧配置，配置未改写。"), true); return false; }
    }
    if(!collectSettings(c,e)){setStatus(e,true);return false;}
    const bool changedKeys=c.saveHotkey!=settings_.saveHotkey || c.copyHotkey!=settings_.copyHotkey || c.bothHotkey!=settings_.bothHotkey ||
        c.saveHotkeyAlternate!=settings_.saveHotkeyAlternate || c.copyHotkeyAlternate!=settings_.copyHotkeyAlternate || c.bothHotkeyAlternate!=settings_.bothHotkeyAlternate || c.hotkeysEnabled!=settings_.hotkeysEnabled;
    // New shortcut edits remain transactional. An unchanged saved shortcut can
    // become unavailable in another process; that must not gate every editor,
    // mini window, or mouse capture through this shared settings entry point.
    if(changedKeys && editingHotkey_ && !paused_){Hotkeys validation;if(!validation.configureBindings(hotkeyBindings(c),e)){setStatus(e,true);return false;}}
    QString hotkeyWarning;
    bool registered=hotkeys_.configureBindings(hotkeyBindings(c), e);
    if(registered)registered=hotkeys_.pause(paused_ || editingHotkey_,e);
    if(!registered){if(changedKeys){setStatus(e,true);return false;}hotkeyWarning=e;e.clear();}
    if (!saveSettings(configurationPath_, c, e)) { QString restore; if(!hotkeys_.configureBindings(hotkeyBindings(settings_), restore))e+="; "+restore; if (paused_ || editingHotkey_) hotkeys_.pause(true, restore); setStatus(e, true); return false; }
    settings_ = c; configurationBlocked_ = false; profiles_->setItemText(settings_.activeProfile, qs(c.profiles[std::size_t(c.activeProfile)].name)); rebuildProfileMenu(); loadProfile();
    if(!hotkeyWarning.isEmpty())setStatus(uiText("配置已保存 · 全局热键暂不可用，仍可编辑和使用鼠标截图。%1").arg(hotkeyWarning),true);
    else setStatus(paused_?uiText("配置已保存 · 截图已暂停，恢复时注册热键"):(editingHotkey_?uiText("配置已保存 · 离开热键输入框后恢复注册"):uiText("配置已保存 · 热键已应用")));
    return true;
}
void MainWindow::setStatus(const QString& text, bool error)
{
    status_->setText(text); status_->setProperty("statusError", error);
    status_->style()->unpolish(status_); status_->style()->polish(status_); status_->update();
    if (tray_) tray_->setToolTip("AITSnap · " + text.left(100));
    if(mini_)mini_->setState(profiles_->currentText(),text,captureActions_[0]->isEnabled(),paused_);
}
void MainWindow::showMiniWindow()
{
    if(!applySettings())return;
    hide();mini_->setState(profiles_->currentText(),status_->text(),captureActions_[0]->isEnabled(),paused_);mini_->showMini();
}
void MainWindow::updateCaptureActions(bool canCapture)
{
    if(!captureActions_[0])return;
    const QStringList labels{uiText("截图并保存"),uiText("截图并复制"),uiText("截图、保存并复制")};
    const QStringList primary{saveKey_->text().trimmed(),copyKey_->text().trimmed(),bothKey_->text().trimmed()};
    QStringList keys;
    for(int mode=0;mode<3;++mode){QStringList bindings;if(hotkeyEnabled_[mode]->isChecked()){if(!primary[mode].isEmpty())bindings<<primary[mode];const auto alternate=alternateKeys_[mode]->text().trimmed();if(!alternate.isEmpty() && !bindings.contains(alternate,Qt::CaseInsensitive))bindings<<alternate;}keys<<(bindings.isEmpty()?uiText("未启用热键"):bindings.join(" / "));}
    const QStringList descriptions{uiText("重新采集启用区域并保存原图/拼图；启用时加入 Word 报告。"),uiText("重新采集启用区域，只复制所选输出；不保存文件、不加入 Word。"),uiText("重新采集、保存原图/拼图并复制所选输出；启用时加入 Word。")};
    for(int i=0;i<3;++i){auto* action=captureActions_[i];action->setText(labels[i]+"\t"+keys[i]);action->setToolTip(descriptions[i]+"\n"+uiText("快捷键：%1（全局）").arg(keys[i]));action->setEnabled(canCapture && !busy_ && !paused_ && !configurationBlocked_);}
    if(mini_)mini_->setState(profiles_->currentText(),status_->text(),captureActions_[0]->isEnabled(),paused_);
}
void MainWindow::triggerCapture(int mode)
{
    if(mode<0 || mode>2){setStatus(uiText("无效的截图动作。"),true);return;}
    // Every UI entry commits the same draft before taking its immutable snapshot.
    if(busy_ || paused_ || configurationBlocked_ || editingRegions_ || editingScene_ || managingObjects_){startCapture(mode);return;}
    if(applySettings())startCapture(mode);
}
bool MainWindow::eventFilter(QObject* object,QEvent* event)
{
    if(event->type()==QEvent::FocusIn || event->type()==QEvent::FocusOut){
        const bool keyField=object==saveKey_ || object==copyKey_ || object==bothKey_ || object==alternateKeys_[0] || object==alternateKeys_[1] || object==alternateKeys_[2];
        if(keyField){
            if(event->type()==QEvent::FocusIn){editingHotkey_=true;QString error;hotkeys_.pause(true,error);}
            else QTimer::singleShot(0,this,[this]{auto* focus=QApplication::focusWidget();if(focus && focus->isVisible() && focus->window()->isActiveWindow() && (focus==saveKey_ || focus==copyKey_ || focus==bothKey_ || focus==alternateKeys_[0] || focus==alternateKeys_[1] || focus==alternateKeys_[2]))return;editingHotkey_=false;QString error;if(!hotkeys_.pause(paused_,error))setStatus(error,true);});
        }
    }
    return QMainWindow::eventFilter(object,event);
}
QString MainWindow::startCapture(int mode, const QJsonObject& options, const QJsonObject& eventSnapshot)
{
    if (managingObjects_) { setStatus(uiText("正在修改配置，请稍候。"), true); return {}; }
    if (editingScene_) { setStatus(aitsnap::uiText("拼图编辑中，截图热键暂停；保存或取消后恢复。"), true); return {}; }
    if (editingRegions_) { setStatus(aitsnap::uiText("区域编辑中，截图未接受；请完成或取消框选。"), true); return {}; }
    if (configurationBlocked_) { setStatus(aitsnap::uiText("原配置无法读取。核对并应用配置后才能截图。"), true); return {}; }
    if (paused_) { setStatus(aitsnap::uiText("截图已暂停，请从托盘恢复"), true); return {}; }
    if (busy_) { setStatus(aitsnap::uiText("上一批截图尚未完成，本次请求未接受"), true); return {}; }
    QString e; Settings snapshot = settings_; ++snapshot.sequence;
    if (options.contains("profileId")) {
        const auto chosen = options["profileId"].toString().toStdString(); const auto found = std::find_if(snapshot.profiles.begin(), snapshot.profiles.end(), [&](const auto& p) { return p.id == chosen; });
        if (found == snapshot.profiles.end()) return {}; snapshot.activeProfile = int(found - snapshot.profiles.begin()); snapshot.outputMode = qs(found->outputMode); snapshot.clipboardRegion = qs(found->clipboardSelection); snapshot.selectedSceneIds.clear(); for (const auto& scene : found->sceneIds) snapshot.selectedSceneIds.append(qs(scene));
    }
    if (options.contains("sceneIds")) { snapshot.selectedSceneIds.clear(); for (const auto& scene : options["sceneIds"].toArray()) snapshot.selectedSceneIds.append(scene.toString()); snapshot.outputMode = snapshot.selectedSceneIds.empty() ? "originals" : "both"; if (!snapshot.selectedSceneIds.empty()) snapshot.clipboardRegion = "scene:" + snapshot.selectedSceneIds.front(); else snapshot.clipboardRegion.clear(); }
    if (auto error = core::validateProfile(snapshot.profiles[std::size_t(snapshot.activeProfile)]); error) {
        setStatus(error.code == "NO_REGIONS" ? uiText("请先添加并启用源区域。") : qs(error.message), true); return {};
    }
    auto persistent = settings_; persistent.sequence = snapshot.sequence;
    if (!saveSettings(configurationPath_, persistent, e)) { setStatus(aitsnap::uiText("序号未持久化：") + e, true); return {}; }
    settings_.sequence = snapshot.sequence; core::CaptureRequest request;
    request.requestId = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz").toStdString() + "-" + id();
    request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); request.profile = snapshot.profiles[std::size_t(snapshot.activeProfile)];
    request.context.testId = snapshot.testId.toStdString(); request.context.deviceId = snapshot.device.toStdString();
    request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString();
    if (!selectedContextSnapshot_.isEmpty()) { request.context = selectedContext_; request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString(); request.contextSnapshotJson = QJsonDocument(selectedContextSnapshot_).toJson(QJsonDocument::Compact).toStdString(); }
    if (!eventSnapshot.isEmpty()) { const auto event = eventSnapshot["event"].toObject(); request.context = contextStore_->context(event["sourceSystem"].toString(), event["eventId"].toString()); request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString(); request.contextSnapshotJson = QJsonDocument(eventSnapshot).toJson(QJsonDocument::Compact).toStdString(); }
    currentRequestId_ = qs(request.requestId);
    busy_=true;if(livePreview_)livePreview_->setActive(false);restoreLiveWindows_.clear();for(auto* window:liveOutputs_)if(window->isVisible()){restoreLiveWindows_.push_back(window);window->hide();}
    restoreMiniAfterCapture_=mini_ && mini_->isVisible();if(restoreMiniAfterCapture_)mini_->hide();
    busy_ = true; updateCaptureActions(true); setStatus(aitsnap::uiText("正在后台采集 %1 个源区域…").arg(request.profile.regions.size())); QPointer<MainWindow> self(this);
    const bool flushWindow=restoreMiniAfterCapture_ || !restoreLiveWindows_.empty();
    QThreadPool::globalInstance()->start(QRunnable::create([self, snapshot, request, mode,flushWindow] {
        if(flushWindow)DwmFlush();
        WinCaptureBackend backend; auto result = std::make_shared<CaptureOutcome>(captureToFiles(backend, snapshot, request, mode != 1, mode != 0));
        if (self) QMetaObject::invokeMethod(self, [self, result, snapshot, mode] { if (self) self->completeCapture(result, snapshot, mode); }, Qt::QueuedConnection);
    }));
    return currentRequestId_;
}
void MainWindow::completeCapture(const std::shared_ptr<CaptureOutcome>& result, const Settings& snapshot, int mode)
{
    if (mode != 0) {
        core::OutputResult output{"clipboard", "", "FAILED", {"CLIPBOARD_SOURCE", "Selected region unavailable"}};
        QImage image;
        if (snapshot.clipboardRegion.startsWith("scene:")) { const auto scene = result->composites.find(snapshot.clipboardRegion.mid(6).toStdString()); if (scene != result->composites.end()) image = scene->second; }
        else {
            const auto& list = result->batch.captures;
            const auto it = std::find_if(list.begin(), list.end(), [&](const auto& c) { return snapshot.clipboardRegion.isEmpty() ? c.image.valid() : qs(c.region.id) == snapshot.clipboardRegion; });
            if (it != list.end() && it->image.valid()) image = toQImage(it->image);
        }
        if (!image.isNull()) {
            auto previous = GetClipboardSequenceNumber(); QApplication::clipboard()->setImage(image);
            if (GetClipboardSequenceNumber() != previous && QApplication::clipboard()->image() == image) { output.status = "COPIED"; output.error = {}; } else output.error = {"CLIPBOARD", "Clipboard readback failed"};
        }
        result->batch.outputs.push_back(output);
    }
    QString e; if (mode != 1 && !result->directory.isEmpty() && !writeBatchManifest(*result, e)) result->batch.outputs.push_back({"manifest", "", "FAILED", {"MANIFEST", e.toStdString()}});
    int captured = 0, saved = 0, failed = 0; QString detail;
    for (const auto& c : result->batch.captures) { if (c.error) { ++failed; detail = qs(c.error.code + ": " + c.error.message); } else ++captured; }
    for (const auto& o : result->batch.outputs) { if (o.status == "SAVED") ++saved; if (o.error) { ++failed; detail = qs(o.error.code + ": " + o.error.message); } }
    setStatus(aitsnap::uiText("已采集 %1 张 · 已保存 %2 张 · %3 ms%4").arg(captured).arg(saved).arg(result->elapsedMs, 0, 'f', 1).arg(failed ? " · " + detail : aitsnap::uiText(" · 完成")), failed > 0);
    for (const auto& c : result->batch.captures) if (c.image.valid()) { preview_->setImage(toQImage(c.image), regionLabel(c.region) + uiText(" · 最近批次")); break; }
    busy_ = false; lastOutcome_ = result;if(restoreMiniAfterCapture_){restoreMiniAfterCapture_=false;mini_->showMini();}for(auto* window:restoreLiveWindows_)window->show();restoreLiveWindows_.clear(); if (captureFinished) captureFinished(result);
    refreshWorkspace(); refreshBatches();
    if (mode != 1 && snapshot.report.enabled && !result->directory.isEmpty()) {
        QString error; if (!reportQueue_->enqueue(*result, snapshot.report, error)) { reportStatus_->setText(aitsnap::uiText("截图已完成，报告未入队：") + error); }
    }
}
void MainWindow::closeEvent(QCloseEvent* e)
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) { hide(); e->ignore(); } else e->accept();
}
void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if ((event->type() == QEvent::PaletteChange || event->type()==QEvent::WindowStateChange) && overviewSources_)
        QTimer::singleShot(0, this, [this] { refreshWorkspace(); });
}
ReportOptions MainWindow::selectedReportOptions() const
{
    ReportOptions options; options.enabled=reportEnabled_->isChecked();options.targetMode=reportTargetMode_->currentData().toString();
    options.documentPath=reportPath_->text().trimmed();options.itemMode=reportMode_->currentData().toString();options.layout=reportLayout_->currentData().toString();
    if(reportTextPreset_){options.text.preset=reportTextPreset_->currentData().toString();options.text.titleEnabled=reportTextEnabled_[0]->isChecked();options.text.captionEnabled=reportTextEnabled_[1]->isChecked();options.text.contextEnabled=reportTextEnabled_[2]->isChecked();options.text.titleTemplate=reportTextTemplates_[0]->toPlainText();options.text.captionTemplate=reportTextTemplates_[1]->toPlainText();options.text.contextTemplate=reportTextTemplates_[2]->toPlainText();}return options;
}
QString MainWindow::reportTarget(const QString& batchDirectory,const QString& issuedAtUtc,QString* diagnostic) const
{
    if(!reportQueue_)return {};
    QString error;const auto time=issuedAtUtc.isEmpty()?QDateTime::currentDateTime():QDateTime::fromString(issuedAtUtc,Qt::ISODateWithMs);
    const auto root=batchDirectory.isEmpty()?directory_->text().trimmed():QFileInfo(batchDirectory).absolutePath();
    const auto path=reportQueue_->resolveTarget(selectedReportOptions(),root,time,false,error);if(diagnostic)*diagnostic=error;return path;
}
void MainWindow::refreshReportStatus()
{
    if (!reportQueue_ || !reportStatus_) return; int committed = 0, pending = 0, uncertain = 0, failed = 0;
    for (const auto& job : reportQueue_->jobs()) { const auto state = job["state"].toString(); if (state == "COMMITTED") ++committed; else if (state == "UNCERTAIN") ++uncertain; else if (state == "FAILED") ++failed; else ++pending; }
    reportStatus_->setText(aitsnap::uiText("Word：已提交 %1 · 排队/执行 %2 · 待核对 %3 · 失败 %4").arg(committed).arg(pending).arg(uncertain).arg(failed) + (reportQueue_->diagnostic().isEmpty() ? "" : "\n" + reportQueue_->diagnostic()));
    QString targetError;const auto today=reportTarget({},{},&targetError);
    const bool automatic=reportTargetMode_->currentData()=="automatic";
    reportTargetStatus_->setText(today.isEmpty() ? (automatic && !QDir(directory_->text().trimmed()).exists()?uiText("首次保存截图后，自动在截图目录建立今日报告。"):uiText("报告目标：")+targetError) : (automatic?uiText("今日报告："):uiText("报告目标："))+QDir::toNativeSeparators(today));
    if(auto* open=findChild<QPushButton*>("openCurrentReport"))open->setEnabled(QFileInfo(today).isFile());
    reportTargetStatus_->setToolTip(today.isEmpty()?targetError:today);
    const auto target=lastOutcome_?reportTarget(lastOutcome_->directory,qs(lastOutcome_->batch.request.issuedAtUtc)):QString{};
    const bool linked = lastOutcome_ && !target.isEmpty() && reportQueue_->hasBatchForDocument(qs(lastOutcome_->batch.batchId),target);
    appendLast_->setToolTip(target);
    appendLast_->setText(linked ? uiText("本批已加入报告") : uiText("将最近保存批次加入报告"));
    appendLast_->setEnabled(lastOutcome_ && !lastOutcome_->directory.isEmpty() && !target.isEmpty() && !linked);
    if (batchCards_) selectBatch();
}
void MainWindow::showContextStore()
{
    QDialog dialog(this); dialog.setWindowTitle(aitsnap::uiText("只读目录与执行上下文")); dialog.resize(1180, 640); auto* layout = new QVBoxLayout(&dialog);
    auto* note = new QLabel(aitsnap::uiText("此处只关联证据，不执行指令。来源时间与首次接收时间独立；参数值不写入事件/批次/导出。只有完整且静态版本唯一的显式选择可供截图继承。")); note->setWordWrap(true); layout->addWidget(note);
    auto* actions = new QHBoxLayout; auto* importCatalog = new QPushButton(aitsnap::uiText("导入目录 JSON / CSV")); auto* importEvent = new QPushButton(aitsnap::uiText("接收事件 JSON")); auto* exportMetadata = new QPushButton(aitsnap::uiText("导出脱敏元数据（新文件）")); actions->addWidget(importCatalog); actions->addWidget(importEvent); actions->addWidget(exportMetadata); layout->addLayout(actions);
    auto* tabs = new QTabWidget; layout->addWidget(tabs, 1);
    auto* table = new QTableWidget(0, 10); table->setHorizontalHeaderLabels({aitsnap::uiText("来源"), aitsnap::uiText("事件"), "Run", "Step execution", "Invocation", aitsnap::uiText("尝试"), aitsnap::uiText("关联 / 静态版本"), aitsnap::uiText("来源 UTC"), aitsnap::uiText("首次接收 UTC"), aitsnap::uiText("类型")}); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); tabs->addTab(table, aitsnap::uiText("执行事件"));
    auto* tree = new QTreeWidget; tree->setHeaderLabels({aitsnap::uiText("定义 / 版本"), aitsnap::uiText("名称 / 类型"), aitsnap::uiText("实测来源 SHA-256")}); tabs->addTab(tree, aitsnap::uiText("静态目录"));
    auto refresh = [this, table, tree] {
        table->setRowCount(0); for (const auto& envelope : contextStore_->events()) { const auto event = envelope["event"].toObject(), sequence = event["sequence"].toObject(), command = event["command"].toObject(); const int row = table->rowCount(); table->insertRow(row);
            const QStringList values{event["sourceSystem"].toString(), event["eventId"].toString(), sequence["runId"].toString(), sequence["stepExecutionId"].toString(), command["invocationId"].toString(), command.contains("attempt") ? QString::number(command["attempt"].toInt()) : QString{}, event["correlationStatus"].toString() + "/" + envelope["definitionStatus"].toString(), event["sourceTimeUtc"].toString(), event["receivedAtUtc"].toString(), event["eventType"].toString()};
            for (int col = 0; col < values.size(); ++col) table->setItem(row, col, new QTableWidgetItem(values[col])); }
        tree->clear(); for (const auto& envelope : contextStore_->catalogs()) { const auto catalog = envelope["catalog"].toObject(); auto* item = new QTreeWidgetItem(tree, {catalog["catalogId"].toString() + "@" + catalog["catalogRevision"].toString(), catalog["sourceSystem"].toString(), envelope["measuredSourceDigest"].toString()});
            for (const auto& value : catalog["commands"].toArray()) { const auto c = value.toObject(); new QTreeWidgetItem(item, {c["commandId"].toString(), c["name"].toString()}); }
            for (const auto& value : catalog["sequences"].toArray()) { const auto s = value.toObject(); auto* sequence = new QTreeWidgetItem(item, {s["sequenceId"].toString() + "@" + s["revision"].toString(), s["name"].toString()});
                for (const auto& value2 : s["steps"].toArray()) { const auto step = value2.toObject(); new QTreeWidgetItem(sequence, {step["stepId"].toString(), step["kind"].toString() + ": " + (step["kind"] == "command" ? step["commandId"].toString() : step["subsequenceId"].toString() + "@" + step["subsequenceRevision"].toString())}); } }
        } tree->resizeColumnToContents(0); tree->resizeColumnToContents(1);
    }; refresh();
    connect(importCatalog, &QPushButton::clicked, &dialog, [this, &dialog, note, refresh] { const auto path = QFileDialog::getOpenFileName(&dialog, aitsnap::uiText("选择来源目录（只读导入）"), {}, "Catalog (*.json *.csv)"); if (path.isEmpty()) return; QString error; note->setText(contextStore_->importCatalog(path, error) ? aitsnap::uiText("已保存实测哈希和不可变来源副本；未修改来源文件。") : error); refresh(); });
    connect(importEvent, &QPushButton::clicked, &dialog, [this, &dialog, note, refresh] { const auto path = QFileDialog::getOpenFileName(&dialog, aitsnap::uiText("选择规范化事件 JSON"), {}, "Event (*.json)"); if (path.isEmpty()) return; QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { note->setText(aitsnap::uiText("事件无法读取或超过 1 MiB。")); return; } const auto result = contextStore_->ingest(QJsonDocument::fromJson(file.readAll()).object()); note->setText(result.error ? qs(result.error.message) : aitsnap::uiText("事件已接收或去重；未切换截图上下文。")); refresh(); });
    connect(exportMetadata, &QPushButton::clicked, &dialog, [this, &dialog, note] { const auto path = QFileDialog::getSaveFileName(&dialog, aitsnap::uiText("导出脱敏元数据，不覆盖已有文件"), {}, "JSON (*.json)", nullptr, QFileDialog::DontConfirmOverwrite); if (path.isEmpty()) return; QString error; note->setText(contextStore_->exportMetadata(path, true, error) ? aitsnap::uiText("脱敏元数据已导出。") : error); });
    auto* select = new QPushButton(aitsnap::uiText("明确选择此事件作为截图上下文")); auto* clear = new QPushButton(aitsnap::uiText("清除关联")); auto* bottom = new QHBoxLayout; bottom->addWidget(select); bottom->addWidget(clear); layout->addLayout(bottom);
    connect(select, &QPushButton::clicked, &dialog, [this, table, note] { if (table->currentRow() < 0) return; const auto source = table->item(table->currentRow(), 0)->text(), id = table->item(table->currentRow(), 1)->text(); const auto context = contextStore_->context(source, id); const auto snapshot = contextStore_->snapshot(source, id);
        if (context.correlationStatus != "EXACT" || snapshot["definitionStatus"] != "RESOLVED") { selectedContext_ = {}; selectedContextSnapshot_ = {}; contextStatus_->setText(aitsnap::uiText("截图上下文：未关联（所选身份或静态版本不唯一）。")); note->setText(aitsnap::uiText("此事件保留歧义/未关联状态，不能自动供热键截图继承。")); return; }
        selectedContext_ = context; selectedContextSnapshot_ = snapshot; contextStatus_->setText(aitsnap::uiText("显式上下文：%1 / %2 · run=%3 · attempt=%4").arg(source, id, qs(context.sequenceRunId)).arg(context.attempt)); note->setText(aitsnap::uiText("后续截图将冻结此事件的身份、时间和静态版本；不会随新事件自动漂移。")); });
    connect(clear, &QPushButton::clicked, &dialog, [this] { selectedContext_ = {}; selectedContextSnapshot_ = {}; contextStatus_->setText(aitsnap::uiText("截图上下文：未关联。")); });
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); layout->addWidget(close); dialog.exec();
}
void MainWindow::showReportQueue()
{
    QDialog dialog(this); dialog.setWindowTitle(aitsnap::uiText("Word 报告任务")); dialog.resize(980, 500); auto* layout = new QVBoxLayout(&dialog);
    auto* note = new QLabel(aitsnap::uiText("截图与 Word 独立。失败或待核对会暂停同一报告的后续任务。排除原因后，仅修改前失败的任务可重试；UNCERTAIN 必须核对标记，不会盲目重插。未发布 pending.docx 保留供核对。")); note->setWordWrap(true); layout->addWidget(note);
    auto* table = new QTableWidget(0, 5); table->setObjectName("reportJobs"); table->setHorizontalHeaderLabels({aitsnap::uiText("任务"), aitsnap::uiText("报告"), aitsnap::uiText("状态"), aitsnap::uiText("尝试"), aitsnap::uiText("错误 / 原因")}); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setTextElideMode(Qt::ElideMiddle); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setColumnWidth(0, 120); table->setColumnWidth(1, 270); table->setColumnWidth(2, 110); table->setColumnWidth(3, 50); table->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table);
    auto refresh = [this, table] { const auto selected = table->currentRow() >= 0 && table->item(table->currentRow(), 0) ? table->item(table->currentRow(), 0)->text() : QString{}; table->setRowCount(0);
        for (const auto& job : reportQueue_->jobs()) { const int row = table->rowCount(); table->insertRow(row); const auto ext = job["extensions"].toObject();
            static const QMap<QString, QString> labels{{"QUEUED", aitsnap::uiText("排队中")}, {"RUNNING", aitsnap::uiText("正在写入")}, {"COMMITTED", aitsnap::uiText("已保存提交")}, {"RETRYABLE", aitsnap::uiText("等待重试")}, {"FAILED", aitsnap::uiText("失败")}, {"UNCERTAIN", aitsnap::uiText("待核对")}};
            const auto state = job["state"].toString();
            const QStringList values{job["jobId"].toString(), job["target"].toObject()["documentPath"].toString(), labels.value(state, state), QString::number(job["attemptCount"].toInt()), job["errorCode"].toString() + " " + ext["lastResult"].toObject()["message"].toString()};
            for (int col = 0; col < values.size(); ++col) { auto* cell = new QTableWidgetItem(values[col]); cell->setToolTip(col == 2 ? state : values[col]); table->setItem(row, col, cell); } if (values[0] == selected) table->selectRow(row); }
    }; refresh();
    auto* inspect = new QPushButton(aitsnap::uiText("核对所选待核对任务的文档标记")); layout->addWidget(inspect);
    connect(inspect, &QPushButton::clicked, &dialog, [this, table, note, refresh] { if (table->currentRow() < 0) return; QString error; if (!reportQueue_->inspectMarker(table->item(table->currentRow(), 0)->text(), error)) note->setText(error); refresh(); });
    auto* retry = new QPushButton(aitsnap::uiText("已排除原因：重试所选修改前失败任务")); retry->setObjectName("retryFailedReport"); layout->addWidget(retry);
    connect(retry, &QPushButton::clicked, &dialog, [this, table, note, refresh] { if (table->currentRow() < 0) return; QString error; note->setText(reportQueue_->retryFailed(table->item(table->currentRow(), 0)->text(), error) ? aitsnap::uiText("原任务已重新排队，保留顺序与重试历史。") : error); refresh(); });
    QTimer timer; connect(&timer, &QTimer::timeout, &dialog, refresh); timer.start(1000); auto* close = new QDialogButtonBox(QDialogButtonBox::Close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); layout->addWidget(close); dialog.exec();
}
void MainWindow::rebuildScenes()
{
    const QSignalBlocker block(scenes_);
    const auto current = scenes_->currentItem() ? scenes_->currentItem()->data(Qt::UserRole).toString() : QString{};
    scenes_->clear(); const auto& profile = settings_.profiles[std::size_t(settings_.activeProfile)];
    for (const auto& scene : settings_.scenes) {
        const bool compatible = std::all_of(scene.layers.begin(), scene.layers.end(), [&](const auto& l) { return std::any_of(profile.regions.begin(), profile.regions.end(), [&](const auto& r) { return r.id == l.sourceRegionId; }); });
        auto* item = new QListWidgetItem(qs(scene.name) + QString(" · r%1").arg(scene.revision) + (compatible ? "" : aitsnap::uiText(" · 源待匹配")), scenes_);
        item->setData(Qt::UserRole, qs(scene.id)); item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(settings_.selectedSceneIds.contains(qs(scene.id)) ? Qt::Checked : Qt::Unchecked);
        if (qs(scene.id) == current) scenes_->setCurrentItem(item);
    }
    if (!scenes_->currentItem() && scenes_->count()) scenes_->setCurrentRow(0);
}
void MainWindow::rebuildSceneMenu()
{
    if (!sceneMenu_) return; sceneMenu_->clear();
    if (settings_.scenes.empty()) { auto* empty = sceneMenu_->addAction(aitsnap::uiText("尚无场景，请先新建拼图")); empty->setEnabled(false); return; }
    for (const auto& scene : settings_.scenes) {
        const auto sceneId = qs(scene.id); auto* action = sceneMenu_->addAction(qs(scene.name)); action->setCheckable(true); action->setChecked(settings_.selectedSceneIds.contains(sceneId));
        connect(action, &QAction::toggled, this, [this, sceneId](bool checked) {
            auto candidate = settings_;
            if (checked && !candidate.selectedSceneIds.contains(sceneId)) candidate.selectedSceneIds.append(sceneId);
            if (!checked) candidate.selectedSceneIds.removeAll(sceneId);
            candidate.outputMode = candidate.selectedSceneIds.empty() ? "originals" : "both";
            auto& profile = candidate.profiles[std::size_t(candidate.activeProfile)]; profile.sceneIds.clear();
            for (const auto& id : candidate.selectedSceneIds) profile.sceneIds.push_back(id.toStdString()); profile.outputMode = candidate.outputMode.toStdString();
            QString error;
            if (saveSettings(configurationPath_, candidate, error)) { settings_ = candidate; loadProfile(); setStatus(candidate.selectedSceneIds.empty() ? aitsnap::uiText("未启用拼图，当前仅保存原图。") : aitsnap::uiText("已更新当前配置的拼图输出。")); }
            else setStatus(error, true);
        });
    }
}
void MainWindow::editScene(bool createNew, bool historical, const QString& manifestOverride)
{
    if (busy_ || editingRegions_ || editingScene_) { setStatus(aitsnap::uiText("请等待当前截图或编辑完成。"), true); return; }
    if (!applySettings()) return;
    auto profile = settings_.profiles[std::size_t(settings_.activeProfile)]; SceneSources sources;
    if (historical) {
        const auto manifest = manifestOverride.isEmpty() ? QFileDialog::getOpenFileName(this, aitsnap::uiText("打开已保存批次的 batch.json"), settings_.outputDirectory, "Batch (batch.json);;JSON (*.json)") : manifestOverride;
        if (manifest.isEmpty()) return; QString error;
        if (!loadHistoricalSources(manifest, sources, profile, error)) { setStatus(error, true); return; }
    } else if(livePreviewEnabled_->isChecked() && livePreview_->snapshot().profile.id==profile.id)sources=livePreview_->snapshot().sources;
    else if(!livePreviewEnabled_->isChecked() && lastOutcome_ && lastOutcome_->batch.request.profile.id == profile.id) sources = batchSources(lastOutcome_->batch);
    core::SceneTemplate model; bool existing = false;
    if (!createNew && scenes_->currentItem()) {
        const auto chosen = scenes_->currentItem()->data(Qt::UserRole).toString().toStdString();
        const auto found = std::find_if(settings_.scenes.begin(), settings_.scenes.end(), [&](const auto& scene) { return scene.id == chosen; });
        if (found != settings_.scenes.end()) { model = *found; existing = true; }
    }
    if (historical && existing && !std::all_of(model.layers.begin(), model.layers.end(), [&](const auto& layer) { return sources.count(layer.sourceRegionId) > 0; })) existing = false;
    if (!existing) model = core::presetScene(profile, id(), (historical ? uiText("历史重排") : uiText("拼图 %1").arg(settings_.scenes.size() + 1)).toStdString(), 1600, 900, core::LayoutPreset::Grid);
    editingScene_ = true;
    SceneEditor dialog(model, profile, sources, this);
    if(!historical && livePreviewEnabled_->isChecked())liveEditor_=&dialog;
    refreshWorkspace();const auto accepted = dialog.exec() == QDialog::Accepted;liveEditor_=nullptr; editingScene_ = false;refreshWorkspace();
    if (!accepted) { setStatus(aitsnap::uiText("已取消场景编辑，截图已恢复。")); return; }
    const auto saved = dialog.sceneTemplate(); auto candidate = settings_;
    const auto found = std::find_if(candidate.scenes.begin(), candidate.scenes.end(), [&](const auto& scene) { return scene.id == saved.id; });
    if (found == candidate.scenes.end()) candidate.scenes.push_back(saved); else *found = saved;
    auto& currentProfile = candidate.profiles[std::size_t(candidate.activeProfile)];
    const bool compatible = std::all_of(saved.layers.begin(), saved.layers.end(), [&](const auto& layer) { return std::any_of(currentProfile.regions.begin(), currentProfile.regions.end(), [&](const auto& region) { return region.id == layer.sourceRegionId; }); });
    if (compatible) {
        if (!candidate.selectedSceneIds.contains(qs(saved.id))) candidate.selectedSceneIds.append(qs(saved.id));
        candidate.outputMode = "both"; candidate.clipboardRegion = "scene:" + qs(saved.id); currentProfile.sceneIds.clear();
        for (const auto& scene : candidate.selectedSceneIds) currentProfile.sceneIds.push_back(scene.toStdString());
        currentProfile.outputMode = "both"; currentProfile.clipboardSelection = candidate.clipboardRegion.toStdString();
    }
    QString error;
    if (!saveSettings(configurationPath_, candidate, error)) { setStatus(error, true); return; }
    settings_ = candidate; loadProfile();
    setStatus(compatible ? aitsnap::uiText("场景已保存并启用；后续截图保存原图和拼图，复制所选拼图。") : aitsnap::uiText("历史场景已保存；源与当前配置不同，未自动启用。可在编辑器导出派生图。"));
}
void MainWindow::importScene()
{
    if (busy_ || editingRegions_ || editingScene_ || !applySettings()) return;
    const auto path = QFileDialog::getOpenFileName(this, aitsnap::uiText("导入独立场景模板"), {}, "Scene (*.json)"); if (path.isEmpty()) return;
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) { setStatus(file.errorString(), true); return; }
    if (file.size() > 8 * 1024 * 1024) { setStatus(aitsnap::uiText("场景文件超过 8 MiB。"), true); return; }
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(file.readAll(), &parse); QString error; core::SceneTemplate scene;
    if (parse.error || !document.isObject() || !sceneFromJson(document.object(), scene, error)) { setStatus(error.isEmpty() ? aitsnap::uiText("场景 JSON 无效，原文件未改动。") : error, true); return; }
    auto candidate = settings_;
    if (std::any_of(candidate.scenes.begin(), candidate.scenes.end(), [&](const auto& s) { return s.id == scene.id; })) { scene.id = id(); scene.revision = 1; scene.name += "（导入副本）"; }
    candidate.scenes.push_back(scene);
    if (!saveSettings(configurationPath_, candidate, error)) { setStatus(error, true); return; }
    settings_ = candidate; loadProfile(); setStatus(aitsnap::uiText("场景已导入。请编辑确认源映射，再勾选输出并应用配置。"));
}
void MainWindow::exportScene()
{
    if (!scenes_->currentItem()) { setStatus(aitsnap::uiText("请先选择一个场景。"), true); return; }
    const auto chosen = scenes_->currentItem()->data(Qt::UserRole).toString().toStdString();
    const auto found = std::find_if(settings_.scenes.begin(), settings_.scenes.end(), [&](const auto& scene) { return scene.id == chosen; }); if (found == settings_.scenes.end()) return;
    const auto path = QFileDialog::getSaveFileName(this, aitsnap::uiText("导出为新的场景文件（不覆盖）"), {}, "Scene (*.json)", nullptr, QFileDialog::DontConfirmOverwrite); if (path.isEmpty()) return;
    QString error; if (saveBytesNew(QJsonDocument(sceneJson(*found)).toJson(), path, error)) setStatus(aitsnap::uiText("独立场景模板已导出。")); else setStatus(error, true);
}
void MainWindow::editRegions()
{
    if (busy_ || editingRegions_ || editingScene_) { setStatus(aitsnap::uiText("请等待当前采集或编辑完成。"), true); return; }
    if (!applySettings()) return;
    const bool restore = isVisible(); hide(); editingRegions_ = true;
    if(livePreview_)livePreview_->setActive(false);
    // Let the compositor remove the host before taking editor-only background frames.
    QTimer::singleShot(180, this, [this, restore] {
        try {
            openRegionEditor(this, settings_.profiles[std::size_t(settings_.activeProfile)], [this, restore](bool accepted, core::CaptureProfile profile) {
                if (accepted) {
                    auto candidate = settings_;
                    const auto target = std::find_if(candidate.profiles.begin(), candidate.profiles.end(), [&](const auto& p) { return p.id == profile.id; });
                    if (target == candidate.profiles.end()) { editingRegions_ = false; setStatus(aitsnap::uiText("编辑配置已不存在，未改写其他配置。"), true); if (restore) showNormal(); return; }
                    *target = std::move(profile); QString error;
                    if (saveSettings(configurationPath_, candidate, error)) { settings_ = candidate; loadProfile(); setStatus(aitsnap::uiText("区域已保存，截图已恢复。")); }
                    else setStatus(error, true);
                } else setStatus(aitsnap::uiText("已取消框选，区域未改动。"));
                editingRegions_ = false; if (restore) { showNormal(); raise(); activateWindow(); }
            }, regions_->currentRow());
        } catch (const std::exception& e) { editingRegions_ = false; setStatus(QString::fromUtf8(e.what()), true); if (restore) showNormal(); }
    });
}

} // namespace aitsnap
