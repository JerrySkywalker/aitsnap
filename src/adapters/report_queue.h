// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "capture_files.h"
#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QLockFile>
#include <functional>
#include <memory>
namespace aitsnap {
QString reportFingerprint(const QJsonObject& job);
bool validateReportJob(const QJsonObject& job, QString& error);
bool readReportJob(const QString& path, QJsonObject& job, QString& error);
bool storeReportJob(const QString& path, const QJsonObject& job, QString& error);
QJsonObject makeReportJob(const CaptureOutcome&, const ReportOptions&, const QString& mode, QString& error);

// One process owns the durable dispatcher. Office is always a child helper;
// screenshot workers never wait for it. Uncertain jobs require marker inspection.
class ReportQueue final : public QObject {
public:
    ReportQueue(QString directory, QString helper, QObject* parent = nullptr, int timeoutMs = 60000);
    ~ReportQueue() override;
    QString resolveTarget(const ReportOptions&, const QString& captureRoot, const QDateTime& batchTime, bool reserve, QString& error) const;
    bool enqueue(const CaptureOutcome&, const ReportOptions&, QString& error);
    bool enqueueHistorical(const QString& manifestPath, const ReportOptions&, QString& error);
    bool hasBatchForDocument(const QString& batchId, const QString& documentPath) const;
    bool inspectMarker(const QString& jobId, QString& error);
    bool retryFailed(const QString& jobId, QString& error);
    QList<QJsonObject> jobs() const;
    QString diagnostic() const { return diagnostic_; }
    std::function<void()> changed;
    void pump();
private:
    QString directory_, helper_, currentPath_, resultPath_;
    mutable QString diagnostic_;
    QJsonObject current_;
    QProcess process_;
    QTimer tick_, deadline_;
    QElapsedTimer workerClock_;
    std::unique_ptr<QLockFile> lock_;
    bool ready_{}, inspecting_{};
    int timeoutMs_;
    void launch(const QString& path, QJsonObject job, bool inspect);
    void finish(bool failedToStart = false);
    bool persist(QJsonObject& job, const QString& path);
};
} // namespace aitsnap
