// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#include "main_window.h"
#include "compat/qt_widgets_compat.h"

#include <QApplication>
#include <QString>

int main(int argc, char* argv[])
{
    aitsnap::compat::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AITSnap"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    aitsnap::MainWindow window;
    window.show();
    return app.exec();
}
