// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "json_store.h"
#include <QJsonArray>
namespace aitsnap {
// Pure candidate mutations. The caller confirms dependencies and persists before refreshing UI.
struct RemovalSummary { int removed{}, disabledRules{}; QStringList affectedScenes; };
void selectProfile(Settings&, int index);
core::CaptureProfile emptyProfile(const QString& name);
RemovalSummary removeRegions(Settings&, QJsonArray& rules, const QString& profileId, const QStringList& ids);
RemovalSummary removeScenes(Settings&, QJsonArray& rules, const QStringList& ids);
RemovalSummary removeProfiles(Settings&, QJsonArray& rules, const QStringList& ids, const QString& fallbackName);
// nullptr leaves the separate rules file untouched (including an invalid file held by the host).
bool persistSettingsCommand(const QString& path, const Settings&, const QJsonArray* changedRules, QString& error);
}
