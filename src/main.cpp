// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#include "main_window.h"
#include "compat/qt_widgets_compat.h"

#include <QApplication>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    aitsnap::compat::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AITSnap"));
    QApplication::setApplicationVersion(QStringLiteral("0.2.0-preview"));
    QDir().mkpath(QFileInfo(aitsnap::settingsPath()).absolutePath());
    QLockFile lock(QFileInfo(aitsnap::settingsPath()).absolutePath() + "/desktop.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        QMessageBox::information(nullptr, QStringLiteral("AITSnap"), QStringLiteral("AITSnap 已在运行。请从通知区域打开现有窗口。"));
        return 2;
    }
    QApplication::setQuitOnLastWindowClosed(false);

    aitsnap::MainWindow window;
    if (!window.startInTray() && !app.arguments().contains("--tray")) window.show();
    return app.exec();
}
