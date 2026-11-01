#include "appearance.h"
// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "region_editor.h"
#include "adapters/json_store.h"
#include "adapters/win_capture.h"
#include "core/region_edit.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace aitsnap {
struct EditState {
    core::CaptureProfile profile;
    int selected{-1}; bool dragging{}, closing{};
    core::Point start; core::Rect original; core::RegionDrag mode{};
    std::vector<QWidget*> overlays;
    std::function<void(bool)> finish;
    void refresh() { for (auto* overlay : overlays) overlay->update(); }
};
static core::Point physicalCursor() { POINT p{}; GetCursorPos(&p); return {p.x, p.y}; }
class RegionOverlay final : public QWidget {
public:
    RegionOverlay(std::shared_ptr<EditState> state, core::Display display, QImage background)
        : state_(std::move(state)), display_(std::move(display)), background_(std::move(background))
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose); setFocusPolicy(Qt::StrongFocus); setMouseTracking(true); setCursor(Qt::CrossCursor);
        setWindowTitle(aitsnap::uiText("AITSnap · 屏幕区域编辑 [%1,%2]").arg(display_.rect.x).arg(display_.rect.y));
        auto* bar = new QWidget(this); bar->setStyleSheet("background:#172332; color:white; border-radius:5px;");
        auto* layout = new QHBoxLayout(bar); layout->addWidget(new QLabel(aitsnap::uiText("空白处框选 · 拖动区域/角点 · Tab 切换 · 方向键微调")));
        auto* accept = new QPushButton(aitsnap::uiText("完成 Enter")); auto* cancel = new QPushButton(aitsnap::uiText("取消 Esc"));
        layout->addWidget(accept); layout->addWidget(cancel); bar->move(16, 16); bar->adjustSize();
        connect(accept, &QPushButton::clicked, this, [this] { state_->finish(true); });
        connect(cancel, &QPushButton::clicked, this, [this] { state_->finish(false); });
    }
    void showOnDisplay()
    {
        show();
        SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST, display_.rect.x, display_.rect.y,
            display_.rect.width, display_.rect.height, SWP_SHOWWINDOW);
    }
protected:
    bool focusNextPrevChild(bool next) override
    {
        if (state_->profile.regions.empty()) return true;
        state_->selected = (state_->selected + (next ? 1 : int(state_->profile.regions.size()) - 1)) % int(state_->profile.regions.size());
        state_->refresh(); return true;
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this); painter.drawImage(rect(), background_); painter.fillRect(rect(), QColor(0, 0, 0, 110));
        const double sx = double(width()) / display_.rect.width, sy = double(height()) / display_.rect.height;
        for (int i = 0; i < int(state_->profile.regions.size()); ++i) {
            const auto& r = state_->profile.regions[std::size_t(i)];
            const QRectF area((double(r.rect.x) - display_.rect.x) * sx, (double(r.rect.y) - display_.rect.y) * sy, r.rect.width * sx, r.rect.height * sy);
            const bool selected = i == state_->selected;
            painter.setPen(QPen(selected ? QColor("#46D8FF") : QColor("#9BE8BA"), 2, r.enabled ? Qt::SolidLine : Qt::DashLine));
            painter.setBrush(QColor(40, 140, 200, selected ? 55 : 20)); painter.drawRect(area);
            const auto label = regionLabel(r) + QString(" · %1×%2").arg(r.rect.width).arg(r.rect.height);
            const double labelWidth = std::min(double(width()), double(std::max(240, painter.fontMetrics().horizontalAdvance(label) + 12)));
            const double labelX = std::clamp(area.x(), 0.0, std::max(0.0, width() - labelWidth));
            double labelY = std::clamp(area.y() - 26, 0.0, double(std::max(0, height() - 26)));
            if (labelY < 70 && labelX < 1000) labelY = std::clamp(std::max(76.0, area.y() + 4), 0.0, double(std::max(0, height() - 26)));
            const QRectF labelRect(labelX, labelY, labelWidth, 26);
            painter.fillRect(labelRect, QColor(16, 35, 51, 235)); painter.setPen(Qt::white);
            painter.drawText(labelRect.adjusted(6, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, painter.fontMetrics().elidedText(label, Qt::ElideRight, int(labelWidth) - 10));
            if (selected) { painter.setBrush(QColor("#46D8FF")); painter.setPen(Qt::NoPen); const double h = 5;
                for (auto point : {area.topLeft(), area.topRight(), area.bottomLeft(), area.bottomRight()}) painter.drawRect(QRectF(point.x() - h, point.y() - h, 2 * h, 2 * h)); }
        }
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || state_->closing) return;
        auto p = physicalCursor(); state_->start = p; state_->mode = core::RegionDrag::Move;
        bool corner = false;
        if (state_->selected >= 0) {
            const auto r = state_->profile.regions[std::size_t(state_->selected)].rect;
            const core::Point points[]{{r.x, r.y}, {r.x + r.width, r.y}, {r.x, r.y + r.height}, {r.x + r.width, r.y + r.height}};
            const core::RegionDrag modes[]{core::RegionDrag::TopLeft, core::RegionDrag::TopRight, core::RegionDrag::BottomLeft, core::RegionDrag::BottomRight};
            const int radius = std::max(8, int(10 * devicePixelRatioF()));
            for (int i = 0; i < 4; ++i) if (std::abs(std::int64_t(p.x) - points[i].x) <= radius && std::abs(std::int64_t(p.y) - points[i].y) <= radius) { state_->mode = modes[i]; corner = true; break; }
        }
        if (!corner) {
            state_->selected = -1;
            for (int i = int(state_->profile.regions.size()) - 1; i >= 0; --i)
                if (core::contains(state_->profile.regions[std::size_t(i)].rect, p)) { state_->selected = i; break; }
            if (state_->selected < 0) {
                state_->mode = core::RegionDrag::Create; state_->selected = int(state_->profile.regions.size());
                state_->profile.regions.push_back({QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
                    "区域 " + std::to_string(state_->profile.nextRegionNumber), {p.x, p.y, 1, 1}, true, state_->profile.nextRegionNumber++});
            }
        }
        state_->original = state_->profile.regions[std::size_t(state_->selected)].rect;
        state_->dragging = true; grabMouse(); state_->refresh();
    }
    void mouseMoveEvent(QMouseEvent*) override
    {
        if (!state_->dragging || state_->closing) return;
        state_->profile.regions[std::size_t(state_->selected)].rect = core::draggedRect(state_->original, state_->start, physicalCursor(), state_->mode); state_->refresh();
    }
    void mouseReleaseEvent(QMouseEvent*) override { state_->dragging = false; releaseMouse(); state_->refresh(); }
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape) { state_->finish(false); return; }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { state_->finish(true); return; }
        if (e->key() == Qt::Key_Tab && !state_->profile.regions.empty()) { state_->selected = (state_->selected + 1) % int(state_->profile.regions.size()); state_->refresh(); return; }
        if (state_->selected >= 0) {
            auto& r = state_->profile.regions[std::size_t(state_->selected)]; core::Point delta{};
            const int step = e->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
            if (e->key() == Qt::Key_Left) delta.x = -step; else if (e->key() == Qt::Key_Right) delta.x = step;
            else if (e->key() == Qt::Key_Up) delta.y = -step; else if (e->key() == Qt::Key_Down) delta.y = step; else { QWidget::keyPressEvent(e); return; }
            r.rect = core::draggedRect(r.rect, {}, delta, core::RegionDrag::Move); state_->refresh(); return;
        }
        QWidget::keyPressEvent(e);
    }
    void closeEvent(QCloseEvent* e) override;
