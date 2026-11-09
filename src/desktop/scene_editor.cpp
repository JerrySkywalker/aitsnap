// SPDX-License-Identifier: Apache-2.0
#include "appearance.h"
#include "scene_editor.h"
#include <QAction>
#include <QMenu>
#include "scene_layer_item.h"
#include "core/scene_geometry.h"
#include "adapters/json_store.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <limits>
#include <cmath>

namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static std::string uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }

class SceneCanvasView final : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;
    std::function<void(double)> zoomChanged;

    void setCanvas(QSize size, const QColor& background)
    {
        const auto centre = mapToScene(viewport()->rect().center());
        canvas_ = QRectF(QPointF{}, QSizeF(size));
        background_ = background;
        content_ = canvas_.united(scene()->itemsBoundingRect());
        updateScrollArea();
        centerOn(centre);
        viewport()->update();
    }

    void fitCanvas()
    {
        if (canvas_.isEmpty()) return;
        const auto size = viewport()->size() - QSize(36, 36);
        setZoom(std::min(double(size.width()) / canvas_.width(), double(size.height()) / canvas_.height()));
        centerOn(canvas_.center());
    }

    void zoomBy(double factor) { zoomAt(viewport()->rect().center(), transform().m11() * factor); }
    void actualSize() { zoomAt(viewport()->rect().center(), 1); }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        const double steps = event->angleDelta().y() != 0 ? event->angleDelta().y() / 120.0 : event->pixelDelta().y() / 60.0;
        if (steps == 0) { QGraphicsView::wheelEvent(event); return; }
        zoomAt(event->position().toPoint(), transform().m11() * std::pow(1.25, std::clamp(steps, -4.0, 4.0)));
        event->accept();
    }

    void drawBackground(QPainter* painter, const QRectF& exposed) override
    {
        painter->fillRect(exposed, palette().mid());
        const auto visible = exposed.intersected(canvas_);
        if (visible.isEmpty()) return;
        if (background_.alpha() < 255) {
            painter->save();
            painter->setClipRect(visible);
            painter->fillRect(visible, palette().base());
            const double cell = 12 / transform().m11();
            const int x0 = int(std::floor(visible.left() / cell)), y0 = int(std::floor(visible.top() / cell));
            const int x1 = int(std::ceil(visible.right() / cell)), y1 = int(std::ceil(visible.bottom() / cell));
            for (int y = y0; y < y1; ++y)
                for (int x = x0 + ((x0 + y) & 1); x < x1; x += 2)
                    painter->fillRect(QRectF(x * cell, y * cell, cell, cell), palette().midlight());
            painter->restore();
        }
        painter->fillRect(visible, background_);
    }

    void drawForeground(QPainter* painter, const QRectF&) override
    {
        painter->save();
        QPen border(palette().highlight().color(), 2);
        border.setCosmetic(true);
        painter->setPen(border);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(canvas_);
        painter->restore();
    }

private:
    QRectF canvas_, content_;
    QColor background_;

    void updateScrollArea()
    {
        // Leave room to hold the point under the pointer even below the fit zoom.
        const auto scale = transform().m11();
        const double marginX = viewport()->width() / scale, marginY = viewport()->height() / scale;
        setSceneRect(content_.adjusted(-marginX, -marginY, marginX, marginY));
    }

    void setZoom(double scale)
    {
        scale = std::clamp(scale, 0.00001, 16.0);
        setTransform(QTransform::fromScale(scale, scale));
        updateScrollArea();
        if (zoomChanged) zoomChanged(scale);
    }

    void zoomAt(QPoint pixel, double scale)
    {
        const auto point = mapToScene(pixel);
        setZoom(scale);
        const auto delta = point - mapToScene(pixel);
        centerOn(mapToScene(viewport()->rect().center()) + delta);
    }
};

static QString geometryMessage(const core::Error& error)
{
    if (error.code == "SCENE_CANVAS_SIZE")
        return uiText("内容需要 %1 px，超出画布上限。请调整图层或明确设置固定画布。").arg(qs(error.message));
    if (error.code == "SCENE_EMPTY") return uiText("没有可用于计算画布的可见图层。");
    return qs(error.message);
}

QSize SceneEditor::sourcePixelSize(const core::Region& region) const
{
    const auto source = sources_.find(region.id);
    if (source != sources_.end() && !source->second.image.isNull()) return source->second.image.size();
    return {region.rect.width, region.rect.height};
}

