// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#pragma once

#include <QMainWindow>
#include <QJsonArray>
#include "adapters/capture_files.h"
#include "adapters/hotkeys.h"
#include <functional>
#include <memory>

class QCheckBox; class QComboBox; class QLabel; class QLineEdit;
class QMenu; class QSystemTrayIcon; class QTableWidget; class QListWidget;
class QStackedWidget; class QTabWidget;
class QPushButton;

namespace aitsnap {
class ReportQueue;
class ContextStore;
class LocalPipeServer;
class DisplayCanvas; class ImagePreview;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr, QString configurationPath = {}, QString pipeName = {});
    ~MainWindow() override;
    void triggerCapture(int mode);
    bool startInTray() const { return settings_.startInTray; }
    std::function<void(std::shared_ptr<CaptureOutcome>)> captureFinished;
protected:
    void closeEvent(QCloseEvent*) override;
    void changeEvent(QEvent*) override;
private:
    Settings settings_;
    QString configurationPath_;
    Hotkeys hotkeys_;
    QSystemTrayIcon* tray_{};
    QMenu* profileMenu_{};
    QMenu* sceneMenu_{};
    QComboBox *profiles_{}, *format_{}, *clipboard_{};
    QComboBox *language_{}, *colorMode_{};
    QLineEdit *profileName_{}, *directory_{}, *filename_{}, *saveKey_{}, *copyKey_{}, *bothKey_{};
    QLineEdit *testId_{}, *device_{}, *station_{}, *operator_{};
    QCheckBox* startTray_{};
    QTableWidget* regions_{};
    QListWidget* scenes_{};
    QComboBox* outputMode_{};
    ReportQueue* reportQueue_{};
    QCheckBox* reportEnabled_{};
    QLineEdit* reportPath_{};
    QComboBox *reportMode_{}, *reportLayout_{}, *reportTargetMode_{};
    QLabel *reportStatus_{}, *reportTargetStatus_{};
    std::unique_ptr<ContextStore> contextStore_;
    core::Context selectedContext_;
    QJsonObject selectedContextSnapshot_;
    QLabel* contextStatus_{};
    std::unique_ptr<LocalPipeServer> localHost_;
    QJsonArray eventRules_;
    QLabel* hostStatus_{};
    QString currentRequestId_;
    std::shared_ptr<CaptureOutcome> lastOutcome_;
    QLabel *status_{}, *overviewSummary_{}, *sourceDetail_{}, *batchDetail_{}, *filenamePreview_{};
    ImagePreview *preview_{}, *sourcePreview_{}, *scenePreview_{}, *batchPreview_{};
    DisplayCanvas *topology_{}, *overviewTopology_{}, *appearanceTopology_{};
    QListWidget *navigation_{}, *sourceCards_{}, *batchCards_{}, *batchImages_{}, *reportCards_{};
    QListWidget *overviewSources_{}, *overviewOutputs_{};
    QString overviewOutputKey_;
    QStackedWidget* pages_{};
    QTabWidget* sourceViews_{};
    QString selectedBatchManifest_;
    QPushButton *appendLast_{}, *appendBatch_{};
    void refreshWorkspace(); void refreshBatches(); void selectBatch(); void selectBatchImage();
    bool busy_{}, paused_{}, configurationBlocked_{}, editingRegions_{}, editingScene_{}, refreshingWorkspace_{};
    void buildUi(); void setupTray(); void loadProfile();
    bool collectSettings(Settings&, QString& error);
    bool applySettings(); void addRegion(); void rebuildProfileMenu();
    void setStatus(const QString&, bool error = false);
    void editRegions();
    void editScene(bool createNew, bool historical = false, const QString& manifestOverride = {});
    void rebuildScenes(); void rebuildSceneMenu(); void importScene(); void exportScene();
    void completeCapture(const std::shared_ptr<CaptureOutcome>&, const Settings&, int mode);
    void showReportQueue(); void refreshReportStatus();
    ReportOptions selectedReportOptions() const;
    QString reportTarget(const QString& batchDirectory = {}, const QString& issuedAtUtc = {}, QString* error = nullptr) const;
    void showContextStore();
    QString startCapture(int mode, const QJsonObject& options = {}, const QJsonObject& eventSnapshot = {});
    void startHost(QString pipeName); void editEventRules();
    enum class ObjectKind { Region, Scene, Profile, Output };
    bool managingObjects_{}, objectMenuOpen_{};
    void setupObjectActions(); void updateObjectActions(); void showProfileManager();
    void objectAction(ObjectKind, const QString& operation, const QStringList& ids = {});
    bool commitObjects(const Settings&, const QJsonArray&);
    QJsonObject hostRequest(const QString&, const QJsonObject&, bool preflight);
    bool validateHostOptions(const QJsonObject&, QString& error) const;
    bool validateEventRule(const QJsonObject&, QString& error) const;
};

} // namespace aitsnap
