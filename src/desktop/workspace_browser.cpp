// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "adapters/win_capture.h"
#include "adapters/report_queue.h"
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
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSet>
#include <QTableWidget>
#include <algorithm>

namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static QImage smallImage(const QString& path)
{
    QImageReader reader(path); auto size = reader.size();
    if (size.width() > 1200 || size.height() > 800) { size.scale({1200, 800}, Qt::KeepAspectRatio); reader.setScaledSize(size); }
    return reader.read();
}
static QImage sceneThumbnail(core::SceneTemplate scene, const SceneSources& sources)
{
    const auto size = QSize(scene.width, scene.height).scaled({720, 420}, Qt::KeepAspectRatio);
    const double scale = double(size.width()) / scene.width;
    scene.width = std::max(1, size.width()); scene.height = std::max(1, size.height());
    for (auto& layer : scene.layers) { layer.destination.x *= scale; layer.destination.y *= scale; layer.destination.width *= scale; layer.destination.height *= scale; layer.missingSourcePolicy = "placeholder"; }
    return renderScene(scene, sources).image;
}
void MainWindow::refreshWorkspace()
{
    if (refreshingWorkspace_ || !sourceCards_ || settings_.profiles.empty()) return;
    QScopedValueRollback<bool> guard(refreshingWorkspace_, true);
    auto candidate = settings_; QString error; if (!collectSettings(candidate, error)) candidate = settings_;
    const auto& profile = candidate.profiles[std::size_t(candidate.activeProfile)];
    if (error.isEmpty()) {
        auto next = candidate; ++next.sequence; core::CaptureRequest sample; sample.profile = profile;
        sample.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
        sample.context = selectedContextSnapshot_.isEmpty() ? core::Context{} : selectedContext_;
        if (selectedContextSnapshot_.isEmpty()) { sample.context.testId = candidate.testId.toStdString(); sample.context.deviceId = candidate.device.toStdString(); }
        sample.context.stationId = candidate.station.toStdString(); sample.context.operatorId = candidate.operatorName.toStdString();
        const auto region = std::find_if(profile.regions.begin(), profile.regions.end(), [](const auto& r) { return r.enabled; });
        if (region != profile.regions.end()) filenamePreview_->setText(expandFilename(next, sample, *region, error) + "." + candidate.format.toLower());
    } else filenamePreview_->setText(error);
    std::vector<core::Display> displays; try { WinCaptureBackend backend; displays = backend.displays(); } catch (...) { /* Unknown layout is rendered explicitly. */ }
    int selected = regions_->currentRow(); if (selected < 0 && !profile.regions.empty()) selected = 0;
    const QSignalBlocker blocker(sourceCards_); sourceCards_->clear();
    const bool sameProfile = lastOutcome_ && lastOutcome_->batch.request.profile.id == profile.id;
    const auto sources = sameProfile ? batchSources(lastOutcome_->batch) : SceneSources{};
    int enabled = 0;
    for (const auto& region : profile.regions) {
        enabled += region.enabled ? 1 : 0;
        const auto source = sources.find(region.id);
        const auto image = source == sources.end() ? QImage{} : source->second.image;
        const auto suffix = image.isNull() ? uiText("未采集") : uiText("最近批次");
        auto* item = new QListWidgetItem(regionLabel(region) + QString("\n%1×%2 · ").arg(region.rect.width).arg(region.rect.height) + suffix + (region.enabled ? QString{} : uiText(" · 未启用")), sourceCards_);
        item->setCheckState(region.enabled ? Qt::Checked : Qt::Unchecked);
        if (!image.isNull()) item->setIcon(QPixmap::fromImage(image.scaled({120, 80}, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        else { QPixmap blank(96, 60); blank.fill(palette().color(QPalette::AlternateBase)); QPainter p(&blank); p.setPen(palette().color(QPalette::Text)); p.drawText(blank.rect(), Qt::AlignCenter, QString::number(region.displayNumber)); item->setIcon(blank); }
        item->setSizeHint({200, 85});
        item->setToolTip(regionLabel(region) + QString(" · %1×%2 px").arg(region.rect.width).arg(region.rect.height));
    }
    sourceCards_->setCurrentRow(selected); topology_->setLayout(profile, displays, selected); overviewTopology_->setLayout(profile, displays, selected);
    appearanceTopology_->setLayout(profile, displays, selected);
    if (selected >= 0 && selected < int(profile.regions.size())) {
        const auto& region = profile.regions[std::size_t(selected)]; const auto source = sources.find(region.id);
        const auto image = source == sources.end() ? QImage{} : source->second.image;
        bool stale = false;
        if (sameProfile) { const auto& previous = lastOutcome_->batch.request.profile.regions; const auto old = std::find_if(previous.begin(), previous.end(), [&](const auto& r) { return r.id == region.id; }); stale = old == previous.end() || old->rect != region.rect; }
        sourcePreview_->setImage(image, regionLabel(region) + (image.isNull() ? uiText(" · 尚未采集") : stale ? uiText(" · 旧批次尺寸，需要重新采集") : uiText(" · 最近批次")), {region.rect.width, region.rect.height});
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
        const auto thumbnail = sceneThumbnail(*scene, sources); item->setIcon(QPixmap::fromImage(thumbnail)); item->setSizeHint({260, 110});
        if (item == scenes_->currentItem()) scenePreview_->setImage(thumbnail, qs(scene->name) + (sources.empty() ? uiText(" · 布局预览，待采集") : uiText(" · 最近批次预览")));
    }
    if (scenes_->count() == 0) scenePreview_->setImage({}, uiText("尚无场景，请先新建拼图"));
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
    reportCards_->clear(); if (!reportQueue_) return;
    const QMap<QString, QString> states{{"COMMITTED", uiText("已提交")}, {"QUEUED", uiText("排队中")}, {"RUNNING", uiText("正在写入")}, {"UNCERTAIN", uiText("待核对")}, {"FAILED", uiText("失败")}, {"RETRYABLE", uiText("等待重试")}};
    for (const auto& job : reportQueue_->jobs()) {
        const auto state = job["state"].toString(); auto* card = new QListWidgetItem(QFileInfo(job["documentPath"].toString()).fileName() + "\n" + states.value(state, state), reportCards_);
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
