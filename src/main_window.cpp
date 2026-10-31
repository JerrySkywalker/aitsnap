// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors
#define NOMINMAX
#include <windows.h>
#include "main_window.h"
#include "adapters/win_capture.h"
#include "adapters/report_queue.h"
#include "adapters/context_store.h"
#include "adapters/local_host.h"
#include "desktop/region_editor.h"
#include "desktop/scene_editor.h"
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
#include <QPushButton>
#include <QRunnable>
#include <QSplitter>
#include <QScrollArea>
#include <QSystemTrayIcon>
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
    QPixmap pixmap(64, 64); pixmap.fill(Qt::transparent); QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(QColor("#1967D2"));
    p.drawRoundedRect(2, 2, 60, 60, 13, 13); p.setPen(QPen(Qt::white, 4)); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(14, 18, 36, 28, 4, 4); p.drawEllipse(25, 25, 14, 14); return QIcon(pixmap);
}
MainWindow::MainWindow(QWidget* parent, QString path, QString pipeName) : QMainWindow(parent), configurationPath_(path.isEmpty() ? settingsPath() : path)
{
    setWindowTitle(QStringLiteral("AITSnap · 多屏截图")); resize(1120, 780); setWindowIcon(appIcon());
    settings_ = defaultSettings(); QString loadError;
    settings_.outputDirectory = QFileInfo(configurationPath_).absolutePath() + "/captures";
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
    reportQueue_ = new ReportQueue(QFileInfo(configurationPath_).absolutePath() + "/report-jobs", QCoreApplication::applicationDirPath() + "/aitsnap-word-bridge.exe", this);
    reportQueue_->changed = [this] { refreshReportStatus(); }; refreshReportStatus();
    contextStore_ = std::make_unique<ContextStore>(QFileInfo(configurationPath_).absolutePath() + "/context");
    if (!contextStore_->diagnostic().isEmpty()) contextStatus_->setText(contextStore_->diagnostic());
    hotkeys_.triggered = [this](int mode) { triggerCapture(mode); };
    hotkeys_.taskbarCreated = [this] { QTimer::singleShot(250, this, [this] { tray_->hide(); tray_->show(); }); };
    QString e;
    if (!hotkeys_.configure({settings_.saveHotkey, settings_.copyHotkey, settings_.bothHotkey}, e)) setStatus(e, true);
    else setStatus(QStringLiteral("就绪 · %1 保存，%2 复制，%3 保存并复制").arg(settings_.saveHotkey, settings_.copyHotkey, settings_.bothHotkey));
    if (!loadError.isEmpty()) setStatus(QStringLiteral("配置未载入，原文件保留：") + loadError + QStringLiteral("。核对后应用配置。"), true);
    startHost(std::move(pipeName));
}
MainWindow::~MainWindow()
{
    localHost_.reset(); // stop admission before tearing down desktop state
    captureFinished = {};
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall); // finish saved-batch receipts before receiver destruction
    if (reportQueue_) { reportQueue_->changed = {}; delete reportQueue_; reportQueue_ = nullptr; }
}
void MainWindow::buildUi()
{
    auto* central = new QWidget(this); auto* outer = new QVBoxLayout(central); outer->setContentsMargins(24, 20, 24, 16); outer->setSpacing(14);
    auto* title = new QLabel(QStringLiteral("AITSnap  多屏证据截图")); title->setStyleSheet("font-size:24px; font-weight:600;"); outer->addWidget(title);
    outer->addWidget(new QLabel(QStringLiteral("区域与场景独立 · 修改后应用配置 · 后台一键保存/复制 · 关闭窗口后驻留托盘")));
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
    auto* visual = new QPushButton(QStringLiteral("屏幕框选 / 拖动")); visual->setObjectName("editRegions");
    auto* visualRow = new QHBoxLayout; visualRow->addWidget(visual); visualRow->addStretch(); l->addLayout(visualRow);
    buttons->addWidget(add); buttons->addWidget(remove); buttons->addWidget(calibrate); l->addLayout(buttons); split->addWidget(left);
    l->addWidget(new QLabel(QStringLiteral("拼图场景 · 可勾选多个，同一批源图分别渲染")));
    scenes_ = new QListWidget; scenes_->setObjectName("enabledScenes"); scenes_->setMaximumHeight(105); l->addWidget(scenes_);
    auto* sceneActions = new QHBoxLayout;
    auto* newScene = new QPushButton(QStringLiteral("新建拼图")); newScene->setObjectName("newScene");
    auto* editSceneButton = new QPushButton(QStringLiteral("编辑拼图")); editSceneButton->setObjectName("editScene");
    auto* history = new QPushButton(QStringLiteral("重排历史批次")); history->setObjectName("openHistoricalBatch");
    auto* import = new QPushButton(QStringLiteral("导入场景")); auto* exportTemplate = new QPushButton(QStringLiteral("导出场景"));
    sceneActions->addWidget(newScene); sceneActions->addWidget(editSceneButton); sceneActions->addWidget(history); sceneActions->addWidget(import); sceneActions->addWidget(exportTemplate); l->addLayout(sceneActions);
    outputMode_ = new QComboBox; outputMode_->setObjectName("outputMode");
    outputMode_->addItem(QStringLiteral("保存原图"), "originals"); outputMode_->addItem(QStringLiteral("保存拼图（源图存入证据子目录）"), "composites"); outputMode_->addItem(QStringLiteral("原图 + 选中拼图"), "both");
    l->addWidget(outputMode_);
    auto* contextButton = new QPushButton(QStringLiteral("静态目录 / 执行事件 / 选择截图上下文")); contextButton->setObjectName("openContextStore"); l->addWidget(contextButton);
    contextStatus_ = new QLabel(QStringLiteral("截图上下文：未关联。事件到达不会自动切换当前上下文。")); contextStatus_->setWordWrap(true); l->addWidget(contextStatus_);
    connect(contextButton, &QPushButton::clicked, this, [this] { showContextStore(); });
    auto* rules = new QPushButton(QStringLiteral("事件截图规则（默认无自动规则）")); l->addWidget(rules); connect(rules, &QPushButton::clicked, this, [this] { editEventRules(); });
    hostStatus_ = new QLabel; hostStatus_->setWordWrap(true); l->addWidget(hostStatus_);
    connect(newScene, &QPushButton::clicked, this, [this] { editScene(true); });
    connect(editSceneButton, &QPushButton::clicked, this, [this] { editScene(false); });
    connect(history, &QPushButton::clicked, this, [this] { editScene(false, true); });
    connect(import, &QPushButton::clicked, this, [this] { importScene(); }); connect(exportTemplate, &QPushButton::clicked, this, [this] { exportScene(); });
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
    reportEnabled_ = new QCheckBox(QStringLiteral("保存截图后自动追加 Word")); reportEnabled_->setChecked(settings_.report.enabled); form->addRow(reportEnabled_);
    auto* reportPathRow = new QWidget; auto* reportPaths = new QHBoxLayout(reportPathRow); reportPaths->setContentsMargins(0, 0, 0, 0);
    reportPath_ = new QLineEdit(settings_.report.documentPath); reportPath_->setObjectName("reportDocumentPath"); reportPaths->addWidget(reportPath_);
    auto* reportBrowse = new QPushButton("…"); reportBrowse->setMaximumWidth(32); reportPaths->addWidget(reportBrowse); form->addRow(QStringLiteral("Word 报告"), reportPathRow);
    connect(reportBrowse, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("选择已有报告，或输入新 DOCX 文件名"), reportPath_->text(), "Word (*.docx)", nullptr, QFileDialog::DontConfirmOverwrite); if (!path.isEmpty()) reportPath_->setText(path); });
    auto* reportPolicy = new QWidget; auto* policies = new QHBoxLayout(reportPolicy); policies->setContentsMargins(0, 0, 0, 0);
    reportMode_ = new QComboBox; reportMode_->addItem(QStringLiteral("原图与拼图"), "both"); reportMode_->addItem(QStringLiteral("仅原图"), "originals"); reportMode_->addItem(QStringLiteral("仅拼图"), "composites"); reportMode_->setCurrentIndex(std::max(0, reportMode_->findData(settings_.report.itemMode))); policies->addWidget(reportMode_);
    reportLayout_ = new QComboBox; reportLayout_->addItem(QStringLiteral("顺序图文"), "inline"); reportLayout_->addItem(QStringLiteral("两列表格"), "table"); reportLayout_->setCurrentIndex(std::max(0, reportLayout_->findData(settings_.report.layout))); policies->addWidget(reportLayout_); form->addRow(reportPolicy);
    auto* queueButton = new QPushButton(QStringLiteral("查看 Word 任务 / 恢复核对")); form->addRow(queueButton); connect(queueButton, &QPushButton::clicked, this, [this] { showReportQueue(); });
    auto* appendLast = new QPushButton(QStringLiteral("将最近保存批次加入报告")); form->addRow(appendLast);
    connect(appendLast, &QPushButton::clicked, this, [this] { if (!lastOutcome_ || lastOutcome_->directory.isEmpty()) { setStatus(QStringLiteral("请先保存一批截图。"), true); return; } if (!applySettings()) return; QString error; if (!reportQueue_->enqueue(*lastOutcome_, settings_.report, error)) setStatus(error, true); else setStatus(QStringLiteral("报告任务已排队，截图可继续。")); });
    reportStatus_ = new QLabel; reportStatus_->setWordWrap(true); form->addRow(reportStatus_);
    preview_ = new QLabel(QStringLiteral("最近一次截图预览")); preview_->setAlignment(Qt::AlignCenter); preview_->setMinimumSize(240, 130); preview_->setStyleSheet("background:#eef2f7; color:#3f4f65; border:1px solid #d8e0ea; border-radius:6px;"); form->addRow(preview_);
    auto* rightScroll = new QScrollArea; rightScroll->setWidgetResizable(true); rightScroll->setWidget(right); split->addWidget(rightScroll); split->setSizes({650, 400});
    auto* actions = new QHBoxLayout; auto* save = new QPushButton(QStringLiteral("截图并保存")); auto* copy = new QPushButton(QStringLiteral("复制选中图片")); auto* both = new QPushButton(QStringLiteral("保存并复制")); auto* folder = new QPushButton(QStringLiteral("打开输出目录"));
    save->setObjectName("captureSave"); save->setStyleSheet("background:#1967d2; color:white; padding:9px 18px; border-radius:5px;");
    actions->addWidget(save); actions->addWidget(copy); actions->addWidget(both); actions->addStretch(); actions->addWidget(folder); outer->addLayout(actions);
    status_ = new QLabel; status_->setObjectName("captureStatus"); status_->setWordWrap(true); status_->setTextInteractionFlags(Qt::TextSelectableByMouse); outer->addWidget(status_); setCentralWidget(central);
    connect(apply, &QPushButton::clicked, this, [this] { applySettings(); });
    connect(add, &QPushButton::clicked, this, [this] { addRegion(); });
    connect(visual, &QPushButton::clicked, this, [this] { editRegions(); });
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
    auto* edit = menu->addAction(QStringLiteral("屏幕框选 / 编辑区域")); connect(edit, &QAction::triggered, this, [this] { QTimer::singleShot(180, this, [this] { editRegions(); }); });
    const QStringList labels{QStringLiteral("截图并保存"), QStringLiteral("复制选中图片"), QStringLiteral("保存并复制")};
    for (int i = 0; i < 3; ++i) { auto* action = menu->addAction(labels[i]); connect(action, &QAction::triggered, this, [this, i] { QTimer::singleShot(180, this, [this, i] { triggerCapture(i); }); }); }
    profileMenu_ = menu->addMenu(QStringLiteral("切换截图配置")); rebuildProfileMenu();
    sceneMenu_ = menu->addMenu(QStringLiteral("拼图场景 / 启用切换")); rebuildSceneMenu();
    auto* sceneEdit = menu->addAction(QStringLiteral("打开自由拼图")); connect(sceneEdit, &QAction::triggered, this, [this] { QTimer::singleShot(180, this, [this] { editScene(false); }); });
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
    settings_.selectedSceneIds.clear(); for (const auto& id : p.sceneIds) settings_.selectedSceneIds.append(qs(id)); settings_.outputMode = qs(p.outputMode);
    const auto wantedClipboard = p.clipboardSelection.empty() ? settings_.clipboardRegion : qs(p.clipboardSelection);
    for (const auto& r : p.regions) {
        int row = regions_->rowCount(); regions_->insertRow(row);
        auto* enabled = new QTableWidgetItem; enabled->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked); regions_->setItem(row, 0, enabled);
        auto* name = new QTableWidgetItem(qs(r.name)); name->setData(Qt::UserRole, qs(r.id)); regions_->setItem(row, 1, name);
        const int values[]{r.rect.x, r.rect.y, r.rect.width, r.rect.height};
        for (int i = 0; i < 4; ++i) regions_->setItem(row, i + 2, new QTableWidgetItem(QString::number(values[i])));
        if (r.enabled) clipboard_->addItem(qs(r.name), qs(r.id));
    }
    for (const auto& scene : settings_.scenes) clipboard_->addItem(QStringLiteral("拼图：") + qs(scene.name), "scene:" + qs(scene.id));
    int index = clipboard_->findData(wantedClipboard); if (index >= 0) clipboard_->setCurrentIndex(index);
    settings_.clipboardRegion = clipboard_->currentData().toString();
    outputMode_->setCurrentIndex(std::max(0, outputMode_->findData(settings_.outputMode))); rebuildScenes(); rebuildSceneMenu();
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
    c.outputMode = outputMode_->currentData().toString(); c.selectedSceneIds.clear();
    for (int i = 0; i < scenes_->count(); ++i) if (scenes_->item(i)->checkState() == Qt::Checked) c.selectedSceneIds.append(scenes_->item(i)->data(Qt::UserRole).toString());
    p.outputMode = c.outputMode.toStdString(); p.sceneIds.clear(); for (const auto& id : c.selectedSceneIds) p.sceneIds.push_back(id.toStdString()); p.clipboardSelection = c.clipboardRegion.toStdString();
    c.report.enabled = reportEnabled_->isChecked(); c.report.documentPath = reportPath_->text().trimmed(); c.report.itemMode = reportMode_->currentData().toString(); c.report.layout = reportLayout_->currentData().toString();
    if (c.report.enabled && (!QDir::isAbsolutePath(c.report.documentPath) || QFileInfo(c.report.documentPath).suffix().compare("docx", Qt::CaseInsensitive))) { e = QStringLiteral("自动报告需要明确的 DOCX 完整路径。"); return false; }
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
    startCapture(mode);
}
QString MainWindow::startCapture(int mode, const QJsonObject& options, const QJsonObject& eventSnapshot)
{
    if (editingScene_) { setStatus(QStringLiteral("拼图编辑中，截图热键暂停；保存或取消后恢复。"), true); return {}; }
    if (editingRegions_) { setStatus(QStringLiteral("区域编辑中，截图未接受；请完成或取消框选。"), true); return {}; }
    if (configurationBlocked_) { setStatus(QStringLiteral("原配置无法读取。核对并应用配置后才能截图。"), true); return {}; }
    if (paused_) { setStatus(QStringLiteral("截图已暂停，请从托盘恢复"), true); return {}; }
    if (busy_) { setStatus(QStringLiteral("上一批截图尚未完成，本次请求未接受"), true); return {}; }
    QString e; Settings snapshot = settings_; ++snapshot.sequence;
    if (options.contains("profileId")) {
        const auto chosen = options["profileId"].toString().toStdString(); const auto found = std::find_if(snapshot.profiles.begin(), snapshot.profiles.end(), [&](const auto& p) { return p.id == chosen; });
        if (found == snapshot.profiles.end()) return {}; snapshot.activeProfile = int(found - snapshot.profiles.begin()); snapshot.outputMode = qs(found->outputMode); snapshot.clipboardRegion = qs(found->clipboardSelection); snapshot.selectedSceneIds.clear(); for (const auto& scene : found->sceneIds) snapshot.selectedSceneIds.append(qs(scene));
    }
    if (options.contains("sceneIds")) { snapshot.selectedSceneIds.clear(); for (const auto& scene : options["sceneIds"].toArray()) snapshot.selectedSceneIds.append(scene.toString()); snapshot.outputMode = snapshot.selectedSceneIds.empty() ? "originals" : "both"; if (!snapshot.selectedSceneIds.empty()) snapshot.clipboardRegion = "scene:" + snapshot.selectedSceneIds.front(); else snapshot.clipboardRegion.clear(); }
    auto persistent = settings_; persistent.sequence = snapshot.sequence;
    if (!saveSettings(configurationPath_, persistent, e)) { setStatus(QStringLiteral("序号未持久化：") + e, true); return {}; }
    settings_.sequence = snapshot.sequence; core::CaptureRequest request;
    request.requestId = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz").toStdString() + "-" + id();
    request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); request.profile = snapshot.profiles[std::size_t(snapshot.activeProfile)];
    request.context.testId = snapshot.testId.toStdString(); request.context.deviceId = snapshot.device.toStdString();
    request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString();
    if (!selectedContextSnapshot_.isEmpty()) { request.context = selectedContext_; request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString(); request.contextSnapshotJson = QJsonDocument(selectedContextSnapshot_).toJson(QJsonDocument::Compact).toStdString(); }
    if (!eventSnapshot.isEmpty()) { const auto event = eventSnapshot["event"].toObject(); request.context = contextStore_->context(event["sourceSystem"].toString(), event["eventId"].toString()); request.context.stationId = snapshot.station.toStdString(); request.context.operatorId = snapshot.operatorName.toStdString(); request.contextSnapshotJson = QJsonDocument(eventSnapshot).toJson(QJsonDocument::Compact).toStdString(); }
    currentRequestId_ = qs(request.requestId);
    busy_ = true; setStatus(QStringLiteral("正在后台采集 %1 个源区域…").arg(request.profile.regions.size())); QPointer<MainWindow> self(this);
    QThreadPool::globalInstance()->start(QRunnable::create([self, snapshot, request, mode] {
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
    setStatus(QStringLiteral("已采集 %1 张 · 已保存 %2 张 · %3 ms%4").arg(captured).arg(saved).arg(result->elapsedMs, 0, 'f', 1).arg(failed ? " · " + detail : QStringLiteral(" · 完成")), failed > 0);
    for (const auto& c : result->batch.captures) if (c.image.valid()) { preview_->setPixmap(QPixmap::fromImage(toQImage(c.image)).scaled(320, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation)); break; }
    busy_ = false; lastOutcome_ = result; if (captureFinished) captureFinished(result);
    if (mode != 1 && snapshot.report.enabled && !result->directory.isEmpty()) {
        QString error; if (!reportQueue_->enqueue(*result, snapshot.report, error)) { reportStatus_->setText(QStringLiteral("截图已完成，报告未入队：") + error); }
    }
}
void MainWindow::closeEvent(QCloseEvent* e)
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) { hide(); e->ignore(); } else e->accept();
}
void MainWindow::refreshReportStatus()
{
    if (!reportQueue_ || !reportStatus_) return; int committed = 0, pending = 0, uncertain = 0, failed = 0;
    for (const auto& job : reportQueue_->jobs()) { const auto state = job["state"].toString(); if (state == "COMMITTED") ++committed; else if (state == "UNCERTAIN") ++uncertain; else if (state == "FAILED") ++failed; else ++pending; }
    reportStatus_->setText(QStringLiteral("Word：已提交 %1 · 排队/执行 %2 · 待核对 %3 · 失败 %4").arg(committed).arg(pending).arg(uncertain).arg(failed) + (reportQueue_->diagnostic().isEmpty() ? "" : "\n" + reportQueue_->diagnostic()));
}
void MainWindow::showContextStore()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("只读目录与执行上下文")); dialog.resize(1180, 640); auto* layout = new QVBoxLayout(&dialog);
    auto* note = new QLabel(QStringLiteral("此处只关联证据，不执行指令。来源时间与首次接收时间独立；参数值不写入事件/批次/导出。只有完整且静态版本唯一的显式选择可供截图继承。")); note->setWordWrap(true); layout->addWidget(note);
    auto* actions = new QHBoxLayout; auto* importCatalog = new QPushButton(QStringLiteral("导入目录 JSON / CSV")); auto* importEvent = new QPushButton(QStringLiteral("接收事件 JSON")); auto* exportMetadata = new QPushButton(QStringLiteral("导出脱敏元数据（新文件）")); actions->addWidget(importCatalog); actions->addWidget(importEvent); actions->addWidget(exportMetadata); layout->addLayout(actions);
    auto* tabs = new QTabWidget; layout->addWidget(tabs, 1);
    auto* table = new QTableWidget(0, 10); table->setHorizontalHeaderLabels({QStringLiteral("来源"), QStringLiteral("事件"), "Run", "Step execution", "Invocation", QStringLiteral("尝试"), QStringLiteral("关联 / 静态版本"), QStringLiteral("来源 UTC"), QStringLiteral("首次接收 UTC"), QStringLiteral("类型")}); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); tabs->addTab(table, QStringLiteral("执行事件"));
    auto* tree = new QTreeWidget; tree->setHeaderLabels({QStringLiteral("定义 / 版本"), QStringLiteral("名称 / 类型"), QStringLiteral("实测来源 SHA-256")}); tabs->addTab(tree, QStringLiteral("静态目录"));
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
    connect(importCatalog, &QPushButton::clicked, &dialog, [this, &dialog, note, refresh] { const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择来源目录（只读导入）"), {}, "Catalog (*.json *.csv)"); if (path.isEmpty()) return; QString error; note->setText(contextStore_->importCatalog(path, error) ? QStringLiteral("已保存实测哈希和不可变来源副本；未修改来源文件。") : error); refresh(); });
    connect(importEvent, &QPushButton::clicked, &dialog, [this, &dialog, note, refresh] { const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择规范化事件 JSON"), {}, "Event (*.json)"); if (path.isEmpty()) return; QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { note->setText(QStringLiteral("事件无法读取或超过 1 MiB。")); return; } const auto result = contextStore_->ingest(QJsonDocument::fromJson(file.readAll()).object()); note->setText(result.error ? qs(result.error.message) : QStringLiteral("事件已接收或去重；未切换截图上下文。")); refresh(); });
    connect(exportMetadata, &QPushButton::clicked, &dialog, [this, &dialog, note] { const auto path = QFileDialog::getSaveFileName(&dialog, QStringLiteral("导出脱敏元数据，不覆盖已有文件"), {}, "JSON (*.json)", nullptr, QFileDialog::DontConfirmOverwrite); if (path.isEmpty()) return; QString error; note->setText(contextStore_->exportMetadata(path, true, error) ? QStringLiteral("脱敏元数据已导出。") : error); });
    auto* select = new QPushButton(QStringLiteral("明确选择此事件作为截图上下文")); auto* clear = new QPushButton(QStringLiteral("清除关联")); auto* bottom = new QHBoxLayout; bottom->addWidget(select); bottom->addWidget(clear); layout->addLayout(bottom);
    connect(select, &QPushButton::clicked, &dialog, [this, table, note] { if (table->currentRow() < 0) return; const auto source = table->item(table->currentRow(), 0)->text(), id = table->item(table->currentRow(), 1)->text(); const auto context = contextStore_->context(source, id); const auto snapshot = contextStore_->snapshot(source, id);
        if (context.correlationStatus != "EXACT" || snapshot["definitionStatus"] != "RESOLVED") { selectedContext_ = {}; selectedContextSnapshot_ = {}; contextStatus_->setText(QStringLiteral("截图上下文：未关联（所选身份或静态版本不唯一）。")); note->setText(QStringLiteral("此事件保留歧义/未关联状态，不能自动供热键截图继承。")); return; }
        selectedContext_ = context; selectedContextSnapshot_ = snapshot; contextStatus_->setText(QStringLiteral("显式上下文：%1 / %2 · run=%3 · attempt=%4").arg(source, id, qs(context.sequenceRunId)).arg(context.attempt)); note->setText(QStringLiteral("后续截图将冻结此事件的身份、时间和静态版本；不会随新事件自动漂移。")); });
    connect(clear, &QPushButton::clicked, &dialog, [this] { selectedContext_ = {}; selectedContextSnapshot_ = {}; contextStatus_->setText(QStringLiteral("截图上下文：未关联。")); });
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); layout->addWidget(close); dialog.exec();
}
void MainWindow::showReportQueue()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("Word 报告任务")); dialog.resize(980, 500); auto* layout = new QVBoxLayout(&dialog);
    auto* note = new QLabel(QStringLiteral("截图与 Word 独立。失败或待核对会暂停同一报告的后续任务。排除原因后，仅修改前失败的任务可重试；UNCERTAIN 必须核对标记，不会盲目重插。未发布 pending.docx 保留供核对。")); note->setWordWrap(true); layout->addWidget(note);
    auto* table = new QTableWidget(0, 5); table->setObjectName("reportJobs"); table->setHorizontalHeaderLabels({QStringLiteral("任务"), QStringLiteral("报告"), QStringLiteral("状态"), QStringLiteral("尝试"), QStringLiteral("错误 / 原因")}); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setTextElideMode(Qt::ElideMiddle); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setColumnWidth(0, 120); table->setColumnWidth(1, 270); table->setColumnWidth(2, 110); table->setColumnWidth(3, 50); table->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table);
    auto refresh = [this, table] { const auto selected = table->currentRow() >= 0 && table->item(table->currentRow(), 0) ? table->item(table->currentRow(), 0)->text() : QString{}; table->setRowCount(0);
        for (const auto& job : reportQueue_->jobs()) { const int row = table->rowCount(); table->insertRow(row); const auto ext = job["extensions"].toObject();
            static const QMap<QString, QString> labels{{"QUEUED", QStringLiteral("排队中")}, {"RUNNING", QStringLiteral("正在写入")}, {"COMMITTED", QStringLiteral("已保存提交")}, {"RETRYABLE", QStringLiteral("等待重试")}, {"FAILED", QStringLiteral("失败")}, {"UNCERTAIN", QStringLiteral("待核对")}};
            const auto state = job["state"].toString();
            const QStringList values{job["jobId"].toString(), job["target"].toObject()["documentPath"].toString(), labels.value(state, state), QString::number(job["attemptCount"].toInt()), job["errorCode"].toString() + " " + ext["lastResult"].toObject()["message"].toString()};
            for (int col = 0; col < values.size(); ++col) { auto* cell = new QTableWidgetItem(values[col]); cell->setToolTip(col == 2 ? state : values[col]); table->setItem(row, col, cell); } if (values[0] == selected) table->selectRow(row); }
    }; refresh();
    auto* inspect = new QPushButton(QStringLiteral("核对所选待核对任务的文档标记")); layout->addWidget(inspect);
    connect(inspect, &QPushButton::clicked, &dialog, [this, table, note, refresh] { if (table->currentRow() < 0) return; QString error; if (!reportQueue_->inspectMarker(table->item(table->currentRow(), 0)->text(), error)) note->setText(error); refresh(); });
    auto* retry = new QPushButton(QStringLiteral("已排除原因：重试所选修改前失败任务")); retry->setObjectName("retryFailedReport"); layout->addWidget(retry);
    connect(retry, &QPushButton::clicked, &dialog, [this, table, note, refresh] { if (table->currentRow() < 0) return; QString error; note->setText(reportQueue_->retryFailed(table->item(table->currentRow(), 0)->text(), error) ? QStringLiteral("原任务已重新排队，保留顺序与重试历史。") : error); refresh(); });
    QTimer timer; connect(&timer, &QTimer::timeout, &dialog, refresh); timer.start(1000); auto* close = new QDialogButtonBox(QDialogButtonBox::Close); connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); layout->addWidget(close); dialog.exec();
}
void MainWindow::rebuildScenes()
{
    const auto current = scenes_->currentItem() ? scenes_->currentItem()->data(Qt::UserRole).toString() : QString{};
    scenes_->clear(); const auto& profile = settings_.profiles[std::size_t(settings_.activeProfile)];
    for (const auto& scene : settings_.scenes) {
        const bool compatible = std::all_of(scene.layers.begin(), scene.layers.end(), [&](const auto& l) { return std::any_of(profile.regions.begin(), profile.regions.end(), [&](const auto& r) { return r.id == l.sourceRegionId; }); });
        auto* item = new QListWidgetItem(qs(scene.name) + QString(" · r%1").arg(scene.revision) + (compatible ? "" : QStringLiteral(" · 源待匹配")), scenes_);
        item->setData(Qt::UserRole, qs(scene.id)); item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(settings_.selectedSceneIds.contains(qs(scene.id)) ? Qt::Checked : Qt::Unchecked);
        if (qs(scene.id) == current) scenes_->setCurrentItem(item);
    }
    if (!scenes_->currentItem() && scenes_->count()) scenes_->setCurrentRow(0);
}
void MainWindow::rebuildSceneMenu()
{
    if (!sceneMenu_) return; sceneMenu_->clear();
    if (settings_.scenes.empty()) { auto* empty = sceneMenu_->addAction(QStringLiteral("尚无场景，请先新建拼图")); empty->setEnabled(false); return; }
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
            if (saveSettings(configurationPath_, candidate, error)) { settings_ = candidate; loadProfile(); setStatus(candidate.selectedSceneIds.empty() ? QStringLiteral("未启用拼图，当前仅保存原图。") : QStringLiteral("已更新当前配置的拼图输出。")); }
            else setStatus(error, true);
        });
    }
}
void MainWindow::editScene(bool createNew, bool historical)
{
    if (busy_ || editingRegions_ || editingScene_) { setStatus(QStringLiteral("请等待当前截图或编辑完成。"), true); return; }
    if (!applySettings()) return;
    auto profile = settings_.profiles[std::size_t(settings_.activeProfile)]; SceneSources sources;
    if (historical) {
        const auto manifest = QFileDialog::getOpenFileName(this, QStringLiteral("打开已保存批次的 batch.json"), settings_.outputDirectory, "Batch (batch.json);;JSON (*.json)");
        if (manifest.isEmpty()) return; QString error;
        if (!loadHistoricalSources(manifest, sources, profile, error)) { setStatus(error, true); return; }
    } else if (lastOutcome_ && lastOutcome_->batch.request.profile.id == profile.id) sources = batchSources(lastOutcome_->batch);
    core::SceneTemplate model; bool existing = false;
    if (!createNew && scenes_->currentItem()) {
        const auto chosen = scenes_->currentItem()->data(Qt::UserRole).toString().toStdString();
        const auto found = std::find_if(settings_.scenes.begin(), settings_.scenes.end(), [&](const auto& scene) { return scene.id == chosen; });
        if (found != settings_.scenes.end()) { model = *found; existing = true; }
    }
    if (historical && existing && !std::all_of(model.layers.begin(), model.layers.end(), [&](const auto& layer) { return sources.count(layer.sourceRegionId) > 0; })) existing = false;
    if (!existing) model = core::presetScene(profile, id(), historical ? "历史重排" : "拼图 " + std::to_string(settings_.scenes.size() + 1), 1600, 900, core::LayoutPreset::Grid);
    editingScene_ = true;
    SceneEditor dialog(model, profile, sources, this);
    const auto accepted = dialog.exec() == QDialog::Accepted; editingScene_ = false;
    if (!accepted) { setStatus(QStringLiteral("已取消场景编辑，截图已恢复。")); return; }
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
    setStatus(compatible ? QStringLiteral("场景已保存并启用；后续截图保存原图和拼图，复制所选拼图。") : QStringLiteral("历史场景已保存；源与当前配置不同，未自动启用。可在编辑器导出派生图。"));
}
void MainWindow::importScene()
{
    if (busy_ || editingRegions_ || editingScene_ || !applySettings()) return;
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("导入独立场景模板"), {}, "Scene (*.json)"); if (path.isEmpty()) return;
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) { setStatus(file.errorString(), true); return; }
    if (file.size() > 8 * 1024 * 1024) { setStatus(QStringLiteral("场景文件超过 8 MiB。"), true); return; }
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(file.readAll(), &parse); QString error; core::SceneTemplate scene;
    if (parse.error || !document.isObject() || !sceneFromJson(document.object(), scene, error)) { setStatus(error.isEmpty() ? QStringLiteral("场景 JSON 无效，原文件未改动。") : error, true); return; }
    auto candidate = settings_;
    if (std::any_of(candidate.scenes.begin(), candidate.scenes.end(), [&](const auto& s) { return s.id == scene.id; })) { scene.id = id(); scene.revision = 1; scene.name += "（导入副本）"; }
    candidate.scenes.push_back(scene);
    if (!saveSettings(configurationPath_, candidate, error)) { setStatus(error, true); return; }
    settings_ = candidate; loadProfile(); setStatus(QStringLiteral("场景已导入。请编辑确认源映射，再勾选输出并应用配置。"));
}
void MainWindow::exportScene()
{
    if (!scenes_->currentItem()) { setStatus(QStringLiteral("请先选择一个场景。"), true); return; }
    const auto chosen = scenes_->currentItem()->data(Qt::UserRole).toString().toStdString();
    const auto found = std::find_if(settings_.scenes.begin(), settings_.scenes.end(), [&](const auto& scene) { return scene.id == chosen; }); if (found == settings_.scenes.end()) return;
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("导出为新的场景文件（不覆盖）"), {}, "Scene (*.json)", nullptr, QFileDialog::DontConfirmOverwrite); if (path.isEmpty()) return;
    QString error; if (saveBytesNew(QJsonDocument(sceneJson(*found)).toJson(), path, error)) setStatus(QStringLiteral("独立场景模板已导出。")); else setStatus(error, true);
}
void MainWindow::editRegions()
{
    if (busy_ || editingRegions_ || editingScene_) { setStatus(QStringLiteral("请等待当前采集或编辑完成。"), true); return; }
    if (!applySettings()) return;
    const bool restore = isVisible(); hide(); editingRegions_ = true;
    // Let the compositor remove the host before taking editor-only background frames.
    QTimer::singleShot(180, this, [this, restore] {
        try {
            openRegionEditor(this, settings_.profiles[std::size_t(settings_.activeProfile)], [this, restore](bool accepted, core::CaptureProfile profile) {
                if (accepted) {
                    auto candidate = settings_;
                    const auto target = std::find_if(candidate.profiles.begin(), candidate.profiles.end(), [&](const auto& p) { return p.id == profile.id; });
                    if (target == candidate.profiles.end()) { editingRegions_ = false; setStatus(QStringLiteral("编辑配置已不存在，未改写其他配置。"), true); if (restore) showNormal(); return; }
                    *target = std::move(profile); QString error;
                    if (saveSettings(configurationPath_, candidate, error)) { settings_ = candidate; loadProfile(); setStatus(QStringLiteral("区域已保存，截图已恢复。")); }
                    else setStatus(error, true);
                } else setStatus(QStringLiteral("已取消框选，区域未改动。"));
                editingRegions_ = false; if (restore) { showNormal(); raise(); activateWindow(); }
            });
        } catch (const std::exception& e) { editingRegions_ = false; setStatus(QString::fromUtf8(e.what()), true); if (restore) showNormal(); }
    });
}

} // namespace aitsnap
