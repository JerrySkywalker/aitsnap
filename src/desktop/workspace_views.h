// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/domain.h"
#include <QWidget>
#include <QImage>
#include <QTransform>
#include <functional>
namespace aitsnap {
class DisplayCanvas final : public QWidget {
public:
    explicit DisplayCanvas(QWidget* parent = nullptr);
    void setLayout(core::CaptureProfile profile, std::vector<core::Display> current, int selected);
    void highlightSources(std::vector<std::string> ids);
    int regionAt(const QPoint&) const;
    std::function<void(int)> selected;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    core::CaptureProfile profile_;
    std::vector<core::Display> current_;
    int selected_{-1};
    QTransform transform_;
    std::vector<std::string> highlighted_;
};
class ImagePreview final : public QWidget {
public:
    explicit ImagePreview(QWidget* parent = nullptr);
    void setImage(QImage image, QString caption = {}, QSize placeholder = {640, 400});
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QImage image_;
    QString caption_;
    QSize placeholder_{640, 400};
};
}