SceneEditor::SceneEditor(core::SceneTemplate scene, core::CaptureProfile profile, SceneSources sources, QWidget* parent, bool initializeNative)
    : QDialog(parent), model_(std::move(scene)), profile_(std::move(profile)), sources_(std::move(sources))
{
    core::numberRegions(profile_);
    if (initializeNative) {
        for (const auto& region : profile_.regions) {
            if (!region.enabled) continue;
            const auto size = sourcePixelSize(region);
            core::Layer layer;
            layer.id = "layer-" + region.id;
            layer.sourceRegionId = region.id;
            layer.zIndex = int(model_.layers.size());
            layer.destination = {0, 0, double(size.width()), double(size.height()), 0};
            model_.layers.push_back(std::move(layer));
        }
        if (!model_.layers.empty()) {
            core::arrangeLayers(model_, core::LayoutPreset::Grid);
            if (const auto error = core::fitCanvasToContent(model_)) pendingCanvasError_ = geometryMessage(error);
        }
    }
    setWindowTitle(aitsnap::uiText("AITSnap · 自由拼图")); resize(1360, 860); setMinimumSize(1040, 680);
    buildUi(); rebuild(); QTimer::singleShot(0, this, [this] { view_->fitCanvas(); });
}
SceneEditor::~SceneEditor()
{
    // Qt destroys the child scene after C++ members (including selected_) are gone.
    // Removing selected items then emits selectionChanged; stop these callbacks now.
    disconnect(graphics_, nullptr, this, nullptr);
    disconnect(layers_, nullptr, this, nullptr);
}
void SceneEditor::buildUi()
{
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(18, 16, 18, 14);
    auto* heading = new QLabel(aitsnap::uiText("自由拼图 · 场景与截图源独立")); heading->setStyleSheet("font-size:22px; font-weight:600;"); outer->addWidget(heading);
    auto* toolbar = new QHBoxLayout;
    auto button = [this, toolbar](const QString& text, const std::function<void()>& action, const char* objectName = "") {
        auto* b = new QPushButton(text); b->setObjectName(objectName); toolbar->addWidget(b); connect(b, &QPushButton::clicked, this, action); return b;
    };
    auto* presets = new QComboBox; presets->setObjectName("sceneLayoutPreset"); presets->addItems({aitsnap::uiText("行排列"), aitsnap::uiText("列排列"), aitsnap::uiText("网格"), aitsnap::uiText("主图 + 辅图")}); toolbar->addWidget(presets);
    button(aitsnap::uiText("应用预设"), [this, presets] { preset(presets->currentIndex()); }, "applyPreset");
    const auto restore = [this](auto& from, auto& to) {
        if (from.empty()) return;
        to.push_back({model_, pendingCanvasError_});
        model_ = std::move(from.back().model);
        pendingCanvasError_ = std::move(from.back().pendingCanvasError);
        from.pop_back();
        rebuild();
    };
    button(aitsnap::uiText("撤销"), [this, restore] { restore(undo_, redo_); }, "undoScene");
    button(aitsnap::uiText("重做"), [this, restore] { restore(redo_, undo_); });
    snap_ = new QCheckBox(aitsnap::uiText("10 px 吸附")); toolbar->addWidget(snap_);
    button(aitsnap::uiText("适合窗口"), [this] { view_->fitCanvas(); }, "fitSceneView");
    button("−", [this] { view_->zoomBy(0.8); }); button("+", [this] { view_->zoomBy(1.25); });
    button("100%", [this] { view_->actualSize(); }, "sceneViewActualSize");
    zoomLevel_ = new QLabel; zoomLevel_->setObjectName("sceneZoomLevel"); toolbar->addWidget(zoomLevel_);
    toolbar->addStretch();
    canvasSize_ = new QLabel; canvasSize_->setObjectName("sceneCanvasSize"); toolbar->addWidget(canvasSize_);
    outer->addLayout(toolbar);
    auto* split = new QSplitter(Qt::Horizontal); outer->addWidget(split, 1);
    auto* left = new QWidget; auto* leftLayout = new QVBoxLayout(left); leftLayout->setContentsMargins(0, 0, 8, 0);
    leftLayout->addWidget(new QLabel(aitsnap::uiText("可复用源区域"))); sourceList_ = new QListWidget; sourceList_->setObjectName("sceneSources");
    for (const auto& r : profile_.regions) {
        const auto size = sourcePixelSize(r);
        auto* item = new QListWidgetItem(regionLabel(r) + QString(" · %1×%2").arg(size.width()).arg(size.height()), sourceList_); item->setData(Qt::UserRole, qs(r.id));
    }
    sourceList_->setMaximumHeight(170); if (sourceList_->count()) sourceList_->setCurrentRow(0); leftLayout->addWidget(sourceList_);
    auto* add = new QPushButton(aitsnap::uiText("添加到画布")); add->setObjectName("addSceneSource"); leftLayout->addWidget(add); connect(add, &QPushButton::clicked, this, [this] { addSource(); });
    leftLayout->addWidget(new QLabel(aitsnap::uiText("图层 · 复选框控制可见性"))); layers_ = new QListWidget; layers_->setObjectName("sceneLayers"); layers_->setSelectionMode(QAbstractItemView::ExtendedSelection); leftLayout->addWidget(layers_, 1);
    auto* layerButtons = new QHBoxLayout; auto* duplicate = new QPushButton(aitsnap::uiText("复制源")); duplicate->setObjectName("duplicateLayer"); auto* remove = new QPushButton(aitsnap::uiText("删除图层"));
    layerButtons->addWidget(duplicate); layerButtons->addWidget(remove); leftLayout->addLayout(layerButtons);
    auto* groups = new QHBoxLayout; auto* group = new QPushButton(aitsnap::uiText("组合")); group->setObjectName("groupLayers"); auto* ungroup = new QPushButton(aitsnap::uiText("取消组合")); groups->addWidget(group); groups->addWidget(ungroup); leftLayout->addLayout(groups);
    connect(duplicate, &QPushButton::clicked, this, [this] { duplicateSelection(); }); connect(remove, &QPushButton::clicked, this, [this] { removeSelection(); });
    connect(group, &QPushButton::clicked, this, [this] { groupSelection(true); }); connect(ungroup, &QPushButton::clicked, this, [this] { groupSelection(false); });
    split->addWidget(left);
    graphics_ = new QGraphicsScene(this); view_ = new SceneCanvasView(graphics_); view_->setObjectName("sceneCanvas"); view_->setDragMode(QGraphicsView::RubberBandDrag);
    view_->setTransformationAnchor(QGraphicsView::NoAnchor);
    view_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    view_->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform); split->addWidget(view_);
    view_->zoomChanged = [this](double scale) { zoomLevel_->setText(QString::number(scale * 100, 'g', 3) + "%"); };
    view_->zoomChanged(1);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setMinimumWidth(245); auto* properties = new QWidget; auto* form = new QFormLayout(properties); form->setContentsMargins(12, 0, 4, 0);
    name_ = new QLineEdit(qs(model_.name)); form->addRow(aitsnap::uiText("场景名称"), name_);
    auto integerSpin = [form](const QString& label, int min, int max) { auto* s = new QSpinBox; s->setRange(min, max); form->addRow(label, s); return s; };
    canvasWidth_ = integerSpin(aitsnap::uiText("画布宽 px"), 1, 1000000); canvasHeight_ = integerSpin(aitsnap::uiText("画布高 px"), 1, 1000000);
    canvasWidth_->setObjectName("sceneCanvasWidth"); canvasHeight_->setObjectName("sceneCanvasHeight");
    canvasWidth_->setValue(model_.width); canvasHeight_->setValue(model_.height); background_ = new QLineEdit(qs(model_.background)); form->addRow(aitsnap::uiText("背景 #RRGGBB[AA]"), background_);
    auto* applyCanvas = new QPushButton(aitsnap::uiText("设置固定画布")); applyCanvas->setObjectName("applySceneCanvas"); form->addRow(applyCanvas);
    auto* fitContent = new QPushButton(aitsnap::uiText("画布适合内容")); fitContent->setObjectName("fitSceneContent"); form->addRow(fitContent);
    connect(fitContent, &QPushButton::clicked, this, [this] { fitCanvas(); });
    connect(applyCanvas, &QPushButton::clicked, this, [this] {
        auto candidate = model_; candidate.name = name_->text().trimmed().toStdString(); candidate.width = canvasWidth_->value(); candidate.height = canvasHeight_->value(); candidate.background = background_->text().toStdString();
        if (const auto e = core::validateScene(candidate); e) { status_->setText(qs(e.message)); return; }
        pushUndo(); model_ = candidate; pendingCanvasError_.clear(); rebuild();
        status_->setText(uiText("已应用固定画布尺寸。"));
    });
    form->addRow(new QLabel(aitsnap::uiText("选中图层属性（目标框像素）")));
    source_ = new QComboBox; form->addRow(aitsnap::uiText("源区域"), source_);
    auto numeric = [form](const QString& label, double min, double max, const char* objectName) { auto* s = new QDoubleSpinBox; s->setObjectName(objectName); s->setDecimals(3); s->setRange(min, max); form->addRow(label, s); return s; };
    x_ = numeric("X", -1000000, 1000000, "layerX"); y_ = numeric("Y", -1000000, 1000000, "layerY");
    width_ = numeric(aitsnap::uiText("宽"), 0.001, 1000000, "layerWidth"); height_ = numeric(aitsnap::uiText("高"), 0.001, 1000000, "layerHeight"); rotation_ = numeric(aitsnap::uiText("旋转 °"), -36000, 36000, "layerRotation");
    zIndex_ = integerSpin(aitsnap::uiText("层级 Z"), -1000000, std::numeric_limits<int>::max());
    visible_ = new QCheckBox(aitsnap::uiText("可见")); locked_ = new QCheckBox(aitsnap::uiText("锁定移动")); locked_->setObjectName("layerLocked"); aspect_ = new QCheckBox(aitsnap::uiText("等比适配（不拉伸图片）")); aspect_->setToolTip(aitsnap::uiText("保持源图或裁剪后的宽高比，在目标框内居中，可能留白。取消后图片会拉伸铺满目标框。")); form->addRow(visible_); form->addRow(locked_); form->addRow(aspect_);
    cropX_ = integerSpin(aitsnap::uiText("裁剪 X"), 0, 32768); cropY_ = integerSpin(aitsnap::uiText("裁剪 Y"), 0, 32768); cropWidth_ = integerSpin(aitsnap::uiText("裁剪宽 (0=完整)"), 0, 32768); cropHeight_ = integerSpin(aitsnap::uiText("裁剪高 (0=完整)"), 0, 32768);
    missing_ = new QComboBox; missing_->setObjectName("missingSourcePolicy"); missing_->addItem(uiText("停止导出"), "fail"); missing_->addItem(uiText("显示占位"), "placeholder"); missing_->addItem(uiText("跳过图层"), "skip"); form->addRow(aitsnap::uiText("缺源策略"), missing_);
    group_ = new QLineEdit; group_->setReadOnly(true); form->addRow(uiText("组合状态"), group_);
    pinned_ = new QLineEdit; pinned_->setReadOnly(true); form->addRow(aitsnap::uiText("固定源图"), pinned_);
    auto* relink = new QPushButton(aitsnap::uiText("改用当前批次源")); form->addRow(relink);
    connect(relink, &QPushButton::clicked, this, [this] { pushUndo(); for (auto& layer : model_.layers) if (selected_.count(layer.id)) layer.sourceArtifactId.clear(); rebuild(); });
    auto* alignRow = new QWidget; auto* alignment = new QHBoxLayout(alignRow); alignment->setContentsMargins(0, 0, 0, 0);
    const QStringList alignmentNames{aitsnap::uiText("左"), aitsnap::uiText("右"), aitsnap::uiText("顶"), aitsnap::uiText("底"), aitsnap::uiText("中X"), aitsnap::uiText("中Y")};
    for (int i = 0; i < alignmentNames.size(); ++i) { auto* b = new QPushButton(alignmentNames[i]); alignment->addWidget(b); connect(b, &QPushButton::clicked, this, [this, i] { geometryAction(i); }); }
    form->addRow(aitsnap::uiText("目标框对齐"), alignRow);
    auto* distribution = new QWidget; auto* spacing = new QHBoxLayout(distribution); spacing->setContentsMargins(0, 0, 0, 0);
    auto* horizontal = new QPushButton(aitsnap::uiText("水平分布")); auto* vertical = new QPushButton(aitsnap::uiText("垂直分布")); spacing->addWidget(horizontal); spacing->addWidget(vertical); form->addRow(distribution);
    connect(horizontal, &QPushButton::clicked, this, [this] { geometryAction(6); }); connect(vertical, &QPushButton::clicked, this, [this] { geometryAction(7); });
    scroll->setWidget(properties); split->addWidget(scroll); split->setSizes({235, 820, 270});
    status_ = new QLabel(sources_.empty() ? aitsnap::uiText("当前为布局占位预览，未伪造截图。先截图或打开历史批次即可看到真实源图。") : aitsnap::uiText("源图只读；拖动图层，右下角缩放，右侧精确裁剪/旋转。导出会生成新文件。")); status_->setObjectName("sceneEditorStatus"); status_->setWordWrap(true); outer->addWidget(status_);
    auto* bottom = new QHBoxLayout; auto* exportImage = new QPushButton(aitsnap::uiText("导出当前派生 PNG")); bottom->addWidget(exportImage); bottom->addStretch(); auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText(aitsnap::uiText("保存场景")); buttons->button(QDialogButtonBox::Save)->setObjectName("saveScene"); bottom->addWidget(buttons); outer->addLayout(bottom);
    connect(exportImage, &QPushButton::clicked, this, [this] { const auto path = QFileDialog::getSaveFileName(this, aitsnap::uiText("保存新的派生拼图"), {}, "PNG (*.png)"); if (path.isEmpty()) return; QString e; status_->setText(exportDerived(path, e) ? aitsnap::uiText("派生 PNG 已保存，源图未改动。") : e); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!pendingCanvasError_.isEmpty()) { status_->setText(pendingCanvasError_); return; }
        auto candidate = model_; candidate.name = name_->text().trimmed().toStdString(); candidate.width = canvasWidth_->value(); candidate.height = canvasHeight_->value(); candidate.background = background_->text().toStdString();
        if (const auto e = core::validateScene(candidate); e) { status_->setText(qs(e.message)); return; }
        candidate.revisionCreatedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); ++candidate.revision; model_ = candidate; accept();
    });
    connect(graphics_, &QGraphicsScene::selectionChanged, this, [this] { selectionFromCanvas(); });
    connect(layers_, &QListWidget::itemSelectionChanged, this, [this] { selectionFromList(); });
    connect(layers_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (syncing_) return; const auto id = item->data(Qt::UserRole).toString().toStdString();
        for (auto& layer : model_.layers) if (layer.id == id) { pushUndo(); layer.visible = item->checkState() == Qt::Checked; rebuild(); break; }
    });
    for (auto* value : {x_, y_, width_, height_, rotation_}) connect(value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] { applyProperties(); });
    for (auto* value : {zIndex_, cropX_, cropY_, cropWidth_, cropHeight_}) connect(value, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { applyProperties(); });
    for (auto* value : {visible_, locked_, aspect_}) connect(value, &QCheckBox::toggled, this, [this] { applyProperties(); });
    connect(missing_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { applyProperties(); });
    connect(source_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { applyProperties(); });
    auto layerAction=[this](const QString& title,const QString& id,const std::function<void()>& operation,const QKeySequence& key=QKeySequence{}) {
        auto* action=new QAction(title,this);action->setObjectName(id);layers_->addAction(action);view_->addAction(action);
        if(!key.isEmpty()){action->setShortcut(key);action->setShortcutContext(Qt::WidgetWithChildrenShortcut);}connect(action,&QAction::triggered,this,operation);return action;
    };
    layerAction(uiText("复制"),"layer.duplicate",[this]{duplicateSelection();},QKeySequence(int(Qt::CTRL)|int(Qt::Key_D)));
    layerAction(uiText("删除图层"),"layer.delete",[this]{removeSelection();},QKeySequence::Delete);
    layerAction(uiText("组合"),"layer.group",[this]{groupSelection(true);});layerAction(uiText("取消组合"),"layer.ungroup",[this]{groupSelection(false);});
    for(bool visible:{true,false})layerAction(visible?uiText("显示图层"):uiText("隐藏图层"),visible?"layer.show":"layer.hide",[this,visible]{if(selected_.empty())return;pushUndo();for(auto& l:model_.layers)if(selected_.count(l.id))l.visible=visible;rebuild();});
    for(bool locked:{true,false})layerAction(locked?uiText("锁定图层"):uiText("解锁图层"),locked?"layer.lock":"layer.unlock",[this,locked]{if(selected_.empty())return;pushUndo();for(auto& l:model_.layers)if(selected_.count(l.id))l.locked=locked;rebuild();});
    for(bool front:{true,false})layerAction(front?uiText("置顶"):uiText("置底"),front?"layer.front":"layer.back",[this,front]{
        if(selected_.empty())return;pushUndo();std::vector<core::Layer*> ordered;for(auto& l:model_.layers)ordered.push_back(&l);
        std::stable_sort(ordered.begin(),ordered.end(),[](auto* a,auto* b){return a->zIndex<b->zIndex;});
        std::stable_partition(ordered.begin(),ordered.end(),[&](auto* l){return front?selected_.count(l->id)==0:selected_.count(l->id)>0;});
        for(int i=0;i<int(ordered.size());++i)ordered[std::size_t(i)]->zIndex=i;rebuild();
    });
    auto popup=[this](const QPoint& global){QMenu menu(this);menu.addSection(uiText("已选 %1 项").arg(selected_.size()));for(auto* action:layers_->actions()){action->setEnabled(!selected_.empty());menu.addAction(action);}menu.exec(global);};
    layers_->setContextMenuPolicy(Qt::CustomContextMenu);connect(layers_,&QWidget::customContextMenuRequested,this,[this,popup](const QPoint& pos){auto* item=layers_->itemAt(pos);if(item&&!item->isSelected()){layers_->clearSelection();item->setSelected(true);}else if(!item)layers_->clearSelection();popup(layers_->viewport()->mapToGlobal(pos));});
    view_->setContextMenuPolicy(Qt::CustomContextMenu);connect(view_,&QWidget::customContextMenuRequested,this,[this,popup](const QPoint& pos){auto* hit=graphics_->itemAt(view_->mapToScene(pos),view_->viewportTransform());while(hit&&!dynamic_cast<LayerItem*>(hit))hit=hit->parentItem();if(hit&&!hit->isSelected()){graphics_->clearSelection();hit->setSelected(true);}else if(!hit)graphics_->clearSelection();popup(view_->viewport()->mapToGlobal(pos));});
    auto* sourceAdd=new QAction(uiText("添加到画布"),sourceList_);sourceAdd->setObjectName("source.addToCanvas");sourceList_->addAction(sourceAdd);sourceList_->setContextMenuPolicy(Qt::ActionsContextMenu);connect(sourceAdd,&QAction::triggered,this,[this]{addSource();});

}
void SceneEditor::pushUndo()
{
    undo_.push_back({model_, pendingCanvasError_}); if (undo_.size() > 100) undo_.erase(undo_.begin()); redo_.clear();
}
std::vector<std::size_t> SceneEditor::selectedIndices() const
{
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < model_.layers.size(); ++i) if (selected_.count(model_.layers[i].id)) indices.push_back(i);
    return indices;
}
void SceneEditor::setPreviewSources(SceneSources sources)
{
    sources_=std::move(sources);
    for(const auto& layer:model_.layers){const auto item=items_.find(layer.id);if(item==items_.end())continue;
        const auto source=sources_.find(layer.sourceRegionId);const bool actual=source!=sources_.end() && !source->second.image.isNull() && (layer.sourceArtifactId.empty() || layer.sourceArtifactId==source->second.artifactId);
        const auto region=std::find_if(profile_.regions.begin(),profile_.regions.end(),[&](const auto& r){return r.id==layer.sourceRegionId;});QImage image;
        if(actual)image=source->second.image;
        else{QSize size=layer.crop.valid()?QSize(layer.crop.width,layer.crop.height):region!=profile_.regions.end()?QSize(region->rect.width,region->rect.height):QSize(qRound(layer.destination.width),qRound(layer.destination.height));size=size.scaled({640,400},Qt::KeepAspectRatio);image=QImage(std::max(1,size.width()),std::max(1,size.height()),QImage::Format_RGB32);image.fill(QColor("#33485d"));}
        item->second->setPreviewImage(image,!actual,(region==profile_.regions.end()?uiText("缺失源"):regionLabel(*region))+(actual?QString{}:uiText(" · 实时缺帧")));
    }
}
void SceneEditor::rebuild()
{
    syncing_ = true; graphics_->clear(); items_.clear(); layers_->clear();
    const QSize canvas(model_.width, model_.height);
    if (displayedCanvas_ != canvas) {
        displayedCanvas_ = canvas;
        canvasWidth_->setValue(canvas.width());
        canvasHeight_->setValue(canvas.height());
    }
    if (!pendingCanvasError_.isEmpty()) status_->setText(pendingCanvasError_);
    graphics_->setSceneRect(0, 0, model_.width, model_.height);
    graphics_->setBackgroundBrush(sceneColour(model_.background));
    auto ordered = model_.layers;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.zIndex == b.zIndex ? a.id < b.id : a.zIndex < b.zIndex; });
    int rank = 0;
    for (const auto& layer : ordered) {
        const auto source = sources_.find(layer.sourceRegionId);
        const bool actual = source != sources_.end() && !source->second.image.isNull() && (layer.sourceArtifactId.empty() || layer.sourceArtifactId == source->second.artifactId);
        QImage preview;
        if (actual) preview = source->second.image;
        else {
            const auto region = std::find_if(profile_.regions.begin(), profile_.regions.end(), [&](const auto& r) { return r.id == layer.sourceRegionId; });
            const QSize geometry = layer.crop.valid() ? QSize(layer.crop.width, layer.crop.height)
                : region != profile_.regions.end() ? QSize(region->rect.width, region->rect.height)
                : QSize(qRound(layer.destination.width), qRound(layer.destination.height));
            auto thumbnail = geometry.scaled(QSize(640, 400), Qt::KeepAspectRatio);
            thumbnail.setWidth(std::max(1, thumbnail.width())); thumbnail.setHeight(std::max(1, thumbnail.height()));
            preview = QImage(thumbnail, QImage::Format_RGB32); preview.fill(QColor("#33485d"));
        }
        const auto sourceRegion = std::find_if(profile_.regions.begin(), profile_.regions.end(), [&](const auto& r) { return r.id == layer.sourceRegionId; });
        const auto sourceLabel = sourceRegion == profile_.regions.end() ? uiText("缺失源") : regionLabel(*sourceRegion);
        auto* item = new LayerItem(layer, preview, !actual, sourceLabel + (actual ? QString{} : uiText(" · 未采集"))); item->setZValue(rank++); graphics_->addItem(item); items_.emplace(layer.id, item);
        item->beginEdit = [this] { pushUndo(); }; item->snapEnabled = [this] { return snap_->isChecked() && selected_.size() <= 1; };
        item->changed = [this](const auto& changed) { updateLayer(changed); };
        item->setSelected(selected_.count(layer.id) > 0);
        auto region = std::find_if(profile_.regions.begin(), profile_.regions.end(), [&](const auto& r) { return r.id == layer.sourceRegionId; });
        const auto label = region == profile_.regions.end() ? uiText("缺失源") : regionLabel(*region);
        auto* entry = new QListWidgetItem(QString("Z%1 · ").arg(layer.zIndex) + label + (layer.groupId.empty() ? "" : aitsnap::uiText(" · 组")), layers_);
        entry->setData(Qt::UserRole, qs(layer.id)); entry->setFlags(entry->flags() | Qt::ItemIsUserCheckable); entry->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked); entry->setSelected(selected_.count(layer.id) > 0);
    }
    view_->setCanvas(canvas, sceneColour(model_.background));
    canvasSize_->setText(QString("%1 × %2 px").arg(canvas.width()).arg(canvas.height()));
    syncing_ = false; refreshProperties();
}
void SceneEditor::selectionFromCanvas()
{
    if (syncing_) return;
    selected_.clear(); for (auto* item : graphics_->selectedItems()) selected_.insert(item->data(0).toString().toStdString()); syncSelection();
}
void SceneEditor::selectionFromList()
{
    if (syncing_) return;
    selected_.clear(); for (auto* item : layers_->selectedItems()) selected_.insert(item->data(Qt::UserRole).toString().toStdString()); syncSelection();
}
void SceneEditor::syncSelection()
{
    syncing_ = true;
    for (const auto& item : items_) item.second->setSelected(selected_.count(item.first) > 0);
    for (int i = 0; i < layers_->count(); ++i) layers_->item(i)->setSelected(selected_.count(layers_->item(i)->data(Qt::UserRole).toString().toStdString()) > 0);
    syncing_ = false; refreshProperties();
}
void SceneEditor::refreshProperties()
{
    loadingProperties_ = true;
    for(auto* action:layers_->actions()) action->setEnabled(!selected_.empty());
    const auto indices = selectedIndices(); const bool one = indices.size() == 1;
    const std::vector<QWidget*> widgets{source_, x_, y_, width_, height_, rotation_, zIndex_, visible_, locked_, aspect_, cropX_, cropY_, cropWidth_, cropHeight_, missing_, group_, pinned_};
    for (auto* widget : widgets) widget->setEnabled(one);
    if (one) {
        const auto& layer = model_.layers[indices.front()]; const auto& d = layer.destination;
        source_->clear(); for (const auto& r : profile_.regions) source_->addItem(regionLabel(r), qs(r.id));
        int index = source_->findData(qs(layer.sourceRegionId));
        if (index < 0) { source_->addItem(uiText("缺失源") + aitsnap::uiText("（未在配置中）"), qs(layer.sourceRegionId)); index = source_->count() - 1; }
        source_->setCurrentIndex(index); x_->setValue(d.x); y_->setValue(d.y); width_->setValue(d.width); height_->setValue(d.height); rotation_->setValue(std::fmod(d.rotationDeg, 360));
        zIndex_->setValue(layer.zIndex); visible_->setChecked(layer.visible); locked_->setChecked(layer.locked); aspect_->setChecked(layer.preserveAspect);
        cropX_->setValue(layer.crop.x); cropY_->setValue(layer.crop.y); cropWidth_->setValue(layer.crop.width); cropHeight_->setValue(layer.crop.height);
        missing_->setCurrentIndex(std::max(0, missing_->findData(qs(layer.missingSourcePolicy)))); group_->setText(layer.groupId.empty() ? uiText("未分组") : uiText("已组合")); pinned_->setText(layer.sourceArtifactId.empty() ? uiText("跟随当前批次") : uiText("已固定历史源图"));
    }
    loadingProperties_ = false;
}
void SceneEditor::applyProperties()
{
    if (loadingProperties_ || syncing_) return;
    const auto indices = selectedIndices(); if (indices.size() != 1) return;
    if ((cropWidth_->value() == 0) != (cropHeight_->value() == 0)) { status_->setText(aitsnap::uiText("请填写完整裁剪宽高，或均为 0 使用完整源图。")); return; }
    const auto& previous = model_.layers[indices.front()];
    auto layer = previous;
    const auto source = source_->currentData().toString().toStdString();
    layer.destination = {x_->value(), y_->value(), width_->value(), height_->value(), rotation_->value()};
    layer.zIndex = zIndex_->value(); layer.visible = visible_->isChecked(); layer.locked = locked_->isChecked(); layer.preserveAspect = aspect_->isChecked();
    layer.crop = cropWidth_->value() > 0 ? core::Rect{cropX_->value(), cropY_->value(), cropWidth_->value(), cropHeight_->value()} : core::Rect{};
    layer.missingSourcePolicy = missing_->currentData().toString().toStdString();
    if (source != layer.sourceRegionId) { layer.sourceRegionId = source; layer.sourceArtifactId.clear(); layer.crop = {}; }
    if (!previous.locked && (layer.crop != previous.crop || layer.sourceRegionId != previous.sourceRegionId)) {
        const auto pixelSize = [this](const core::Layer& item) {
            if (item.crop.valid()) return QSize(item.crop.width, item.crop.height);
            const auto region = std::find_if(profile_.regions.begin(), profile_.regions.end(), [&](const auto& r) { return r.id == item.sourceRegionId; });
            return region == profile_.regions.end() ? QSize{} : sourcePixelSize(*region);
        };
        const auto before = pixelSize(previous);
        const auto after = pixelSize(layer);
        if (after.isValid() && layer.destination.width == before.width() && layer.destination.height == before.height()) {
            layer.destination.width = after.width();
            layer.destination.height = after.height();
        }
    }
    pushUndo(); updateLayer(layer); rebuild();
}
void SceneEditor::updateLayer(const core::Layer& changed)
{
    if (syncing_) return;
    auto candidate = model_; const auto found = std::find_if(candidate.layers.begin(), candidate.layers.end(), [&](const auto& l) { return l.id == changed.id; });
    if (found == candidate.layers.end()) return;
    const auto previous = *found; const double dx = changed.destination.x - previous.destination.x, dy = changed.destination.y - previous.destination.y;
    if (!previous.groupId.empty() && (dx != 0 || dy != 0) && std::any_of(candidate.layers.begin(), candidate.layers.end(), [&](const auto& layer) { return layer.groupId == previous.groupId && layer.locked && layer.id != changed.id; })) { status_->setText(aitsnap::uiText("组合中有锁定图层，请先解除锁定。")); items_[previous.id]->sync(previous); return; }
    if (previous.locked && changed.locked && (dx != 0 || dy != 0 || changed.destination.width != previous.destination.width || changed.destination.height != previous.destination.height || changed.destination.rotationDeg != previous.destination.rotationDeg)) { status_->setText(aitsnap::uiText("请先解除图层锁定。")); items_[previous.id]->sync(previous); return; }
    *found = changed;
    if (!changed.groupId.empty() && (dx != 0 || dy != 0)) for (auto& layer : candidate.layers)
        if (layer.id != changed.id && layer.groupId == changed.groupId && !layer.locked && !selected_.count(layer.id)) { layer.destination.x += dx; layer.destination.y += dy; }
    if (const auto error = core::validateScene(candidate); error) { status_->setText(qs(error.message)); items_[previous.id]->sync(previous); return; }
    model_ = candidate; syncing_ = true;
    for (const auto& layer : model_.layers) if (layer.id != changed.id && items_.count(layer.id)) items_[layer.id]->sync(layer);
    syncing_ = false; refreshProperties();
}
void SceneEditor::addSource()
{
    auto* source = sourceList_->currentItem(); if (!source) return;
    const auto id = source->data(Qt::UserRole).toString().toStdString();
    const auto region = std::find_if(profile_.regions.begin(), profile_.regions.end(), [&](const auto& row) { return row.id == id; });
    if (region == profile_.regions.end()) return;
    pushUndo();
    core::Layer layer; layer.id = uuid(); layer.sourceRegionId = id;
    int top = 0; for (const auto& l : model_.layers) top = std::max(top, l.zIndex == std::numeric_limits<int>::max() ? l.zIndex : l.zIndex + 1); layer.zIndex = top;
    const auto size = sourcePixelSize(*region);
    layer.destination = {24.0 + (model_.layers.size() % 8) * 24, 24.0 + (model_.layers.size() % 8) * 24, double(size.width()), double(size.height()), 0};
    model_.layers.push_back(layer); selected_ = {layer.id}; rebuild();
}
void SceneEditor::duplicateSelection()
{
    const auto indices = selectedIndices(); if (indices.empty()) return; pushUndo();
    std::vector<core::Layer> copies; std::map<std::string, std::string> groups; int top = 0; for (const auto& l : model_.layers) top = std::max(top, l.zIndex == std::numeric_limits<int>::max() ? l.zIndex : l.zIndex + 1);
    selected_.clear(); for (auto i : indices) { auto layer = model_.layers[i]; layer.id = uuid(); layer.destination.x += 24; layer.destination.y += 24; layer.zIndex = top; if (top < std::numeric_limits<int>::max()) ++top;
        if (!layer.groupId.empty()) { if (indices.size() == 1) layer.groupId.clear(); else { if (!groups.count(layer.groupId)) groups.emplace(layer.groupId, uuid()); layer.groupId = groups[layer.groupId]; } }
        selected_.insert(layer.id); copies.push_back(layer); }
    model_.layers.insert(model_.layers.end(), copies.begin(), copies.end()); rebuild();
}
void SceneEditor::removeSelection()
{
    if (selected_.empty()) return; pushUndo();
    model_.layers.erase(std::remove_if(model_.layers.begin(), model_.layers.end(), [&](const auto& layer) { return selected_.count(layer.id) > 0; }), model_.layers.end()); selected_.clear(); rebuild();
}
void SceneEditor::groupSelection(bool group)
{
    if (selected_.size() < (group ? 2u : 1u)) { status_->setText(aitsnap::uiText("组合需要至少选中两个图层。")); return; }
    pushUndo(); const auto groupId = group ? uuid() : std::string{};
    for (auto& layer : model_.layers) if (selected_.count(layer.id)) layer.groupId = groupId;
    rebuild();
}
void SceneEditor::preset(int kind)
{
    auto replacement = model_;
    core::arrangeLayers(replacement, core::LayoutPreset(kind));
    if (const auto error = core::validateScene(replacement); error) { status_->setText(qs(error.message)); return; }
    pushUndo(); model_ = std::move(replacement); rebuild();
}

