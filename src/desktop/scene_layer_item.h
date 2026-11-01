// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "adapters/scene_render.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <functional>
#include <cmath>
namespace aitsnap {
class LayerItem final : public QGraphicsItem {
public:
    std::function<void()> beginEdit;
    std::function<void(const core::Layer&)> changed;
    std::function<bool()> snapEnabled;
    LayerItem(core::Layer layer, QImage image, bool placeholder = false, const QString& label = {}) : layer_(std::move(layer)), image_(std::move(image)), placeholder_(placeholder)
    {
        setData(0, QString::fromStdString(layer_.id)); setAcceptHoverEvents(true);
        sync(layer_);
        if (!label.isEmpty()) {
            auto* background = new QGraphicsRectItem(this); background->setFlag(ItemIgnoresTransformations);
            background->setPos(4, 4); background->setZValue(1000); background->setAcceptedMouseButtons(Qt::NoButton);
            background->setBrush(QColor(16, 35, 51, 235)); background->setPen(Qt::NoPen);
            auto* text = new QGraphicsSimpleTextItem(label, background); text->setBrush(Qt::white); text->setPos(6, 3); text->setAcceptedMouseButtons(Qt::NoButton);
            background->setRect(text->boundingRect().adjusted(0, 0, 12, 6));
        }
    }
    QRectF boundingRect() const override { return {0, 0, layer_.destination.width, layer_.destination.height}; }
    void sync(const core::Layer& layer)
    {
        updating_ = true; prepareGeometryChange(); layer_ = layer;
        setFlags(ItemIsSelectable | ItemSendsGeometryChanges | (layer.locked ? GraphicsItemFlags{} : GraphicsItemFlags(ItemIsMovable)));
        setPos(layer.destination.x, layer.destination.y); setTransformOriginPoint(layer.destination.width / 2, layer.destination.height / 2);
        setRotation(std::fmod(layer.destination.rotationDeg, 360)); setVisible(layer.visible); updating_ = false; update();
    }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        auto previewLayer = layer_;
        // Placeholder dimensions already represent the effective source/crop; never crop its thumbnail again.
        if (placeholder_) previewLayer.crop = {};
        paintLayerLocal(*p, previewLayer, image_);
        if (isSelected()) {
            const double zoom = scene() && !scene()->views().isEmpty() ? std::abs(scene()->views().front()->transform().m11()) : 1;
            p->setPen(QPen(QColor("#1689ff"), 2 / std::max(zoom, 0.01), Qt::DashLine)); p->setBrush(Qt::NoBrush); p->drawRect(boundingRect());
            if (!layer_.locked) { p->setPen(Qt::NoPen); p->setBrush(QColor("#1689ff")); p->drawRect(handle()); }
        }
    }
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (!updating_ && change == ItemPositionChange && snapEnabled && snapEnabled()) {
            auto p = value.toPointF(); p.setX(std::round(p.x() / 10) * 10); p.setY(std::round(p.y() / 10) * 10); return p;
        }
        if (!updating_ && change == ItemPositionHasChanged) {
            layer_.destination.x = pos().x(); layer_.destination.y = pos().y(); if (changed) changed(layer_);
        }
        return QGraphicsItem::itemChange(change, value);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override
    {
        if (!layer_.locked && e->button() == Qt::LeftButton) {
            if (beginEdit) beginEdit(); resizing_ = isSelected() && handle().contains(e->pos());
            if (resizing_) { e->accept(); return; }
        }
        QGraphicsItem::mousePressEvent(e);
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override
    {
        if (resizing_) {
            prepareGeometryChange(); layer_.destination.width = std::max(1.0, e->pos().x()); layer_.destination.height = std::max(1.0, e->pos().y());
            setTransformOriginPoint(layer_.destination.width / 2, layer_.destination.height / 2);
            if (changed) changed(layer_); update(); e->accept(); return;
        }
        QGraphicsItem::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override { resizing_ = false; QGraphicsItem::mouseReleaseEvent(e); }
    void hoverMoveEvent(QGraphicsSceneHoverEvent* e) override { setCursor(!layer_.locked && isSelected() && handle().contains(e->pos()) ? Qt::SizeFDiagCursor : layer_.locked ? Qt::ArrowCursor : Qt::SizeAllCursor); }
private:
    core::Layer layer_; QImage image_; bool placeholder_{}, updating_{}, resizing_{};
    QRectF handle() const {
        const double zoom = scene() && !scene()->views().isEmpty() ? std::abs(scene()->views().front()->transform().m11()) : 1;
        const double size = 9 / std::max(zoom, 0.01); return {layer_.destination.width - size, layer_.destination.height - size, size, size};
    }
};
} // namespace aitsnap
