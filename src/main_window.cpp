// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors
#define NOMINMAX
#include <windows.h>
#include "main_window.h"
#include "adapters/win_capture.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRunnable>
#include <QSplitter>
#include <QSystemTrayIcon>
#include <QTableWidget>
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
    QPixmap pixmap(64, 64); pixmap.fill(Qt::transparent); QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(QColor("#1967D2"));
    p.drawRoundedRect(2, 2, 60, 60, 13, 13); p.setPen(QPen(Qt::white, 4)); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(14, 18, 36, 28, 4, 4); p.drawEllipse(25, 25, 14, 14); return QIcon(pixmap);
}
MainWindow::MainWindow(QWidget* parent, QString path) : QMainWindow(parent), configurationPath_(path.isEmpty() ? settingsPath() : path)
{
    setWindowTitle(QStringLiteral("AITSnap · 多屏截图")); resize(1120, 780); setWindowIcon(appIcon());
    settings_ = defaultSettings(); QString loadError;
    if (QFile::exists(configurationPath_)) loadSettings(configurationPath_, settings_, loadError);
    if (settings_.profiles.empty()) {
        WinCaptureBackend backend; core::CaptureProfile p; p.id = id(); p.name = "默认多屏";
        try { p.calibratedDisplays = backend.displays(); } catch (const std::exception& e) { loadError = QString::fromUtf8(e.what()); }
        for (int i = 0; i < 5; ++i) {
            const auto b = p.calibratedDisplays.empty() ? core::Rect{0, 0, 1920, 1080} : p.calibratedDisplays[std::size_t(i) % p.calibratedDisplays.size()].rect;
            p.regions.push_back({id(), "区域 " + std::to_string(i + 1), {b.x + 60 + i * 20, b.y + 60 + i * 20, std::max(1, std::min(640, b.width / 2)), std::max(1, std::min(400, b.height / 2))}, true});
        }
        settings_.profiles.push_back(p);
    }
    configurationBlocked_ = !loadError.isEmpty();
    buildUi(); setupTray(); loadProfile(); qApp->installNativeEventFilter(&hotkeys_);
    hotkeys_.triggered = [this](int mode) { triggerCapture(mode); };
    hotkeys_.taskbarCreated = [this] { QTimer::singleShot(250, this, [this] { tray_->hide(); tray_->show(); }); };
    QString e;
    if (!hotkeys_.configure({settings_.saveHotkey, settings_.copyHotkey, settings_.bothHotkey}, e)) setStatus(e, true);
    else setStatus(QStringLiteral("就绪 · %1 保存，%2 复制，%3 保存并复制").arg(settings_.saveHotkey, settings_.copyHotkey, settings_.bothHotkey));
    if (!loadError.isEmpty()) setStatus(QStringLiteral("配置未载入，原文件保留：") + loadError + QStringLiteral("。核对后应用配置。"), true);
}
MainWindow::~MainWindow() { QThreadPool::globalInstance()->waitForDone(); }
void MainWindow::buildUi()
{
    auto* central = new QWidget(this); auto* outer = new QVBoxLayout(central); outer->setContentsMargins(24, 20, 24, 16); outer->setSpacing(14);
    auto* title = new QLabel(QStringLiteral("AITSnap  多屏证据截图")); title->setStyleSheet("font-size:24px; font-weight:600;"); outer->addWidget(title);
    outer->addWidget(new QLabel(QStringLiteral("选定区域 → 后台一键截图 → 保存原图 / 复制单图。关闭窗口后继续驻留托盘。")));
    auto* row = new QHBoxLayout; profiles_ = new QComboBox; profiles_->setObjectName("profiles");
    for (const auto& p : settings_.profiles) profiles_->addItem(qs(p.name));
    profiles_->setCurrentIndex(settings_.activeProfile); row->addWidget(new QLabel(QStringLiteral("截图配置"))); row->addWidget(profiles_, 1);
    auto* addProfile = new QPushButton(QStringLiteral("新建配置")); row->addWidget(addProfile);
    auto* apply = new QPushButton(QStringLiteral("应用配置")); apply->setObjectName("applySettings"); row->addWidget(apply); outer->addLayout(row);
    auto* split = new QSplitter(Qt::Horizontal); outer->addWidget(split, 1);
    auto* left = new QWidget; auto* l = new QVBoxLayout(left); l->setContentsMargins(0, 0, 10, 0);
    regions_ = new QTableWidget(0, 6); regions_->setObjectName("regions");
    regions_->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("名称"), "X", "Y", QStringLiteral("宽 px"), QStringLiteral("高 px")});
    regions_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); regions_->setSelectionBehavior(QAbstractItemView::SelectRows);
    l->addWidget(new QLabel(QStringLiteral("源区域 · 原生桌面物理像素（可为负坐标）"))); l->addWidget(regions_, 1);
    auto* buttons = new QHBoxLayout; auto* add = new QPushButton(QStringLiteral("添加区域")); auto* remove = new QPushButton(QStringLiteral("删除选中")); auto* calibrate = new QPushButton(QStringLiteral("确认当前屏幕布局"));
    buttons->addWidget(add); buttons->addWidget(remove); buttons->addWidget(calibrate); l->addLayout(buttons); split->addWidget(left);
    auto* right = new QWidget; auto* form = new QFormLayout(right); form->setContentsMargins(10, 0, 0, 0);
    profileName_ = new QLineEdit; form->addRow(QStringLiteral("配置名称"), profileName_);
    directory_ = new QLineEdit(settings_.outputDirectory); directory_->setObjectName("outputDirectory");
    auto* pathRow = new QWidget; auto* paths = new QHBoxLayout(pathRow); paths->setContentsMargins(0, 0, 0, 0); paths->addWidget(directory_);
    auto* browse = new QPushButton("…"); browse->setMaximumWidth(32); paths->addWidget(browse); form->addRow(QStringLiteral("保存目录"), pathRow);
    format_ = new QComboBox; format_->addItems({"PNG", "JPEG", "BMP"}); format_->setCurrentText(settings_.format); form->addRow(QStringLiteral("图片格式"), format_);
    filename_ = new QLineEdit(settings_.filenameTemplate); filename_->setToolTip("{datetime} {sequence} {region} {profile} {test} {device} {station} {operator} {run} {step} {command}"); form->addRow(QStringLiteral("文件命名"), filename_);
    clipboard_ = new QComboBox; form->addRow(QStringLiteral("复制哪张图"), clipboard_);
    saveKey_ = new QLineEdit(settings_.saveHotkey); copyKey_ = new QLineEdit(settings_.copyHotkey); bothKey_ = new QLineEdit(settings_.bothHotkey);
    form->addRow(QStringLiteral("保存热键"), saveKey_); form->addRow(QStringLiteral("复制热键"), copyKey_); form->addRow(QStringLiteral("保存并复制"), bothKey_);
    testId_ = new QLineEdit(settings_.testId); device_ = new QLineEdit(settings_.device); station_ = new QLineEdit(settings_.station); operator_ = new QLineEdit(settings_.operatorName);
    form->addRow(QStringLiteral("试验标识"), testId_); form->addRow(QStringLiteral("设备标识"), device_); form->addRow(QStringLiteral("工位"), station_); form->addRow(QStringLiteral("操作员"), operator_);
    startTray_ = new QCheckBox(QStringLiteral("启动时直接进入托盘")); startTray_->setChecked(settings_.startInTray); form->addRow(startTray_);
    preview_ = new QLabel(QStringLiteral("最近一次截图预览")); preview_->setAlignment(Qt::AlignCenter); preview_->setMinimumSize(240, 130); preview_->setStyleSheet("background:#eef2f7; color:#3f4f65; border:1px solid #d8e0ea; border-radius:6px;"); form->addRow(preview_);
    split->addWidget(right); split->setSizes({650, 400});
    auto* actions = new QHBoxLayout; auto* save = new QPushButton(QStringLiteral("截图并保存")); auto* copy = new QPushButton(QStringLiteral("复制选中原图")); auto* both = new QPushButton(QStringLiteral("保存并复制")); auto* folder = new QPushButton(QStringLiteral("打开输出目录"));
    save->setObjectName("captureSave"); save->setStyleSheet("background:#1967d2; color:white; padding:9px 18px; border-radius:5px;");
    actions->addWidget(save); actions->addWidget(copy); actions->addWidget(both); actions->addStretch(); actions->addWidget(folder); outer->addLayout(actions);
    status_ = new QLabel; status_->setObjectName("captureStatus"); status_->setWordWrap(true); status_->setTextInteractionFlags(Qt::TextSelectableByMouse); outer->addWidget(status_); setCentralWidget(central);
    connect(apply, &QPushButton::clicked, this, [this] { applySettings(); });
    connect(add, &QPushButton::clicked, this, [this] { addRegion(); });
    connect(remove, &QPushButton::clicked, this, [this] { if (regions_->currentRow() >= 0) regions_->removeRow(regions_->currentRow()); });
    connect(browse, &QPushButton::clicked, this, [this] { auto p = QFileDialog::getExistingDirectory(this, QStringLiteral("选择截图保存目录"), directory_->text()); if (!p.isEmpty()) directory_->setText(p); });
    connect(folder, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(directory_->text())); });
    connect(save, &QPushButton::clicked, this, [this] { if (applySettings()) triggerCapture(0); });
    connect(copy, &QPushButton::clicked, this, [this] { if (applySettings()) triggerCapture(1); });
    connect(both, &QPushButton::clicked, this, [this] { if (applySettings()) triggerCapture(2); });
    connect(profiles_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || index >= int(settings_.profiles.size())) return;
        settings_.activeProfile = index; loadProfile(); QString e;
        if (!configurationBlocked_ && !saveSettings(configurationPath_, settings_, e)) setStatus(e, true);
        rebuildProfileMenu();
    });
    connect(addProfile, &QPushButton::clicked, this, [this] {
        bool ok{}; auto name = QInputDialog::getText(this, QStringLiteral("新建截图配置"), QStringLiteral("名称"), QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        auto p = settings_.profiles[std::size_t(settings_.activeProfile)]; p.id = id(); p.name = name.trimmed().toStdString(); p.revision = 1;
        settings_.profiles.push_back(p); profiles_->addItem(name); profiles_->setCurrentIndex(int(settings_.profiles.size()) - 1);
    });
    connect(calibrate, &QPushButton::clicked, this, [this] {
        Settings candidate = settings_; QString e;
        if (!collectSettings(candidate, e)) { setStatus(e, true); return; }
        try { WinCaptureBackend b; candidate.profiles[std::size_t(candidate.activeProfile)].calibratedDisplays = b.displays(); }
        catch (const std::exception& x) { setStatus(QString::fromUtf8(x.what()), true); return; }
        if (configurationBlocked_) { setStatus(QStringLiteral("原配置无法读取。请先核对并应用配置。"), true); return; }
        if (saveSettings(configurationPath_, candidate, e)) { settings_ = candidate; setStatus(QStringLiteral("已确认显示器布局。请核对物理区域后截图。")); } else setStatus(e, true);
    });
}
void MainWindow::setupTray()
{
    tray_ = new QSystemTrayIcon(windowIcon(), this); auto* menu = new QMenu(this);
    auto* show = menu->addAction(QStringLiteral("打开区域与输出设置")); connect(show, &QAction::triggered, this, [this] { showNormal(); raise(); activateWindow(); });
    const QStringList labels{QStringLiteral("截图并保存"), QStringLiteral("复制选中原图"), QStringLiteral("保存并复制")};
    for (int i = 0; i < 3; ++i) { auto* action = menu->addAction(labels[i]); connect(action, &QAction::triggered, this, [this, i] { QTimer::singleShot(180, this, [this, i] { triggerCapture(i); }); }); }
    profileMenu_ = menu->addMenu(QStringLiteral("切换截图配置")); rebuildProfileMenu();
    auto* pause = menu->addAction(QStringLiteral("暂停截图热键")); pause->setCheckable(true);
    connect(pause, &QAction::toggled, this, [this, pause](bool checked) {
        QString e; if (hotkeys_.pause(checked, e)) { paused_ = checked; setStatus(checked ? QStringLiteral("截图已暂停") : QStringLiteral("截图已恢复")); }
        else { pause->blockSignals(true); pause->setChecked(true); pause->blockSignals(false); paused_ = true; setStatus(e, true); }
    });
    auto* folder = menu->addAction(QStringLiteral("打开输出目录")); connect(folder, &QAction::triggered, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(settings_.outputDirectory)); });
    menu->addSeparator(); auto* quit = menu->addAction(QStringLiteral("退出 AITSnap")); connect(quit, &QAction::triggered, qApp, &QApplication::quit);
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
    const auto& p = settings_.profiles[std::size_t(settings_.activeProfile)]; profileName_->setText(qs(p.name)); regions_->setRowCount(0); clipboard_->clear();
    for (const auto& r : p.regions) {
        int row = regions_->rowCount(); regions_->insertRow(row);
        auto* enabled = new QTableWidgetItem; enabled->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked); regions_->setItem(row, 0, enabled);
        auto* name = new QTableWidgetItem(qs(r.name)); name->setData(Qt::UserRole, qs(r.id)); regions_->setItem(row, 1, name);
        const int values[]{r.rect.x, r.rect.y, r.rect.width, r.rect.height};
        for (int i = 0; i < 4; ++i) regions_->setItem(row, i + 2, new QTableWidgetItem(QString::number(values[i])));
        if (r.enabled) clipboard_->addItem(qs(r.name), qs(r.id));
    }
    int index = clipboard_->findData(settings_.clipboardRegion); if (index >= 0) clipboard_->setCurrentIndex(index);
    settings_.clipboardRegion = clipboard_->currentData().toString();
}
void MainWindow::addRegion()
{
    int row = regions_->rowCount(); regions_->insertRow(row);
    auto* enabled = new QTableWidgetItem; enabled->setCheckState(Qt::Checked); regions_->setItem(row, 0, enabled);
    auto* name = new QTableWidgetItem(QStringLiteral("区域 %1").arg(row + 1)); name->setData(Qt::UserRole, QString::fromStdString(id())); regions_->setItem(row, 1, name);
    const int values[]{0, 0, 640, 400}; for (int i = 0; i < 4; ++i) regions_->setItem(row, i + 2, new QTableWidgetItem(QString::number(values[i])));
}
bool MainWindow::collectSettings(Settings& c, QString& e)
{
    auto& p = c.profiles[std::size_t(c.activeProfile)]; p.name = profileName_->text().trimmed().toStdString(); p.regions.clear();
    if (p.name.empty()) { e = QStringLiteral("请输入配置名称"); return false; }
    for (int row = 0; row < regions_->rowCount(); ++row) {
        int v[4]{};
        for (int i = 0; i < 4; ++i) { bool ok{}; const auto* item = regions_->item(row, i + 2); v[i] = item ? item->text().toInt(&ok) : 0; if (!ok) { e = QStringLiteral("第 %1 行需要整数物理坐标和尺寸").arg(row + 1); return false; } }
        const auto* name = regions_->item(row, 1); p.regions.push_back({name->data(Qt::UserRole).toString().toStdString(), name->text().trimmed().toStdString(), {v[0], v[1], v[2], v[3]}, regions_->item(row, 0)->checkState() == Qt::Checked});
    }
    if (auto err = core::validateProfile(p); err) { e = qs(err.message); return false; }
    ++p.revision; c.outputDirectory = QDir(directory_->text().trimmed()).absolutePath(); c.format = format_->currentText(); c.filenameTemplate = filename_->text();
    c.saveHotkey = saveKey_->text(); c.copyHotkey = copyKey_->text(); c.bothHotkey = bothKey_->text(); c.clipboardRegion = clipboard_->currentData().toString();
    c.testId = testId_->text(); c.device = device_->text(); c.station = station_->text(); c.operatorName = operator_->text(); c.startInTray = startTray_->isChecked();
    core::CaptureRequest sample; sample.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); sample.profile = p;
    expandFilename(c, sample, p.regions.front(), e); return e.isEmpty();
}
bool MainWindow::applySettings()
{
    Settings c = settings_; QString e;
    if (configurationBlocked_ && QFile::exists(configurationPath_)) {
        const auto backup = configurationPath_ + ".preserved-" + QString::fromStdString(id());
        if (!QFile::copy(configurationPath_, backup)) { setStatus(QStringLiteral("无法保留旧配置，配置未改写。"), true); return false; }
    }
    if (!collectSettings(c, e) || !hotkeys_.configure({c.saveHotkey, c.copyHotkey, c.bothHotkey}, e)) { setStatus(e, true); return false; }
    if (paused_) hotkeys_.pause(true, e);
    if (!saveSettings(configurationPath_, c, e)) { QString ignored; hotkeys_.configure({settings_.saveHotkey, settings_.copyHotkey, settings_.bothHotkey}, ignored); if (paused_) hotkeys_.pause(true, ignored); setStatus(e, true); return false; }
    settings_ = c; configurationBlocked_ = false; profiles_->setItemText(settings_.activeProfile, qs(c.profiles[std::size_t(c.activeProfile)].name)); rebuildProfileMenu(); loadProfile(); setStatus(QStringLiteral("配置已保存 · 热键已应用")); return true;
}
void MainWindow::setStatus(const QString& text, bool error)
{
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    status_->setText(text); status_->setStyleSheet(error ? (dark ? "color:#ff9a91;" : "color:#b42318;") : (dark ? "color:#96dfb5;" : "color:#275b43;"));
    if (tray_) tray_->setToolTip("AITSnap · " + text.left(100));
}
void MainWindow::triggerCapture(int mode)
{
    if (configurationBlocked_) { setStatus(QStringLiteral("原配置无法读取。核对并应用配置后才能截图。"), true); return; }
    if (paused_) { setStatus(QStringLiteral("截图已暂停，请从托盘恢复"), true); return; }
    if (busy_) { setStatus(QStringLiteral("上一批截图尚未完成，本次请求未接受"), true); return; }
    QString e; Settings snapshot = settings_; ++snapshot.sequence;
    if (!saveSettings(configurationPath_, snapshot, e)) { setStatus(QStringLiteral("序号未持久化：") + e, true); return; }
    settings_.sequence = snapshot.sequence; core::CaptureRequest request;
    request.requestId = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz").toStdString() + "-" + id();
    request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); request.profile = snapshot.profiles[std::size_t(snapshot.activeProfile)];
    request.context.testId = snapshot.testId.toStdString(); request.context.deviceId = snapshot.device.toStdString();
    request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString();
    busy_ = true; setStatus(QStringLiteral("正在后台采集 %1 个源区域…").arg(request.profile.regions.size())); QPointer<MainWindow> self(this);
    QThreadPool::globalInstance()->start(QRunnable::create([self, snapshot, request, mode] {
        WinCaptureBackend backend; auto result = std::make_shared<CaptureOutcome>(captureToFiles(backend, snapshot, request, mode != 1));
        if (self) QMetaObject::invokeMethod(self, [self, result, snapshot, mode] { if (self) self->completeCapture(result, snapshot, mode); }, Qt::QueuedConnection);
    }));
}
void MainWindow::completeCapture(const std::shared_ptr<CaptureOutcome>& result, const Settings& snapshot, int mode)
{
    if (mode != 0) {
        const auto& list = result->batch.captures;
        const auto it = std::find_if(list.begin(), list.end(), [&](const auto& c) { return snapshot.clipboardRegion.isEmpty() ? c.image.valid() : qs(c.region.id) == snapshot.clipboardRegion; });
        core::OutputResult output{"clipboard", "", "FAILED", {"CLIPBOARD_SOURCE", "Selected region unavailable"}};
        if (it != list.end() && it->image.valid()) {
            auto image = toQImage(it->image); auto previous = GetClipboardSequenceNumber(); QApplication::clipboard()->setImage(image);
            if (GetClipboardSequenceNumber() != previous && QApplication::clipboard()->image() == image) { output.status = "COPIED"; output.error = {}; } else output.error = {"CLIPBOARD", "Clipboard readback failed"};
        }
        result->batch.outputs.push_back(output);
    }
    QString e; if (mode != 1 && !result->directory.isEmpty() && !writeBatchManifest(*result, e)) result->batch.outputs.push_back({"manifest", "", "FAILED", {"MANIFEST", e.toStdString()}});
    int captured = 0, saved = 0, failed = 0; QString detail;
    for (const auto& c : result->batch.captures) { if (c.error) { ++failed; detail = qs(c.error.code + ": " + c.error.message); } else ++captured; }
    for (const auto& o : result->batch.outputs) { if (o.status == "SAVED") ++saved; if (o.error) { ++failed; detail = qs(o.error.code + ": " + o.error.message); } }
    setStatus(QStringLiteral("已采集 %1 张 · 已保存 %2 张 · %3 ms%4").arg(captured).arg(saved).arg(result->elapsedMs, 0, 'f', 1).arg(failed ? " · " + detail : QStringLiteral(" · 完成")), failed > 0);
    for (const auto& c : result->batch.captures) if (c.image.valid()) { preview_->setPixmap(QPixmap::fromImage(toQImage(c.image)).scaled(320, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation)); break; }
    busy_ = false; if (captureFinished) captureFinished(result);
}
void MainWindow::closeEvent(QCloseEvent* e)
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) { hide(); e->ignore(); } else e->accept();
}

} // namespace aitsnap
