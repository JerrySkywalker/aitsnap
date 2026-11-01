// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/domain.h"
#include <QJsonObject>
#include <QMap>
#include <QString>
namespace aitsnap {
struct ReportTextSpec {
    bool titleEnabled{true},captionEnabled{true},contextEnabled{true};
    QString preset{"legacy"};
    QString titleTemplate{"AITSnap {profile} | {datetime}"};
    QString captionTemplate{"{label} | {datetime}"};
    QString contextTemplate{"test={test} device={device} station={station} operator={operator} run={run} step={step} command={command} event={event}"};
};
QMap<QString,QString> reportTextValues(const core::CaptureRequest&);
QString expandReportText(const QString&,const QMap<QString,QString>&,QString& error);
bool validateReportText(const ReportTextSpec&,QString& error);
QJsonObject reportTextJson(const ReportTextSpec&);
bool reportTextFromJson(const QJsonObject&,ReportTextSpec&,QString& error);
}
