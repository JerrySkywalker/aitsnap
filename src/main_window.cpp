// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#include "main_window.h"

#include <QLabel>
#include <QString>
#include <Qt>

namespace aitsnap {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AITSnap"));
    resize(960, 600);

    auto* placeholder = new QLabel(
        QStringLiteral("AITSnap\nProject skeleton - features are not implemented."), this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}

} // namespace aitsnap
