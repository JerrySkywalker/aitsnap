// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "adapters/scene_render.h"
#include <QDialog>
#include <map>
#include <set>
class QCheckBox; class QComboBox; class QDoubleSpinBox; class QGraphicsScene; class QGraphicsView;
class QLabel; class QLineEdit; class QListWidget; class QSpinBox;
namespace aitsnap {
class LayerItem;
class SceneEditor final : public QDialog {
public:
    SceneEditor(core::SceneTemplate, core::CaptureProfile, SceneSources, QWidget* parent = nullptr);
    ~SceneEditor() override;
    core::SceneTemplate sceneTemplate() const { return model_; }
    bool exportDerived(const QString& path, QString& error) const;
private:
    core::SceneTemplate model_;
    core::CaptureProfile profile_;
    SceneSources sources_;
    std::map<std::string, LayerItem*> items_;
    std::set<std::string> selected_;
    std::vector<core::SceneTemplate> undo_, redo_;
    QGraphicsScene* graphics_{}; QGraphicsView* view_{};
    QListWidget *layers_{}, *sourceList_{};
    QLineEdit *name_{}, *background_{}, *group_{}, *pinned_{};
    QSpinBox *canvasWidth_{}, *canvasHeight_{}, *zIndex_{}, *cropX_{}, *cropY_{}, *cropWidth_{}, *cropHeight_{};
    QDoubleSpinBox *x_{}, *y_{}, *width_{}, *height_{}, *rotation_{};
    QCheckBox *visible_{}, *locked_{}, *aspect_{}, *snap_{};
    QComboBox *missing_{}, *source_{};
    QLabel* status_{};
    bool syncing_{}, loadingProperties_{};
    void buildUi(); void rebuild(); void refreshProperties(); void applyProperties(); void pushUndo();
    void selectionFromCanvas(); void selectionFromList(); void syncSelection();
    void updateLayer(const core::Layer&); void addSource(); void duplicateSelection(); void removeSelection();
    void groupSelection(bool group); void preset(int); void geometryAction(int);
    std::vector<std::size_t> selectedIndices() const;
};
} // namespace aitsnap
