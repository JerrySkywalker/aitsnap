// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors
#pragma once

#include <QCoreApplication>
#include <QtGlobal>

namespace aitsnap::compat {

// Call before QApplication. Qt 6 enables high-DPI behavior by default.
inline void configureHighDpi()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
}

} // namespace aitsnap::compat
