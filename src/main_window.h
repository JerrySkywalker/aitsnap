// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#pragma once

#include <QMainWindow>
#include "adapters/capture_files.h"
#include "adapters/hotkeys.h"
#include <functional>
#include <memory>

class QCheckBox; class QComboBox; class QLabel; class QLineEdit;
class QMenu; class QSystemTrayIcon; class QTableWidget;

namespace aitsnap {

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr, QString configurationPath = {});
    ~MainWindow() override;
    void triggerCapture(int mode);
    bool startInTray() const { return settings_.startInTray; }
    std::function<void(std::shared_ptr<CaptureOutcome>)> captureFinished;
protected:
    void closeEvent(QCloseEvent*) override;
private:
    Settings settings_;
    QString configurationPath_;
    Hotkeys hotkeys_;
    QSystemTrayIcon* tray_{};
    QMenu* profileMenu_{};
    QComboBox *profiles_{}, *format_{}, *clipboard_{};
    QLineEdit *profileName_{}, *directory_{}, *filename_{}, *saveKey_{}, *copyKey_{}, *bothKey_{};
    QLineEdit *testId_{}, *device_{}, *station_{}, *operator_{};
    QCheckBox* startTray_{};
    QTableWidget* regions_{};
    QLabel *status_{}, *preview_{};
    bool busy_{}, paused_{}, configurationBlocked_{};
    void buildUi(); void setupTray(); void loadProfile();
    bool collectSettings(Settings&, QString& error);
    bool applySettings(); void addRegion(); void rebuildProfileMenu();
    void setStatus(const QString&, bool error = false);
    void completeCapture(const std::shared_ptr<CaptureOutcome>&, const Settings&, int mode);
};

} // namespace aitsnap