void SceneEditor::fitCanvas()
{
    auto candidate = model_;
    if (const auto error = core::fitCanvasToContent(candidate)) {
        status_->setText(geometryMessage(error));
        return;
    }
    pushUndo();
    model_ = std::move(candidate);
    pendingCanvasError_.clear();
    rebuild();
    status_->setText(uiText("画布已按可见图层边界调整。"));
}
void SceneEditor::geometryAction(int action)
{
    auto indices = selectedIndices(); if (indices.empty()) return; pushUndo();
    std::map<std::string, std::vector<std::size_t>> units;
    for (auto i : indices) { const auto& layer = model_.layers[i]; const auto key = layer.groupId.empty() ? "layer:" + layer.id : "group:" + layer.groupId;
        if (units.count(key)) continue;
        for (std::size_t j = 0; j < model_.layers.size(); ++j) if (j == i || (!layer.groupId.empty() && model_.layers[j].groupId == layer.groupId)) units[key].push_back(j); }
    core::SceneTemplate proxy = model_; proxy.layers.clear(); std::vector<std::vector<std::size_t>> members;
    for (const auto& unit : units) {
        core::Layer box; box.id = unit.first; double left = 1e9, top = 1e9, right = -1e9, bottom = -1e9;
        for (auto i : unit.second) { const auto& layer = model_.layers[i]; const auto& d = layer.destination; left = std::min(left, d.x); top = std::min(top, d.y); right = std::max(right, d.x + d.width); bottom = std::max(bottom, d.y + d.height); box.locked = box.locked || layer.locked; }
        box.destination = {left, top, right - left, bottom - top, 0}; proxy.layers.push_back(box); members.push_back(unit.second);
    }
    const auto before = proxy; std::vector<std::size_t> selected;
    for (std::size_t i = 0; i < proxy.layers.size(); ++i) selected.push_back(i);
    if (action < 6) core::alignLayers(proxy, selected, core::Align(action)); else core::distributeLayers(proxy, selected, action == 6);
    for (std::size_t i = 0; i < members.size(); ++i) { const double dx = proxy.layers[i].destination.x - before.layers[i].destination.x, dy = proxy.layers[i].destination.y - before.layers[i].destination.y;
        for (auto member : members[i]) { model_.layers[member].destination.x += dx; model_.layers[member].destination.y += dy; } }
    rebuild();
}
bool SceneEditor::exportDerived(const QString& path, QString& error) const
{
    if (!pendingCanvasError_.isEmpty()) { error = pendingCanvasError_; return false; }
    if (QFile::exists(path + ".derivation.json")) { error = aitsnap::uiText("派生清单已存在，请选择新文件名。 "); return false; }
    const auto rendered = renderScene(model_, sources_);
    if (rendered.error) { error = qs(rendered.error.message); return false; }
    if (!rendered.complete) { error = aitsnap::uiText("源图不完整：") + rendered.warnings.join("; "); return false; }
    QJsonArray originals; std::set<std::string> used;
    for (const auto& layer : model_.layers) if (layer.visible && used.insert(layer.sourceRegionId).second) {
        const auto found = sources_.find(layer.sourceRegionId);
        if (found == sources_.end() || found->second.path.isEmpty()) { error = aitsnap::uiText("原图尚未保存。请先截图并保存，或打开历史批次。空白场景可直接导出。 "); return false; }
        QFile original(found->second.path); QCryptographicHash digest(QCryptographicHash::Sha256);
        if (!original.open(QIODevice::ReadOnly) || !digest.addData(&original)) { error = aitsnap::uiText("无法核对原图哈希。 "); return false; }
        const auto sha = digest.result().toHex();
        if (!found->second.sha256.empty() && sha.toStdString() != found->second.sha256) { error = aitsnap::uiText("原图已变化，未导出拼图。 "); return false; }
        originals.append(QJsonObject{{"artifactId", qs(found->second.artifactId)}, {"relativePath", QFileInfo(path).absoluteDir().relativeFilePath(found->second.path)}, {"sha256", QString::fromLatin1(sha)}});
    }
    auto snapshot = model_; snapshot.id = "derived-" + uuid(); snapshot.revision = 1; snapshot.revisionCreatedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
    QJsonObject receipt{{"schemaVersion", 1}, {"kind", "DERIVED_COMPOSITE"}, {"parentSceneId", qs(model_.id)}, {"parentSceneRevision", model_.revision},
        {"sceneSnapshot", sceneJson(snapshot)}, {"createdAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"file", QFileInfo(path).fileName()}, {"originals", originals}};
    if (!saveImageNew(rendered.image, path, "PNG", error)) return false;
    if (!saveBytesNew(QJsonDocument(receipt).toJson(), path + ".derivation.json", error)) { error = aitsnap::uiText("PNG 已写出，但派生清单保存失败：") + error; return false; }
    return true;
}
} // namespace aitsnap