private:
    std::shared_ptr<EditState> state_; core::Display display_; QImage background_;
};
void RegionOverlay::closeEvent(QCloseEvent* e)
{
    if (!state_->closing) state_->finish(false);
    QWidget::closeEvent(e);
}
void openRegionEditor(QWidget* owner, const core::CaptureProfile& profile, std::function<void(bool, core::CaptureProfile)> finished)
{
    auto state = std::make_shared<EditState>(); state->profile = profile; core::numberRegions(state->profile);
    WinCaptureBackend backend; const auto displays = backend.displays();
    std::vector<QImage> backgrounds;
    for (const auto& d : displays) {
        auto frame = backend.acquire(d);
        if (frame.error) throw std::runtime_error(frame.error.message);
        backgrounds.push_back(toQImage(frame.image));
    }
    state->profile.calibratedDisplays = displays;
    // Weak reference avoids a closure/state cycle; overlay ownership lasts to finish.
    std::weak_ptr<EditState> weak = state;
    QPointer<QWidget> ownerGuard(owner);
    state->finish = [weak, finished, ownerGuard](bool accepted) {
        auto current = weak.lock(); if (!current || current->closing) return;
        if (accepted && core::validateProfile(current->profile)) return;
        current->closing = true;
        if (QWidget::mouseGrabber()) QWidget::mouseGrabber()->releaseMouse();
        for (auto* overlay : current->overlays) { overlay->hide(); overlay->deleteLater(); }
        DwmFlush();
        if (accepted) ++current->profile.revision;
        if (ownerGuard) QTimer::singleShot(180, ownerGuard, [current, finished, accepted] { finished(accepted, current->profile); });
    };
    for (std::size_t i = 0; i < displays.size(); ++i) {
        auto* overlay = new RegionOverlay(state, displays[i], backgrounds[i]); state->overlays.push_back(overlay);
        QObject::connect(owner, &QObject::destroyed, overlay, [overlay, state] { state->closing = true; overlay->hide(); overlay->deleteLater(); });
        overlay->showOnDisplay();
    }
    if (!state->overlays.empty()) { state->overlays.front()->activateWindow(); state->overlays.front()->setFocus(); }
    else throw std::runtime_error("No interactive displays");
}
} // namespace aitsnap
