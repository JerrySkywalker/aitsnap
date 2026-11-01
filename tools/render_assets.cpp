// SPDX-License-Identifier: Apache-2.0
// Offline rendering of repo-authored SVG assets, never user screenshots.
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QImage>
#include <QPainter>
#include <QFileInfo>
#include <QFont>
#include <iostream>
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setFont(QFont("Microsoft YaHei UI", 10));
    if (argc < 3) { std::cerr << "input.svg output.png [square-size]\n"; return 2; }
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if (!svg.isValid()) return 3;
    const auto size = argc > 3 ? QSize(QString(argv[3]).toInt(), QString(argv[3]).toInt()) : svg.defaultSize();
    if (size.isEmpty()) return 4;
    QImage image(size, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    QPainter painter(&image); svg.render(&painter); painter.end();
    return image.save(QString::fromLocal8Bit(argv[2]), "PNG") ? 0 : 5;
}
