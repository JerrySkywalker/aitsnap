// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "adapters/win_capture.h"
#include "adapters/report_queue.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static std::string id() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
void MainWindow::buildUi()
{
    auto* central = new QWidget(this); auto* outer = new QVBoxLayout(central); outer->setContentsMargins(18, 16, 18, 12); outer->setSpacing(12);
    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("AITSnap 0.7.0"); title->setStyleSheet("font-size:25px;font-weight:600;"); titleRow->addWidget(title);
    titleRow->addWidget(new QLabel(uiText("多屏证据工作台"))); titleRow->addStretch();
    profiles_ = new QComboBox; profiles_->setObjectName("profiles"); for (const auto& p : settings_.profiles) profiles_->addItem(qs(p.name)); profiles_->setCurrentIndex(settings_.activeProfile);
    titleRow->addWidget(new QLabel(uiText("截图配置"))); titleRow->addWidget(profiles_);
    auto* addProfile = new QPushButton(uiText("新建配置")); titleRow->addWidget(addProfile);
    auto* apply = new QPushButton(uiText("应用配置")); apply->setObjectName("applySettings"); titleRow->addWidget(apply); outer->addLayout(titleRow);
    auto* body = new QHBoxLayout; outer->addLayout(body, 1);
    navigation_ = new QListWidget; navigation_->setObjectName("workspaceNavigation"); navigation_->setFixedWidth(158);
    navigation_->addItems({uiText("工作台"), uiText("源区域"), uiText("拼图场景"), uiText("批次与报告"), uiText("设置")});
    navigation_->setSpacing(8); body->addWidget(navigation_);
    navigation_->setStyleSheet("QListWidget::item {padding:10px 8px;}");
    pages_ = new QStackedWidget; pages_->setObjectName("workspacePages"); body->addWidget(pages_, 1);
    auto page = [this] { auto* p = new QWidget; pages_->addWidget(p); return new QVBoxLayout(p); };
    auto subtitle = [](QVBoxLayout* layout, const QString& text) { auto* label = new QLabel(text); label->setWordWrap(true); label->setProperty("muted", true); layout->addWidget(label); return label; };
    auto* workbench = page(); overviewSummary_ = subtitle(workbench, {}); overviewSummary_->setObjectName("workspaceSummary");
    auto* workSplit = new QSplitter;
    auto* overviewSourcePanel = new QWidget; auto* overviewSourceLayout = new QVBoxLayout(overviewSourcePanel); overviewSourceLayout->setContentsMargins(0, 0, 0, 0);
    overviewSourceLayout->addWidget(new QLabel(uiText("源区域"))); overviewSources_ = new QListWidget; overviewSources_->setObjectName("overviewSources"); overviewSources_->setIconSize({80, 50}); overviewSources_->setWordWrap(true); overviewSources_->setMinimumWidth(180); overviewSourceLayout->addWidget(overviewSources_, 1); workSplit->addWidget(overviewSourcePanel);
    overviewTopology_ = new DisplayCanvas; overviewTopology_->setObjectName("overviewDisplayLayout"); workSplit->addWidget(overviewTopology_);
    auto* overviewOutputPanel = new QWidget; auto* overviewOutputLayout = new QVBoxLayout(overviewOutputPanel); overviewOutputLayout->setContentsMargins(0, 0, 0, 0);
    overviewOutputLayout->addWidget(new QLabel(uiText("下一次输出"))); overviewOutputs_ = new QListWidget; overviewOutputs_->setObjectName("overviewOutputs"); overviewOutputs_->setIconSize({96, 60}); overviewOutputs_->setMinimumWidth(220); overviewOutputs_->setWordWrap(true); overviewOutputLayout->addWidget(overviewOutputs_, 1);
    preview_ = new ImagePreview; preview_->setObjectName("latestCapturePreview"); preview_->setMaximumHeight(190); preview_->setImage({}, uiText("最近一次截图 · 尚未采集")); overviewOutputLayout->addWidget(preview_); workSplit->addWidget(overviewOutputPanel);
    workSplit->setSizes({200, 470, 260}); workbench->addWidget(workSplit, 1);
    auto* quick = new QHBoxLayout; auto* quickRegions = new QPushButton(uiText("查看源区域")); auto* quickScenes = new QPushButton(uiText("查看拼图输出")); auto* quickBatches = new QPushButton(uiText("浏览已保存批次"));
    quick->addWidget(quickRegions); quick->addWidget(quickScenes); quick->addWidget(quickBatches); quick->addStretch(); workbench->addLayout(quick);
    connect(quickRegions, &QPushButton::clicked, this, [this] { navigation_->setCurrentRow(1); }); connect(quickScenes, &QPushButton::clicked, this, [this] { navigation_->setCurrentRow(2); }); connect(quickBatches, &QPushButton::clicked, this, [this] { navigation_->setCurrentRow(3); });

    auto* sources = page(); subtitle(sources, uiText("先看显示器与区域分布，再切换区域浏览；修改后应用配置。"));
    auto* sourceSplit = new QSplitter;
    sourceCards_ = new QListWidget; sourceCards_->setObjectName("sourceCards"); sourceCards_->setMinimumWidth(250); sourceCards_->setWordWrap(true); sourceCards_->setIconSize({96, 60}); sourceCards_->setSpacing(5); sourceSplit->addWidget(sourceCards_);
    sourceViews_ = new QTabWidget; sourceViews_->setObjectName("sourceViews"); topology_ = new DisplayCanvas; topology_->setObjectName("sourceDisplayLayout");
    sourceViews_->addTab(topology_, uiText("显示器布局")); sourcePreview_ = new ImagePreview; sourcePreview_->setObjectName("sourceImagePreview"); sourceViews_->addTab(sourcePreview_, uiText("区域浏览")); sourceSplit->addWidget(sourceViews_); sourceSplit->setSizes({240, 750}); sources->addWidget(sourceSplit, 1);
    sourceDetail_ = subtitle(sources, {}); sourceDetail_->setObjectName("sourceDetails");
    auto* buttons = new QHBoxLayout; auto* add = new QPushButton(uiText("添加区域")); auto* remove = new QPushButton(uiText("删除选中")); auto* calibrate = new QPushButton(uiText("确认当前屏幕布局")); auto* visual = new QPushButton(uiText("屏幕框选 / 拖动")); visual->setObjectName("editRegions");
    buttons->addWidget(visual); buttons->addWidget(add); buttons->addWidget(remove); buttons->addWidget(calibrate); sources->addLayout(buttons);
    auto* advanced = new QCheckBox(uiText("显示高级坐标编辑")); sources->addWidget(advanced);
    regions_ = new QTableWidget(0, 6); regions_->setObjectName("regions"); regions_->setHorizontalHeaderLabels({uiText("启用"), uiText("名称"), "X", "Y", uiText("宽 px"), uiText("高 px")});
    regions_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); regions_->setSelectionBehavior(QAbstractItemView::SelectRows); regions_->setMaximumHeight(220); regions_->hide(); sources->addWidget(regions_);
    connect(advanced, &QCheckBox::toggled, regions_, &QWidget::setVisible);
    auto chooseSource = [this](int row) { if (row >= 0 && row < regions_->rowCount()) { overviewOutputKey_.clear(); regions_->setCurrentCell(row, 1); QTimer::singleShot(0, this, [this] { refreshWorkspace(); }); } };
    topology_->selected = chooseSource; overviewTopology_->selected = chooseSource;
    connect(sourceCards_, &QListWidget::currentRowChanged, this, chooseSource);
    connect(overviewSources_, &QListWidget::currentRowChanged, this, chooseSource);
    connect(overviewSources_, &QListWidget::itemDoubleClicked, this, [this] { navigation_->setCurrentRow(1); });
    connect(overviewSources_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) { const int row = overviewSources_->row(item); if (row >= 0 && row < regions_->rowCount()) regions_->item(row, 0)->setCheckState(item->checkState()); });
    connect(overviewOutputs_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) { overviewOutputKey_ = item ? item->data(Qt::UserRole).toString() : QString{}; QTimer::singleShot(0, this, [this] { refreshWorkspace(); }); });
    connect(overviewOutputs_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        if (item->data(Qt::UserRole).toString().startsWith("scene:")) navigation_->setCurrentRow(2);
        else { navigation_->setCurrentRow(1); sourceViews_->setCurrentIndex(1); }
    });
    connect(sourceCards_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const int row = sourceCards_->row(item); if (row < 0 || row >= regions_->rowCount()) return;
        bool ok{}; const auto name = QInputDialog::getText(this, uiText("重命名区域"), uiText("名称"), QLineEdit::Normal, regions_->item(row, 1)->text(), &ok);
        if (ok && !name.trimmed().isEmpty()) regions_->item(row, 1)->setText(name.trimmed());
    });
    connect(sourceCards_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) { const int row = sourceCards_->row(item); if (row >= 0 && row < regions_->rowCount()) regions_->item(row, 0)->setCheckState(item->checkState()); });
    auto* refreshTimer = new QTimer(this); refreshTimer->setSingleShot(true); refreshTimer->setInterval(30);
    connect(refreshTimer, &QTimer::timeout, this, [this] { refreshWorkspace(); });
    connect(regions_, &QTableWidget::itemChanged, this, [refreshTimer] { refreshTimer->start(); });
    auto watchScreen = [this, refreshTimer](QScreen* screen) {
        connect(screen, &QScreen::geometryChanged, this, [refreshTimer] { refreshTimer->start(300); });
        connect(screen, &QScreen::logicalDotsPerInchChanged, this, [refreshTimer] { refreshTimer->start(300); });
    };
    for (auto* screen : QGuiApplication::screens()) watchScreen(screen);
    connect(qApp, &QGuiApplication::screenAdded, this, [watchScreen, refreshTimer](QScreen* screen) { watchScreen(screen); refreshTimer->start(300); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [refreshTimer] { refreshTimer->start(300); });
    connect(regions_, &QTableWidget::currentCellChanged, this, [this] { QTimer::singleShot(0, this, [this] { refreshWorkspace(); }); });

    auto* compositions = page(); subtitle(compositions, uiText("选择场景预览，再打开原有画布编辑；勾选用于下一次截图。"));
    auto* sceneSplit = new QSplitter; scenes_ = new QListWidget; scenes_->setObjectName("enabledScenes"); scenes_->setIconSize({160, 90}); scenes_->setSpacing(6); sceneSplit->addWidget(scenes_);
    scenePreview_ = new ImagePreview; scenePreview_->setObjectName("sceneOutputPreview"); sceneSplit->addWidget(scenePreview_); sceneSplit->setSizes({300, 680}); compositions->addWidget(sceneSplit, 1);
    auto* sceneActions = new QHBoxLayout; auto* newScene = new QPushButton(uiText("新建拼图")); newScene->setObjectName("newScene"); auto* editSceneButton = new QPushButton(uiText("编辑拼图")); editSceneButton->setObjectName("editScene");
    auto* history = new QPushButton(uiText("重排历史批次")); history->setObjectName("openHistoricalBatch"); auto* import = new QPushButton(uiText("导入场景")); auto* exportTemplate = new QPushButton(uiText("导出场景"));
    for (auto* button : {newScene, editSceneButton, history, import, exportTemplate}) sceneActions->addWidget(button); compositions->addLayout(sceneActions);
    outputMode_ = new QComboBox; outputMode_->setObjectName("outputMode"); outputMode_->addItem(uiText("保存原图"), "originals"); outputMode_->addItem(uiText("保存拼图（源图存入证据子目录）"), "composites"); outputMode_->addItem(uiText("原图 + 选中拼图"), "both"); compositions->addWidget(outputMode_);
    connect(outputMode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [refreshTimer] { refreshTimer->start(); });
    connect(scenes_, &QListWidget::itemChanged, this, [refreshTimer] { refreshTimer->start(); });
    connect(newScene, &QPushButton::clicked, this, [this] { editScene(true); }); connect(editSceneButton, &QPushButton::clicked, this, [this] { editScene(false); }); connect(history, &QPushButton::clicked, this, [this] { editScene(false, true); });
    connect(import, &QPushButton::clicked, this, [this] { importScene(); }); connect(exportTemplate, &QPushButton::clicked, this, [this] { exportScene(); }); connect(scenes_, &QListWidget::currentRowChanged, this, [this] { refreshWorkspace(); });

    auto* batches = page(); subtitle(batches, uiText("浏览当前输出目录的批次和原图/拼图；右侧展示关联 Word 任务，实际排版请在 Word 查看。"));
    auto* batchSplit = new QSplitter; batchCards_ = new QListWidget; batchCards_->setObjectName("batchCards"); batchSplit->addWidget(batchCards_);
    auto* imagesPane = new QWidget; auto* imagesLayout = new QVBoxLayout(imagesPane); batchPreview_ = new ImagePreview; batchPreview_->setObjectName("batchImagePreview"); imagesLayout->addWidget(batchPreview_, 1);
    batchImages_ = new QListWidget; batchImages_->setObjectName("batchImages"); batchImages_->setMaximumHeight(110); imagesLayout->addWidget(batchImages_); batchSplit->addWidget(imagesPane);
    reportCards_ = new QListWidget; reportCards_->setObjectName("reportCards"); reportCards_->setMinimumWidth(200); batchSplit->addWidget(reportCards_); batchSplit->setSizes({240, 560, 240}); batches->addWidget(batchSplit, 1);
    batchDetail_ = subtitle(batches, {}); auto* batchActions = new QHBoxLayout; auto* refresh = new QPushButton(uiText("刷新批次")); auto* batchFolder = new QPushButton(uiText("打开批次目录")); auto* queueButton = new QPushButton(uiText("查看 Word 任务 / 恢复核对"));
    auto* recomposeBatch = new QPushButton(uiText("用本批重排")); batchActions->addWidget(refresh); batchActions->addWidget(batchFolder); batchActions->addWidget(recomposeBatch); batchActions->addWidget(queueButton); batches->addLayout(batchActions); reportStatus_ = subtitle(batches, {});
    appendBatch_ = new QPushButton(uiText("将本批加入报告")); appendBatch_->setObjectName("appendSelectedBatch"); batchActions->insertWidget(3, appendBatch_);
    connect(appendBatch_, &QPushButton::clicked, this, [this] {
        if (selectedBatchManifest_.isEmpty() || !applySettings()) return;
        QString error;
        if (reportQueue_->enqueueHistorical(selectedBatchManifest_, settings_.report, error)) { setStatus(uiText("报告任务已排队，截图可继续。")); refreshReportStatus(); }
        else setStatus(error, true);
    });
    connect(recomposeBatch, &QPushButton::clicked, this, [this] { if (!selectedBatchManifest_.isEmpty()) editScene(false, true, selectedBatchManifest_); });
    connect(refresh, &QPushButton::clicked, this, [this] { refreshBatches(); }); connect(batchCards_, &QListWidget::currentRowChanged, this, [this] { selectBatch(); }); connect(batchImages_, &QListWidget::currentRowChanged, this, [this] { selectBatchImage(); });
    connect(batchFolder, &QPushButton::clicked, this, [this] { if (!selectedBatchManifest_.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(selectedBatchManifest_).absolutePath())); }); connect(queueButton, &QPushButton::clicked, this, [this] { showReportQueue(); });

    auto* settings = page(); auto* settingsSplit = new QSplitter; auto* categories = new QListWidget; categories->setObjectName("settingsCategories"); categories->setMaximumWidth(180);
    categories->addItems({uiText("常用设置"), uiText("外观"), uiText("保存与命名"), uiText("快捷键与托盘"), uiText("Word 报告"), uiText("元数据关联"), uiText("关于 AITSnap")}); settingsSplit->addWidget(categories);
    categories->setSpacing(4); categories->setStyleSheet("QListWidget::item {padding:10px 8px;}");
    auto* settingPages = new QStackedWidget; settingPages->setObjectName("settingsPages"); settingsSplit->addWidget(settingPages); settings->addWidget(settingsSplit, 1);
    auto makeForm = [settingPages] { auto* content = new QWidget; auto* form = new QFormLayout(content); form->setContentsMargins(20, 16, 20, 16); form->setVerticalSpacing(14); form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(content); settingPages->addWidget(scroll); return form; };
    auto* commonForm = makeForm(); profileName_ = new QLineEdit; commonForm->addRow(uiText("配置名称"), profileName_); clipboard_ = new QComboBox; commonForm->addRow(uiText("复制哪张图"), clipboard_);
    auto* appendLast = new QPushButton(uiText("将最近保存批次加入报告")); appendLast_ = appendLast; appendLast_->setObjectName("appendLatestBatch"); commonForm->addRow(appendLast);
    connect(appendLast, &QPushButton::clicked, this, [this] { if (!lastOutcome_ || lastOutcome_->directory.isEmpty()) { setStatus(uiText("请先保存一批截图。"), true); return; } if (!applySettings()) return; if (reportQueue_->hasBatchForDocument(qs(lastOutcome_->batch.batchId), settings_.report.documentPath)) { setStatus(uiText("本批已加入报告")); return; } QString error; if (!reportQueue_->enqueue(*lastOutcome_, settings_.report, error)) setStatus(error, true); else setStatus(uiText("报告任务已排队，截图可继续。")); });
    auto* appearanceForm = makeForm(); language_ = new QComboBox; language_->setObjectName("appearanceLanguage"); language_->addItem(uiText("跟随系统"), "system"); language_->addItem(QStringLiteral("简体中文"), "zh-CN"); language_->addItem("English", "en-US"); language_->setCurrentIndex(std::max(0, language_->findData(settings_.language))); appearanceForm->addRow(uiText("界面语言"), language_);
    colorMode_ = new QComboBox; colorMode_->setObjectName("appearanceColorMode"); colorMode_->addItem(uiText("跟随系统"), "system"); colorMode_->addItem(uiText("浅色"), "light"); colorMode_->addItem(uiText("深色"), "dark"); colorMode_->setCurrentIndex(std::max(0, colorMode_->findData(settings_.colorMode))); appearanceForm->addRow(uiText("色彩模式"), colorMode_);
    auto* appearanceNote = new QLabel(uiText("配色立即生效；语言更改将在重新启动 AITSnap 后生效。")); appearanceNote->setWordWrap(true); appearanceForm->addRow(appearanceNote);
    appearanceTopology_ = new DisplayCanvas; appearanceTopology_->setMaximumHeight(360);
    appearanceForm->addRow(uiText("外观效果预览"), appearanceTopology_);
    connect(colorMode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { applyColorMode(colorMode_->currentData().toString()); });
    auto* saveForm = makeForm(); directory_ = new QLineEdit(settings_.outputDirectory); directory_->setObjectName("outputDirectory"); auto* pathRow = new QWidget; auto* paths = new QHBoxLayout(pathRow); paths->setContentsMargins(0, 0, 0, 0); paths->addWidget(directory_); auto* browse = new QPushButton("…"); paths->addWidget(browse); saveForm->addRow(uiText("保存目录"), pathRow);
    format_ = new QComboBox; format_->addItems({"PNG", "JPEG", "BMP"}); format_->setCurrentText(settings_.format); saveForm->addRow(uiText("图片格式"), format_);
    filename_ = new QLineEdit(settings_.filenameTemplate); filename_->setToolTip("{datetime} {sequence} {number} {region} {profile} {test} {device} {station} {operator} {run} {step} {command}"); saveForm->addRow(uiText("文件命名"), filename_);
    filenamePreview_ = new QLabel; filenamePreview_->setObjectName("filenamePreview"); filenamePreview_->setWordWrap(true); saveForm->addRow(uiText("文件名预览"), filenamePreview_);
    connect(format_, &QComboBox::currentTextChanged, this, [refreshTimer] { refreshTimer->start(); });
    auto* hotkeys = makeForm(); saveKey_ = new QLineEdit(settings_.saveHotkey); copyKey_ = new QLineEdit(settings_.copyHotkey); bothKey_ = new QLineEdit(settings_.bothHotkey); hotkeys->addRow(uiText("保存热键"), saveKey_); hotkeys->addRow(uiText("复制热键"), copyKey_); hotkeys->addRow(uiText("保存并复制"), bothKey_); startTray_ = new QCheckBox(uiText("启动时直接进入托盘")); startTray_->setChecked(settings_.startInTray); hotkeys->addRow(startTray_);
    auto* reportForm = makeForm(); reportEnabled_ = new QCheckBox(uiText("保存截图后自动追加 Word")); reportEnabled_->setChecked(settings_.report.enabled); reportForm->addRow(reportEnabled_);
    auto* reportPathRow = new QWidget; auto* reportPaths = new QHBoxLayout(reportPathRow); reportPaths->setContentsMargins(0, 0, 0, 0); reportPath_ = new QLineEdit(settings_.report.documentPath); reportPath_->setObjectName("reportDocumentPath"); reportPaths->addWidget(reportPath_); auto* reportBrowse = new QPushButton("…"); reportPaths->addWidget(reportBrowse); reportForm->addRow(uiText("Word 报告"), reportPathRow);
    connect(reportBrowse, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getSaveFileName(this, uiText("选择已有报告，或输入新 DOCX 文件名"), reportPath_->text(), "Word (*.docx)", nullptr, QFileDialog::DontConfirmOverwrite); if (!path.isEmpty()) reportPath_->setText(path); });
    connect(reportPath_, &QLineEdit::textChanged, this, [this] { if (reportQueue_) refreshReportStatus(); });
    reportMode_ = new QComboBox; reportMode_->addItem(uiText("原图与拼图"), "both"); reportMode_->addItem(uiText("仅原图"), "originals"); reportMode_->addItem(uiText("仅拼图"), "composites"); reportMode_->setCurrentIndex(std::max(0, reportMode_->findData(settings_.report.itemMode))); reportForm->addRow(uiText("输出"), reportMode_);
    reportLayout_ = new QComboBox; reportLayout_->addItem(uiText("顺序图文"), "inline"); reportLayout_->addItem(uiText("两列表格"), "table"); reportLayout_->setCurrentIndex(std::max(0, reportLayout_->findData(settings_.report.layout))); reportForm->addRow(uiText("报告"), reportLayout_);
    auto* metadata = makeForm(); testId_ = new QLineEdit(settings_.testId); device_ = new QLineEdit(settings_.device); station_ = new QLineEdit(settings_.station); operator_ = new QLineEdit(settings_.operatorName); metadata->addRow(uiText("试验标识"), testId_); metadata->addRow(uiText("设备标识"), device_); metadata->addRow(uiText("工位"), station_); metadata->addRow(uiText("操作员"), operator_);
    auto* contextButton = new QPushButton(uiText("静态目录 / 执行事件 / 选择截图上下文")); contextButton->setObjectName("openContextStore"); metadata->addRow(contextButton); contextStatus_ = new QLabel(uiText("截图上下文：未关联。事件到达不会自动切换当前上下文。")); contextStatus_->setWordWrap(true); metadata->addRow(contextStatus_); connect(contextButton, &QPushButton::clicked, this, [this] { showContextStore(); });
    auto* rules = new QPushButton(uiText("事件截图规则（默认无自动规则）")); metadata->addRow(rules); connect(rules, &QPushButton::clicked, this, [this] { editEventRules(); }); hostStatus_ = new QLabel; hostStatus_->setWordWrap(true); metadata->addRow(hostStatus_);
    auto* about = makeForm(); about->addRow(new QLabel("AITSnap 0.7.0")); auto* aboutNote = new QLabel(uiText("多屏截图、托盘操作、自由拼图与 Word 报告。Word 报告需要已安装 Microsoft Word。截图、配置和报告保存在本机。")); aboutNote->setWordWrap(true); about->addRow(aboutNote);
    for (auto* line : {profileName_, filename_, testId_, device_, station_, operator_}) connect(line, &QLineEdit::textChanged, this, [refreshTimer] { refreshTimer->start(); });
    connect(categories, &QListWidget::currentRowChanged, settingPages, &QStackedWidget::setCurrentIndex); categories->setCurrentRow(0);
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int index) { pages_->setCurrentIndex(index); if (index == 1) sourceViews_->setCurrentIndex(0); if (index == 3) refreshBatches(); }); navigation_->setCurrentRow(0);
    auto* actions = new QHBoxLayout; auto* save = new QPushButton(uiText("截图并保存")); auto* copy = new QPushButton(uiText("复制选中图片")); auto* both = new QPushButton(uiText("保存并复制")); auto* folder = new QPushButton(uiText("打开输出目录")); save->setObjectName("captureSave"); save->setProperty("primary", true);
    actions->addWidget(save); actions->addWidget(copy); actions->addWidget(both); actions->addStretch(); actions->addWidget(folder); outer->addLayout(actions);
    status_ = new QLabel; status_->setObjectName("captureStatus"); status_->setWordWrap(true); status_->setTextInteractionFlags(Qt::TextSelectableByMouse); outer->addWidget(status_); setCentralWidget(central);
    connect(apply, &QPushButton::clicked, this, [this] { applySettings(); });
    connect(add, &QPushButton::clicked, this, [this] { addRegion(); });
    connect(visual, &QPushButton::clicked, this, [this] { editRegions(); });
    connect(remove, &QPushButton::clicked, this, [this] { if (regions_->currentRow() >= 0) regions_->removeRow(regions_->currentRow()); });
    connect(browse, &QPushButton::clicked, this, [this] { auto p = QFileDialog::getExistingDirectory(this, aitsnap::uiText("选择截图保存目录"), directory_->text()); if (!p.isEmpty()) directory_->setText(p); });
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
        bool ok{}; auto name = QInputDialog::getText(this, aitsnap::uiText("新建截图配置"), aitsnap::uiText("名称"), QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        auto p = settings_.profiles[std::size_t(settings_.activeProfile)]; p.id = id(); p.name = name.trimmed().toStdString(); p.revision = 1;
        settings_.profiles.push_back(p); profiles_->addItem(name); profiles_->setCurrentIndex(int(settings_.profiles.size()) - 1);
    });
    connect(calibrate, &QPushButton::clicked, this, [this] {
        Settings candidate = settings_; QString e;
        if (!collectSettings(candidate, e)) { setStatus(e, true); return; }
        try { WinCaptureBackend b; candidate.profiles[std::size_t(candidate.activeProfile)].calibratedDisplays = b.displays(); }
        catch (const std::exception& x) { setStatus(QString::fromUtf8(x.what()), true); return; }
        if (configurationBlocked_) { setStatus(aitsnap::uiText("原配置无法读取。请先核对并应用配置。"), true); return; }
        if (saveSettings(configurationPath_, candidate, e)) { settings_ = candidate; refreshWorkspace(); setStatus(aitsnap::uiText("已确认显示器布局。请核对物理区域后截图。")); } else setStatus(e, true);
    });
}

} // namespace aitsnap
