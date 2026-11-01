// SPDX-License-Identifier: Apache-2.0
#include "workspace_views.h"
#include "appearance.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

namespace aitsnap {
static QRectF physicalRect(core::Rect r) { return {double(r.x), double(r.y), double(r.width), double(r.height)}; }
DisplayCanvas::DisplayCanvas(QWidget* parent) : QWidget(parent) { setMinimumSize(360, 240); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); }
void DisplayCanvas::setLayout(core::CaptureProfile p, std::vector<core::Display> current, int selection)
{ profile_ = std::move(p); current_ = std::move(current); selected_ = selection; update(); }
void DisplayCanvas::highlightSources(std::vector<std::string> ids) { highlighted_ = std::move(ids); update(); }
void DisplayCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.fillRect(this->rect(), palette().color(QPalette::Base));
    p.setPen(palette().color(QPalette::WindowText));
    const bool matches = !current_.empty() && core::sameTopology(profile_.calibratedDisplays, current_);
    p.drawText(QRect(16, 10, width() - 32, 44), Qt::AlignLeft | Qt::TextWordWrap,
        matches ? uiText("布局匹配 · 点击区域查看来源") : uiText("布局有变化或尚未确认 · 核对区域后确认布局"));
    auto displays = current_;
    for (const auto& saved : profile_.calibratedDisplays)
        if (std::none_of(displays.begin(), displays.end(), [&](const auto& d) { return d.id == saved.id; })) displays.push_back(saved);
    std::sort(displays.begin(), displays.end(), [](const auto& a, const auto& b) { return a.rect.x == b.rect.x ? a.rect.y < b.rect.y : a.rect.x < b.rect.x; });
    QRectF bounds;
    for (const auto& d : displays) bounds = bounds.united(physicalRect(d.rect));
    if (bounds.isEmpty()) { p.drawText(this->rect(), Qt::AlignCenter, uiText("暂无显示器布局")); transform_ = {}; return; }
    const auto area = QRectF(24, 65, std::max(1, width() - 48), std::max(1, height() - 100));
    const double scale = std::min(area.width() / bounds.width(), area.height() / bounds.height());
    transform_ = QTransform(); transform_.translate(area.center().x() - bounds.width() * scale / 2, area.center().y() - bounds.height() * scale / 2);
    transform_.scale(scale, scale); transform_.translate(-bounds.left(), -bounds.top());
    int number = 0;
    for (const auto& d : displays) {
        ++number; const bool connected = std::any_of(current_.begin(), current_.end(), [&](const auto& actual) { return actual.id == d.id; });
        const auto frame = transform_.mapRect(physicalRect(d.rect));
        p.setPen(QPen(palette().color(connected ? QPalette::Mid : QPalette::Link), 2, connected ? Qt::SolidLine : Qt::DashLine));
        p.setBrush(palette().color(QPalette::AlternateBase)); p.drawRoundedRect(frame, 7, 7);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(frame.adjusted(8, 5, -5, -5), Qt::AlignLeft | Qt::AlignTop, uiText("显示器 %1").arg(number) + QString(" · %1×%2").arg(d.rect.width).arg(d.rect.height) + (connected ? QString{} : uiText(" · 已断开")));
    }
    std::vector<QRectF> badges;
    for (int i = 0; i < int(profile_.regions.size()); ++i) {
        const auto& r = profile_.regions[std::size_t(i)]; const auto frame = transform_.mapRect(physicalRect(r.rect));
        const bool highlighted = i == selected_ || std::find(highlighted_.begin(), highlighted_.end(), r.id) != highlighted_.end();
        QColor accent = palette().color(QPalette::Link); QColor fill = accent; fill.setAlpha(highlighted ? 100 : 35);
        p.setPen(QPen(accent, highlighted ? 3 : 1, r.enabled ? Qt::SolidLine : Qt::DashLine)); p.setBrush(fill); p.drawRoundedRect(frame, 3, 3);
        auto label = regionLabel(r); if (!r.enabled) label += uiText(" · 未启用");
        const int labelWidth = std::min(width() - 16, std::max(56, p.fontMetrics().horizontalAdvance(label) + 14));
        QRectF badge(std::clamp(frame.left(), 8.0, double(std::max(8, width() - labelWidth - 8))), std::clamp(frame.top() + 3, 54.0, double(std::max(54, height() - 30))), labelWidth, 24);
        const auto overlaps = [&](const QRectF& candidate) { return std::any_of(badges.begin(), badges.end(), [&](const auto& used) { return candidate.adjusted(-2, -2, 2, 2).intersects(used); }); };
        if (overlaps(badge)) {
            const auto start = badge;
            for (int step = 1; step < height() / 28; ++step) {
                auto candidate = start.translated(0, step * 28);
                if (candidate.bottom() <= height() - 6 && !overlaps(candidate)) { badge = candidate; break; }
                candidate = start.translated(0, -step * 28);
                if (candidate.top() >= 54 && !overlaps(candidate)) { badge = candidate; break; }
            }
            if (badge != start) { p.setPen(QPen(accent, 1)); p.drawLine(frame.center(), badge.center()); }
        }
        badges.push_back(badge);
        p.fillRect(badge, palette().color(QPalette::Base)); p.setPen(palette().color(QPalette::Text));
        p.drawText(badge.adjusted(5, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(label, Qt::ElideRight, labelWidth - 10));
    }
}
void DisplayCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !selected) return;
    const auto point = transform_.inverted().map(QPointF(event->pos()));
    for (int i = int(profile_.regions.size()) - 1; i >= 0; --i) if (physicalRect(profile_.regions[std::size_t(i)].rect).contains(point)) { selected(i); return; }
}
ImagePreview::ImagePreview(QWidget* parent) : QWidget(parent) { setMinimumSize(220, 160); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); }
void ImagePreview::setImage(QImage image, QString caption, QSize placeholder)
{ image_ = std::move(image); caption_ = std::move(caption); placeholder_ = placeholder; update(); }
void ImagePreview::paintEvent(QPaintEvent*)
{
    QPainter p(this); p.setRenderHint(QPainter::SmoothPixmapTransform); p.fillRect(rect(), palette().color(QPalette::Base));
    const QRect area = rect().adjusted(16, 16, -16, -58); auto size = image_.isNull() ? placeholder_ : image_.size();
    if (size.isEmpty() || area.isEmpty()) return;
    size.scale(area.size(), Qt::KeepAspectRatio); const QRect target(area.center() - QPoint(size.width() / 2, size.height() / 2), size);
    if (!image_.isNull()) p.drawImage(target, image_);
    else { p.setPen(QPen(palette().color(QPalette::Mid), 2, Qt::DashLine)); p.setBrush(palette().color(QPalette::AlternateBase)); p.drawRect(target); p.setPen(palette().color(QPalette::Text)); p.drawText(target.adjusted(5, 5, -5, -5), Qt::AlignCenter | Qt::TextWordWrap, uiText("未采集 · 等比布局占位")); }
    p.setPen(palette().color(QPalette::Text)); p.drawText(QRect(16, height() - 48, width() - 32, 42), Qt::AlignCenter | Qt::TextWordWrap, caption_);
}
}
