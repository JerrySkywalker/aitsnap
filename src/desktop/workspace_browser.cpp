// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "adapters/win_capture.h"
#include "adapters/report_queue.h"
#include "adapters/live_preview.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSet>
#include <QTableWidget>
#include <QScrollBar>
#include <algorithm>

namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static QImage smallImage(const QString& path)
{
    QImageReader reader(path); auto size = reader.size();
    if (size.width() > 1200 || size.height() > 800) { size.scale({1200, 800}, Qt::KeepAspectRatio); reader.setScaledSize(size); }
    return reader.read();
}
static QSet<QString> selectionIds(QListWidget* list)
{
    QSet<QString> ids; for (const auto* item : list->selectedItems()) ids.insert(item->data(Qt::UserRole).toString()); return ids;
}
static void restoreSelection(QListWidget* list, int current, const QSet<QString>& ids, bool fallback)
{
    fallback = fallback && !list->property("selectionInitialized").toBool();
    list->setCurrentItem(current >= 0 ? list->item(current) : nullptr, QItemSelectionModel::NoUpdate);
    for (int row=0; row<list->count(); ++row) list->item(row)->setSelected(ids.contains(list->item(row)->data(Qt::UserRole).toString()) || (fallback && ids.empty() && row==current));
    list->setProperty("selectionInitialized", true);
}
static RenderResult sceneThumbnail(core::SceneTemplate scene, const SceneSources& sources)
{
    return renderScenePreview(std::move(scene),sources);
}
void MainWindow::refreshWorkspace()
{
    if (refreshingWorkspace_ || objectMenuOpen_ || !sourceCards_ || settings_.profiles.empty()) return;
    QScopedValueRollback<bool> guard(refreshingWorkspace_, true);
    auto candidate = settings_; QString error; if (!collectSettings(candidate, error)) candidate = settings_;
    const auto& profile = candidate.profiles[std::size_t(candidate.activeProfile)];
    if (overviewOutputKey_.startsWith("scene:")) {
        if (candidate.outputMode == "originals" || !candidate.selectedSceneIds.contains(overviewOutputKey_.mid(6))) overviewOutputKey_.clear();
    } else if (!overviewOutputKey_.isEmpty() && (candidate.outputMode == "composites" || std::none_of(profile.regions.begin(), profile.regions.end(), [&](const auto& r) { return r.enabled && qs(r.id) == overviewOutputKey_; }))) overviewOutputKey_.clear();
    if (error.isEmpty()) {
        auto next = candidate; ++next.sequence; core::CaptureRequest sample; sample.profile = profile;
        sample.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
        sample.context = selectedContextSnapshot_.isEmpty() ? core::Context{} : selectedContext_;
        if (selectedContextSnapshot_.isEmpty()) { sample.context.testId = candidate.testId.toStdString(); sample.context.deviceId = candidate.device.toStdString(); }
        sample.context.stationId = candidate.station.toStdString(); sample.context.operatorId = candidate.operatorName.toStdString();
        const auto region = std::find_if(profile.regions.begin(), profile.regions.end(), [](const auto& r) { return r.enabled; });
        if (region != profile.regions.end()) filenamePreview_->setText(expandFilename(next, sample, *region, error) + "." + candidate.format.toLower());
        else filenamePreview_->setText(uiText("添加并启用区域后显示文件名"));
    } else filenamePreview_->setText(error);
    std::vector<core::Display> displays; try { WinCaptureBackend backend; displays = backend.displays(); } catch (...) { /* Unknown layout is rendered explicitly. */ }
    int selected = regions_->currentRow(); if (selected < 0 && !profile.regions.empty()) selected = 0;
    if (!profile.regions.empty()) selected = std::min(selected, int(profile.regions.size()) - 1);
    std::vector<std::string> outputSources;
    if (overviewOutputKey_.startsWith("scene:")) {
        const auto id = overviewOutputKey_.mid(6).toStdString();
        const auto scene = std::find_if(candidate.scenes.begin(), candidate.scenes.end(), [&](const auto& s) { return s.id == id; });
        if (scene != candidate.scenes.end()) for (const auto& layer : scene->layers) if (layer.visible) outputSources.push_back(layer.sourceRegionId);
    } else if (!overviewOutputKey_.isEmpty()) {
        for (int i = 0; i < int(profile.regions.size()); ++i) if (qs(profile.regions[std::size_t(i)].id) == overviewOutputKey_) { selected = i; regions_->setCurrentCell(i, 1); break; }
    }
    const auto selectedCards=selectionIds(sourceCards_), selectedOverview=selectionIds(overviewSources_), selectedOutputs=selectionIds(overviewOutputs_);
    QListWidget* scrolling[]{sourceCards_,overviewSources_,overviewOutputs_};int vertical[3]{},horizontal[3]{};for(int i=0;i<3;++i){vertical[i]=scrolling[i]->verticalScrollBar()->value();horizontal[i]=scrolling[i]->horizontalScrollBar()->value();}
    const QSignalBlocker blocker(sourceCards_), overviewBlock(overviewSources_), outputBlock(overviewOutputs_);
    sourceCards_->clear(); overviewSources_->clear(); overviewOutputs_->clear();
    syncLivePreview(profile);
    const bool live=livePreview_ && livePreviewEnabled_->isChecked();
    const bool sameProfile = !live && lastOutcome_ && lastOutcome_->batch.request.profile.id == profile.id;
    const auto sources = live?livePreview_->snapshot().sources:sameProfile ? batchSources(lastOutcome_->batch) : SceneSources{};
    std::map<std::string,QImage> images;for(const auto& region:profile.regions){const auto source=sources.find(region.id);if(source!=sources.end() && source->second.image.size()==QSize(region.rect.width,region.rect.height))images[region.id]=source->second.image;}
    topology_->setImages(images);overviewTopology_->setImages(images);
    int enabled = 0, regionIndex = 0;
    for (const auto& region : profile.regions) {
        enabled += region.enabled ? 1 : 0;
        const auto source = sources.find(region.id);
        const auto image = source == sources.end() ? QImage{} : source->second.image;
        const auto suffix = live?livePreview_->snapshot().state:image.isNull() ? uiText("未采集") : uiText("最近批次");
        auto* item = new QListWidgetItem(regionLabel(region) + QString("\n%1×%2 · ").arg(region.rect.width).arg(region.rect.height) + suffix + (region.enabled ? QString{} : uiText(" · 未启用")), sourceCards_);
        item->setCheckState(region.enabled ? Qt::Checked : Qt::Unchecked);
        if (!image.isNull()) item->setIcon(QPixmap::fromImage(image.scaled({120, 80}, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        else { QPixmap blank(96, 60); blank.fill(palette().color(QPalette::AlternateBase)); QPainter p(&blank); p.setPen(palette().color(QPalette::Text)); p.drawText(blank.rect(), Qt::AlignCenter, QString::number(region.displayNumber)); item->setIcon(blank); }
        item->setSizeHint({200, 85});
        item->setToolTip(regionLabel(region) + QString(" · %1×%2 px").arg(region.rect.width).arg(region.rect.height));
        item->setData(Qt::UserRole, qs(region.id));
        auto* overviewItem = item->clone(); overviewSources_->addItem(overviewItem);
        const bool linked = std::find(outputSources.begin(), outputSources.end(), region.id) != outputSources.end();
        if (linked) overviewItem->setData(linkedItemRole,true);
        if (region.enabled && candidate.outputMode != "composites") {
            auto* output = new QListWidgetItem(item->icon(), uiText("原图") + " · " + regionLabel(region) + QString("\n%1×%2").arg(region.rect.width).arg(region.rect.height), overviewOutputs_);
            output->setData(Qt::UserRole, qs(region.id)); output->setToolTip(output->text()); output->setSizeHint({210, 85});
            const bool referenced = !overviewOutputKey_.isEmpty() ? qs(region.id) == overviewOutputKey_ : regionIndex == selected;
            if (referenced) output->setData(linkedItemRole,true);
            if (qs(region.id) == overviewOutputKey_) overviewOutputs_->setCurrentItem(output);
        }
        ++regionIndex;
    }
    restoreSelection(sourceCards_,selected,selectedCards,true); topology_->setLayout(profile, displays, selected);
    restoreSelection(overviewSources_,outputSources.empty() ? selected : -1,selectedOverview,outputSources.empty());
    overviewTopology_->setLayout(profile, displays, outputSources.empty() ? selected : -1); overviewTopology_->highlightSources(outputSources);
    appearanceTopology_->setLayout(profile, displays, selected);
    const bool canCapture = !core::validateProfile(profile);
    updateCaptureActions(canCapture);
    if (profile.regions.empty()) {
        sourcePreview_->setImage({}, uiText("请先添加源区域")); preview_->setImage({}, uiText("请先添加源区域"));
        sourceDetail_->setText(uiText("当前配置暂无源区域，可新建或屏幕框选。"));
    }
    if (selected >= 0 && selected < int(profile.regions.size())) {
        const auto& region = profile.regions[std::size_t(selected)]; const auto source = sources.find(region.id);
        const auto image = source == sources.end() ? QImage{} : source->second.image;
        bool stale = false;
        if (sameProfile) { const auto& previous = lastOutcome_->batch.request.profile.regions; const auto old = std::find_if(previous.begin(), previous.end(), [&](const auto& r) { return r.id == region.id; }); stale = old == previous.end() || old->rect != region.rect; }
        const auto state=live?" · "+livePreview_->snapshot().state:image.isNull()?uiText(" · 尚未采集"):stale?uiText(" · 旧批次尺寸，需要重新采集"):uiText(" · 最近批次");
        sourcePreview_->setImage(image, regionLabel(region)+state,{region.rect.width,region.rect.height});preview_->setImage(image,regionLabel(region)+state,{region.rect.width,region.rect.height});
        if(live){const auto issue=livePreview_->snapshot().issues.find(region.id);if(issue!=livePreview_->snapshot().issues.end())sourceDetail_->setText(sourceDetail_->text()+"\n"+issue->second);}
        sourceDetail_->setText(regionLabel(region) + QString(" · (%1, %2) · %3×%4 px").arg(region.rect.x).arg(region.rect.y).arg(region.rect.width).arg(region.rect.height) + (stale ? uiText(" · 旧批次尺寸，需要重新采集") : QString{}));
    }
    if (!error.isEmpty()) sourceDetail_->setText(uiText("当前修改尚未生效：") + error);
    const int originals = candidate.outputMode == "composites" ? 0 : enabled;
    const int compositions = candidate.outputMode == "originals" ? 0 : candidate.selectedSceneIds.size();
    overviewSummary_->setText(uiText("%1 个显示器 · %2 个启用区域 · 下一次输出 %3 张原图 + %4 张拼图").arg(displays.size()).arg(enabled).arg(originals).arg(compositions));
    const QSignalBlocker sceneBlock(scenes_);
    for (int i = 0; i < scenes_->count(); ++i) {
        auto* item = scenes_->item(i); const auto id = item->data(Qt::UserRole).toString().toStdString();
        const auto scene = std::find_if(candidate.scenes.begin(), candidate.scenes.end(), [&](const auto& s) { return s.id == id; });
        if (scene == candidate.scenes.end()) continue;
        const auto rendered = sceneThumbnail(*scene, sources); const auto& thumbnail = rendered.image; item->setIcon(QPixmap::fromImage(thumbnail)); item->setSizeHint({260, 110});
        const bool stale = sameProfile && std::any_of(scene->layers.begin(), scene->layers.end(), [&](const auto& layer) {
            if (!layer.visible) return false;
            const auto current = std::find_if(profile.regions.begin(), profile.regions.end(), [&](const auto& r) { return r.id == layer.sourceRegionId; });
            const auto& oldRegions = lastOutcome_->batch.request.profile.regions;
            const auto previous = std::find_if(oldRegions.begin(), oldRegions.end(), [&](const auto& r) { return r.id == layer.sourceRegionId; });
            return current != profile.regions.end() && previous != oldRegions.end() && current->rect != previous->rect;
        });
        const auto caption = qs(scene->name) + (live?" · "+livePreview_->snapshot().state:!rendered.complete ? uiText(" · 布局预览，待采集") : stale ? uiText(" · 来源尺寸已改动，请重新采集") : sources.empty() ? uiText(" · 场景预览") : uiText(" · 最近批次预览"));
        const QString sceneKey = "scene:" + qs(scene->id);
        if (candidate.outputMode != "originals" && candidate.selectedSceneIds.contains(qs(scene->id))) {
            const bool missing = std::any_of(scene->layers.begin(), scene->layers.end(), [&](const auto& layer) { return layer.visible && std::none_of(profile.regions.begin(), profile.regions.end(), [&](const auto& r) { return r.id == layer.sourceRegionId && r.enabled; }); });
            auto* output = new QListWidgetItem(item->icon(), uiText("拼图") + " · " + qs(scene->name) + (missing ? uiText(" · 源未启用或缺失") : QString{}) + QString("\n%1×%2").arg(scene->width).arg(scene->height), overviewOutputs_);
            output->setData(Qt::UserRole, sceneKey); output->setToolTip(output->text()); output->setSizeHint({210, 100});
            if (sceneKey == overviewOutputKey_) { overviewOutputs_->setCurrentItem(output); scenes_->setCurrentRow(i); preview_->setImage(thumbnail, caption); }
            else if (overviewOutputKey_.isEmpty() && selected >= 0 && std::any_of(scene->layers.begin(), scene->layers.end(), [&](const auto& l) { return l.visible && l.sourceRegionId == profile.regions[std::size_t(selected)].id; })) output->setData(linkedItemRole,true);
        }
        if (item == scenes_->currentItem()) scenePreview_->setImage(thumbnail, caption);
    }
    if (scenes_->count() == 0) scenePreview_->setImage({}, uiText("尚无场景，请先新建拼图"));
    for (int row=0; row<overviewOutputs_->count(); ++row) if(selectedOutputs.contains(overviewOutputs_->item(row)->data(Qt::UserRole).toString())) overviewOutputs_->item(row)->setSelected(true);
    updateObjectActions();
    if (!overviewOutputs_->count()) { auto* empty = new QListWidgetItem(uiText("请在拼图场景中选择输出"), overviewOutputs_); empty->setFlags(Qt::NoItemFlags); }
    for(int i=0;i<3;++i){scrolling[i]->doItemsLayout();scrolling[i]->verticalScrollBar()->setValue(vertical[i]);scrolling[i]->horizontalScrollBar()->setValue(horizontal[i]);}
}
static QJsonObject readManifest(const QString& path)
{
    if (path.isEmpty()) return {};
    QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
static QString artifactPath(const QString& manifest, const QJsonObject& output)
{
    const auto root = QFileInfo(manifest).absolutePath();
    const auto relative = output["relativePath"].toString();
    const auto path = QFileInfo(relative.isEmpty() ? output["path"].toString() : QDir(root).filePath(relative)).canonicalFilePath();
    const auto canonicalRoot = QFileInfo(root).canonicalFilePath() + '/';
    if (path.isEmpty() || !path.startsWith(canonicalRoot, Qt::CaseInsensitive)) return {};
    return path;
}
void MainWindow::refreshBatches()
{
    if (!batchCards_) return;
    const auto previous = selectedBatchManifest_; const QSignalBlocker guard(batchCards_); batchCards_->clear();
    const QDir root(settings_.outputDirectory);
    const auto directories = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    int chosen = 0;
    for (const auto& name : directories) {
        if (batchCards_->count() >= 200) break;
        const auto manifest = root.filePath(name + "/batch.json"); if (!QFileInfo::exists(manifest)) continue;
        const auto metadata = readManifest(manifest); const auto created = QDateTime::fromString(metadata["createdAtUtc"].toString(), Qt::ISODateWithMs);
        const auto profileName = metadata["extensions"].toObject()["profileSnapshot"].toObject()["name"].toString(uiText("历史批次"));
        auto* item = new QListWidgetItem((created.isValid() ? created.toLocalTime().toString("yyyy-MM-dd HH:mm:ss") : uiText("时间未知")) + "\n" + profileName, batchCards_);
        item->setData(Qt::UserRole, manifest); item->setSizeHint({200, 78}); item->setToolTip(name);
        if (manifest == previous) chosen = batchCards_->count() - 1;
    }
    batchCards_->setCurrentRow(batchCards_->count() ? chosen : -1); selectBatch();
}
void MainWindow::selectBatch()
{
    const auto* item = batchCards_->currentItem(); selectedBatchManifest_ = item ? item->data(Qt::UserRole).toString() : QString{};
    const QSignalBlocker guard(batchImages_); batchImages_->clear();
    const auto manifest = readManifest(selectedBatchManifest_);
    auto outputs = manifest["artifacts"].toArray();
    const auto diagnostics = manifest["extensions"].toObject()["outputs"].toArray();
    if (outputs.isEmpty()) outputs = diagnostics;
    else for (const auto& value : diagnostics) if (value.toObject()["kind"] == "IMAGE_EXPORT") outputs.append(value);
    QSet<QString> shown;
    for (const auto& value : outputs) {
        const auto output = value.toObject(); if (output["status"].toString(output["saveStatus"].toString()) != "SAVED") continue;
        const auto path = artifactPath(selectedBatchManifest_, output); const auto suffix = QFileInfo(path).suffix().toLower();
        if (!QStringList{"png", "jpeg", "jpg", "bmp"}.contains(suffix) || shown.contains(path)) continue;
        shown.insert(path);
        auto* image = new QListWidgetItem(QFileInfo(path).fileName(), batchImages_); image->setData(Qt::UserRole, path);
    }
    batchDetail_->setText(item ? item->text().replace('\n', " · ") + QString(" · %1 ").arg(batchImages_->count()) + uiText("个已保存图像") : uiText("当前输出目录暂无已保存批次"));
    batchDetail_->setToolTip(QFileInfo(selectedBatchManifest_).absolutePath());
    batchImages_->setCurrentRow(batchImages_->count() ? 0 : -1); selectBatchImage();
    const auto target=reportTarget(QFileInfo(selectedBatchManifest_).absolutePath(),manifest["createdAtUtc"].toString());
    const bool linked = reportQueue_ && !target.isEmpty() && reportQueue_->hasBatchForDocument(manifest["batchId"].toString(),target);
    appendBatch_->setText(linked ? uiText("本批已加入报告") : uiText("将本批加入报告"));
    appendBatch_->setToolTip(target);
    appendBatch_->setEnabled(reportQueue_ && !selectedBatchManifest_.isEmpty() && !target.isEmpty() && !manifest["batchId"].toString().isEmpty() && batchImages_->count() > 0 && !linked);
    reportCards_->clear(); if (!reportQueue_) return;
    const QMap<QString, QString> states{{"COMMITTED", uiText("已提交")}, {"QUEUED", uiText("排队中")}, {"RUNNING", uiText("正在写入")}, {"UNCERTAIN", uiText("待核对")}, {"FAILED", uiText("失败")}, {"RETRYABLE", uiText("等待重试")}};
    for (const auto& job : reportQueue_->jobs()) {
        const auto state = job["state"].toString(); auto* card = new QListWidgetItem(QFileInfo(job["target"].toObject()["documentPath"].toString()).fileName() + "\n" + states.value(state, state), reportCards_);
        card->setData(Qt::UserRole, job["target"].toObject()["documentPath"].toString());
        card->setSizeHint({200, 74}); card->setToolTip(job["errorMessage"].toString());
        if (job["batchId"] == manifest["batchId"] && batchCards_->currentItem()) card->setIcon(batchCards_->currentItem()->icon());
    }
}
void MainWindow::selectBatchImage()
{
    const auto* item = batchImages_->currentItem(); const auto path = item ? item->data(Qt::UserRole).toString() : QString{};
    const auto image = path.isEmpty() ? QImage{} : smallImage(path);
    batchPreview_->setImage(image, item ? QFileInfo(path).fileName() + (image.isNull() ? uiText(" · 图像无法读取") : QString{}) : uiText("没有可预览的图像"));
    if (batchCards_->currentItem() && !image.isNull()) batchCards_->currentItem()->setIcon(QPixmap::fromImage(image.scaled({96, 60}, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}
}
