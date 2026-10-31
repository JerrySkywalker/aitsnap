// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QJsonObject>
#include <QString>
#include <functional>
#include <memory>
namespace aitsnap {
using HostHandler = std::function<QJsonObject(const QString& action, const QJsonObject& payload, bool preflight)>;
QJsonObject hostError(const QString& code, const QString& message, const QString& status = "ERROR");
QString currentLogonSid(QString& error);
QString defaultPipeName(QString& error);
bool validPipeName(const QString&);
class RequestJournal {
public:
    explicit RequestJournal(QString directory, int capacity = 10000);
    ~RequestJournal();
    QJsonObject dispatch(const QJsonObject& request, const HostHandler&);
    QString diagnostic() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
class LocalPipeServer {
public:
    LocalPipeServer(QString name, QString journalDirectory, HostHandler handler);
    ~LocalPipeServer();
    bool start(QString& error);
    QString name() const;
    QString diagnostic() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
QJsonObject callLocalHost(const QString& name, const QJsonObject& request, QString& error, int timeoutMs = 15000);
} // namespace aitsnap
