// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/domain.h"
#include <QImage>
#include <QJsonObject>
#include <QString>
#include <QStringList>
namespace aitsnap {
struct ReportOptions {
    bool enabled{};
    QString documentPath, itemMode{"both"}, layout{"inline"};
};
struct Settings {
    std::vector<core::CaptureProfile> profiles;
    int activeProfile{};
    QString outputDirectory, format{"PNG"}, filenameTemplate{"{datetime}_{sequence}_{region}"};
    QString saveHotkey{"Ctrl+Alt+S"}, copyHotkey{"Ctrl+Alt+C"}, bothHotkey{"Ctrl+Alt+A"};
    QString clipboardRegion, testId, device, station, operatorName;
    qint64 sequence{};
    bool startInTray{};
    QString language{"system"}, colorMode{"system"};
    std::vector<core::SceneTemplate> scenes;
    QStringList selectedSceneIds;
    QString outputMode{"originals"};
    ReportOptions report;
};
QString settingsPath();
Settings defaultSettings();
bool loadSettings(const QString& path, Settings& settings, QString& error);
bool saveSettings(const QString& path, const Settings&, QString& error);
QJsonObject profileJson(const core::CaptureProfile&);
bool profileFromJson(const QJsonObject&, core::CaptureProfile&, QString& error);
QImage toQImage(const core::Image&);
QString expandFilename(const Settings&, const core::CaptureRequest&, const core::Region&, QString& error);
bool saveImageNew(const QImage&, const QString& path, const QByteArray& format, QString& error);
bool saveImageNumbered(const QImage&, const QString& directory, const QString& stem, const QByteArray& format, QString& path, QString& error);
bool saveBytesNew(const QByteArray&, const QString& path, QString& error);
QJsonObject batchJson(const core::CaptureBatch&);
} // namespace aitsnap
