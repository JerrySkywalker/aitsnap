// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/context.h"
#include <QJsonObject>
#include <QString>
#include <QList>
namespace aitsnap {
bool validateCatalog(const QJsonObject&, QString& error);
QJsonObject catalogFromCsv(const QByteArray&, QString& error);
class ContextStore {
public:
    explicit ContextStore(QString directory);
    bool importCatalog(const QString& sourcePath, QString& error);
    core::EventResult ingest(QJsonObject event);
    QList<QJsonObject> catalogs() const { return catalogs_; }
    QList<QJsonObject> events() const;
    QJsonObject snapshot(const QString& source, const QString& eventId, bool redacted = true) const;
    core::Context context(const QString& source, const QString& eventId) const;
    bool exportMetadata(const QString& newPath, bool redacted, QString& error) const;
    QString diagnostic() const { return diagnostic_; }
private:
    QString directory_, diagnostic_;
    core::EventRegistry registry_;
    QList<QJsonObject> catalogs_;
    std::map<core::EventKey, QJsonObject> envelopes_;
    bool parseEvent(QJsonObject, core::ExecutionEvent&, QJsonObject& normalized, QString& error, bool persisted = false) const;
};
} // namespace aitsnap
