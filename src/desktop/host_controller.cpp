// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "adapters/local_host.h"
#include "adapters/context_store.h"
#include "adapters/report_queue.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QVBoxLayout>
#include <future>
#include <atomic>
#include <algorithm>
namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static const QStringList eventTypes{"SEQUENCE_STARTED", "SEQUENCE_COMPLETED", "SEQUENCE_FAILED", "SEQUENCE_ABORTED", "SEQUENCE_PAUSED", "SEQUENCE_RESUMED", "STEP_STARTED", "STEP_COMPLETED", "COMMAND_DISPATCHED", "COMMAND_ACKNOWLEDGED", "COMMAND_FAILED", "TELEMETRY_CONDITION"};
static bool allowed(const QJsonObject& value, const QStringList& keys) { for (auto i = value.begin(); i != value.end(); ++i) if (!keys.contains(i.key())) return false; return true; }
bool MainWindow::validateHostOptions(const QJsonObject& value, QString& error) const
{
    if (value.contains("mode") && !QStringList{"save", "copy", "both"}.contains(value["mode"].toString())) { error = "Unsupported capture output mode"; return false; }
    if (value.contains("profileId") && (!value["profileId"].isString() || std::none_of(settings_.profiles.begin(), settings_.profiles.end(), [&](const auto& p) { return qs(p.id) == value["profileId"].toString(); }))) { error = "Unknown capture profile"; return false; }
    if (value.contains("sceneIds")) {
        if (!value["sceneIds"].isArray() || value["sceneIds"].toArray().size() > 100) { error = "Scene IDs require a bounded array"; return false; }
        for (const auto& id : value["sceneIds"].toArray()) if (!id.isString() || std::none_of(settings_.scenes.begin(), settings_.scenes.end(), [&](const auto& s) { return qs(s.id) == id.toString(); })) { error = "Unknown scene reference"; return false; }
    } return true;
}
bool MainWindow::validateEventRule(const QJsonObject& rule, QString& error) const
{
    if (!allowed(rule, {"enabled", "sourceSystem", "eventType", "sequenceDefinitionId", "stepId", "commandDefinitionId", "profileId", "sceneIds", "mode"}) || !rule["enabled"].isBool() || !eventTypes.contains(rule["eventType"].toString()) || !rule["sceneIds"].isArray() || !QStringList{"save", "copy", "both"}.contains(rule["mode"].toString())) { error = "Invalid event-rule v1 fields/type"; return false; }
    for (const auto& key : {"sourceSystem", "sequenceDefinitionId", "stepId", "commandDefinitionId", "profileId"}) if (rule.contains(key) && !rule[key].isString()) { error = "Rule filters require strings"; return false; }
    QJsonObject options{{"sceneIds", rule["sceneIds"]}, {"mode", rule["mode"]}}; if (!rule["profileId"].toString().isEmpty()) options["profileId"] = rule["profileId"]; return validateHostOptions(options, error);
}
void MainWindow::startHost(QString name)
{
    QString warning; const auto rulesPath = QFileInfo(configurationPath_).absolutePath() + "/host-rules-v1.json"; QFile rulesFile(rulesPath);
    if (rulesFile.exists()) {
        if (!rulesFile.open(QIODevice::ReadOnly) || rulesFile.size() > 128 * 1024) warning = "Rules unreadable/oversized; automatic rules held";
        else { QJsonParseError parse; const auto doc = QJsonDocument::fromJson(rulesFile.readAll(), &parse); const auto root = doc.object();
            if (parse.error || !doc.isObject() || !allowed(root, {"schemaVersion", "rules"}) || root["schemaVersion"].toDouble() != 1 || !root["rules"].isArray() || root["rules"].toArray().size() > 100) warning = "Invalid rule file retained; automatic rules held";
            else { auto candidate = root["rules"].toArray(); for (const auto& r : candidate) { if (!r.isObject()) { warning = "Rule requires an object"; break; } if (!validateEventRule(r.toObject(), warning)) break; } if (warning.isEmpty()) eventRules_ = candidate; }
        }
    }
    if (name == "-") { hostStatus_->setText(QStringLiteral("本地 CLI：本测试实例未启动端点。")); return; }
    QString error; if (name.isEmpty()) name = defaultPipeName(error); QPointer<MainWindow> self(this);
    HostHandler handler = [self](const QString& action, const QJsonObject& payload, bool preflight) {
        struct Pending { std::promise<QJsonObject> result; std::atomic<bool> cancelled{}; }; auto pending = std::make_shared<Pending>(); auto result = pending->result.get_future();
        if (!self || !QMetaObject::invokeMethod(self, [self, pending, action, payload, preflight] {
            if (pending->cancelled || !self) { pending->result.set_value(hostError("UI_UNAVAILABLE", "Desktop request cancelled before invocation")); return; }
            try { pending->result.set_value(self->hostRequest(action, payload, preflight)); } catch (...) { pending->result.set_value(hostError("UI_HANDLER", "Desktop handler may have begun", preflight ? "ERROR" : "UNCERTAIN")); }
        }, Qt::QueuedConnection)) return hostError("UI_UNAVAILABLE", "Desktop not available");
        if (result.wait_for(std::chrono::seconds(5)) != std::future_status::ready) { pending->cancelled = true; return hostError("UI_TIMEOUT", "Desktop response not confirmed", preflight ? "BUSY" : "UNCERTAIN"); }
        return result.get();
    };
    localHost_ = std::make_unique<LocalPipeServer>(name, QFileInfo(configurationPath_).absolutePath() + "/host-receipts", std::move(handler));
    if (!localHost_->start(error)) hostStatus_->setText(QStringLiteral("本地 CLI HOLD：") + error);
    else hostStatus_->setText(QStringLiteral("本地 CLI：%1 · 当前登录会话 · %2 条规则").arg(name).arg(eventRules_.size()) + (warning.isEmpty() ? "" : "\n" + warning));
}
QJsonObject MainWindow::hostRequest(const QString& action, const QJsonObject& payload, bool preflight)
{
    auto ready = [] { return QJsonObject{{"status", "READY"}}; };
    auto available = [&]() -> QJsonObject { if (busy_ || editingRegions_ || editingScene_ || paused_) return hostError("CAPTURE_BUSY", "Capture paused, editing or busy; retry the same key later", "BUSY"); if (configurationBlocked_) return hostError("CONFIGURATION_HOLD", "Review and apply the preserved configuration"); return ready(); };
    if (action == "query-status") {
        if (!payload.isEmpty()) return hostError("PAYLOAD", "Status payload must be empty");
        QJsonObject result{{"busy", busy_}, {"requestId", currentRequestId_}, {"profileId", qs(settings_.profiles[std::size_t(settings_.activeProfile)].id)}, {"events", contextStore_->events().size()}, {"catalogs", contextStore_->catalogs().size()}, {"eventRules", eventRules_.size()}};
        if (lastOutcome_) { result["lastBatchId"] = qs(lastOutcome_->batch.batchId); result["lastBatchDirectory"] = lastOutcome_->directory; result["lastBatchStatus"] = batchJson(lastOutcome_->batch)["status"]; }
        int committed = 0, uncertain = 0; for (const auto& job : reportQueue_->jobs()) { if (job["state"] == "COMMITTED") ++committed; if (job["state"] == "UNCERTAIN") ++uncertain; } result["reportCommitted"] = committed; result["reportUncertain"] = uncertain; return {{"status", "OK"}, {"result", result}};
    }
    if (action == "list-profiles") { if (!payload.isEmpty()) return hostError("PAYLOAD", "Profile-list payload must be empty"); QJsonArray profiles, scenes; for (const auto& p : settings_.profiles) profiles.append(QJsonObject{{"profileId", qs(p.id)}, {"name", qs(p.name)}, {"revision", p.revision}}); for (const auto& s : settings_.scenes) scenes.append(QJsonObject{{"sceneId", qs(s.id)}, {"name", qs(s.name)}, {"revision", s.revision}}); return {{"status", "OK"}, {"result", QJsonObject{{"profiles", profiles}, {"scenes", scenes}}}}; }
    if (action == "import-catalog") { if (!allowed(payload, {"path"}) || !payload["path"].isString() || !QDir::isAbsolutePath(payload["path"].toString())) return hostError("PAYLOAD", "Import requires an exact absolute catalog path"); if (preflight) return ready(); QString error; return contextStore_->importCatalog(payload["path"].toString(), error) ? QJsonObject{{"status", "OK"}, {"result", QJsonObject{{"catalogs", contextStore_->catalogs().size()}}}} : hostError("CATALOG", error); }
    if (action == "select-profile") {
        QString error; if (!allowed(payload, {"profileId"}) || !payload.contains("profileId") || !validateHostOptions(payload, error)) return hostError("PROFILE", error.isEmpty() ? "Select requires profileId" : error); const auto availability = available(); if (availability["status"] != "READY") return availability; if (preflight) return ready();
        auto candidate = settings_; const auto id = payload["profileId"].toString();
        for (std::size_t i = 0; i < candidate.profiles.size(); ++i) if (qs(candidate.profiles[i].id) == id) candidate.activeProfile = int(i);
        const auto& profile = candidate.profiles[std::size_t(candidate.activeProfile)]; candidate.outputMode = qs(profile.outputMode); candidate.clipboardRegion = qs(profile.clipboardSelection); candidate.selectedSceneIds.clear(); for (const auto& scene : profile.sceneIds) candidate.selectedSceneIds.append(qs(scene));
        if (!saveSettings(configurationPath_, candidate, error)) return hostError("SETTINGS", error); settings_ = candidate; profiles_->blockSignals(true); profiles_->setCurrentIndex(candidate.activeProfile); profiles_->blockSignals(false); loadProfile(); rebuildProfileMenu(); return {{"status", "OK"}, {"result", QJsonObject{{"profileId", id}}}};
    }
    if (action == "capture") {
        QString error; if (!allowed(payload, {"mode", "profileId", "sceneIds", "sourceSystem", "eventId"}) || !validateHostOptions(payload, error)) return hostError("PAYLOAD", error.isEmpty() ? "Unsupported capture field" : error);
        QJsonObject snapshot; if (payload.contains("sourceSystem") || payload.contains("eventId")) { snapshot = contextStore_->snapshot(payload["sourceSystem"].toString(), payload["eventId"].toString()); if (snapshot.isEmpty()) return hostError("CONTEXT", "Explicit source event was not received"); }
        const auto availability = available(); if (availability["status"] != "READY") return availability; if (preflight) return ready(); const auto mode = payload["mode"].toString("save"); const auto id = startCapture(mode == "copy" ? 1 : mode == "both" ? 2 : 0, payload, snapshot);
        return id.isEmpty() ? hostError("CAPTURE_NOT_STARTED", "Capture was not accepted; saved event metadata remains separate") : QJsonObject{{"status", "ACCEPTED"}, {"result", QJsonObject{{"requestId", id}}}};
    }
    if (action == "submit-event") {
        QString error; if (!allowed(payload, {"event", "capture", "mode", "profileId", "sceneIds"}) || !payload["event"].isObject() || (payload.contains("capture") && !payload["capture"].isBool()) || !validateHostOptions(payload, error)) return hostError("PAYLOAD", error.isEmpty() ? "Invalid event submit fields" : error);
        const auto event = payload["event"].toObject(), sequence = event["sequence"].toObject(), command = event["command"].toObject(); QJsonObject options; bool matched = false;
        for (const auto& value : eventRules_) { const auto rule = value.toObject(); if (!rule["enabled"].toBool() || rule["eventType"] != event["eventType"]) continue;
            auto match = [&](const char* key, const QJsonValue& actual) { return rule[key].toString().isEmpty() || rule[key] == actual; };
            if (!match("sourceSystem", event["sourceSystem"]) || !match("sequenceDefinitionId", sequence["definitionId"]) || !match("stepId", sequence["stepId"]) || !match("commandDefinitionId", command["definitionId"])) continue;
            matched = true; options["mode"] = rule["mode"]; options["sceneIds"] = rule["sceneIds"]; if (!rule["profileId"].toString().isEmpty()) options["profileId"] = rule["profileId"]; break;
        }
        for (const auto& key : {"mode", "profileId", "sceneIds"}) if (payload.contains(key)) options[key] = payload[key];
        if (!validateHostOptions(options, error)) return hostError("RULE_TARGET", error);
        const auto preview = contextStore_->preview(event); if (preview.error) return hostError(qs(preview.error.code), qs(preview.error.message));
        const bool capture = matched || payload["capture"].toBool(); if (capture && preview.disposition != core::EventDisposition::Duplicate) { const auto availability = available(); if (availability["status"] != "READY") return availability; }
        if (preflight) return ready(); const auto result = contextStore_->ingest(event); if (result.error) return hostError(qs(result.error.code), qs(result.error.message));
        if (result.disposition == core::EventDisposition::Duplicate) return {{"status", "DUPLICATE"}, {"result", QJsonObject{{"eventId", event["eventId"]}, {"newCapture", false}}}};
        QString id; if (capture) { const auto mode = options["mode"].toString("save"); id = startCapture(mode == "copy" ? 1 : mode == "both" ? 2 : 0, options, contextStore_->snapshot(event["sourceSystem"].toString(), event["eventId"].toString())); if (id.isEmpty()) return hostError("EVENT_DURABLE_CAPTURE_UNCONFIRMED", "Event saved; inspect status before a separate explicit capture", "UNCERTAIN"); }
        return {{"status", "ACCEPTED"}, {"result", QJsonObject{{"eventId", event["eventId"]}, {"requestId", id}, {"newCapture", capture}, {"correlationStatus", qs(result.context.correlationStatus)}}}};
    }
    return hostError("ACTION", "Unsupported action");
}
void MainWindow::editEventRules()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("事件截图规则 · 元数据匹配")); dialog.resize(1240, 530); auto* layout = new QVBoxLayout(&dialog);
    auto* note = new QLabel(QStringLiteral("按表格顺序采用第一条启用且匹配的规则。空过滤器匹配任意值；空 Scene IDs 表示仅原图。Word 使用当前自动报告配置。规则不执行来源指令。")); note->setWordWrap(true); layout->addWidget(note);
    auto* table = new QTableWidget(0, 9); table->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("来源"), QStringLiteral("事件类型"), "Sequence ID", "Step ID", "Command ID", QStringLiteral("截图配置"), "Scene IDs (,)", QStringLiteral("输出")}); table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); layout->addWidget(table);
    auto add = [this, table](QJsonObject rule) { const int row = table->rowCount(); table->insertRow(row); auto* enabled = new QTableWidgetItem; enabled->setCheckState(rule["enabled"].toBool() ? Qt::Checked : Qt::Unchecked); table->setItem(row, 0, enabled);
        const char* filters[]{"sourceSystem", "sequenceDefinitionId", "stepId", "commandDefinitionId"}; const int cols[]{1, 3, 4, 5}; for (int i = 0; i < 4; ++i) table->setItem(row, cols[i], new QTableWidgetItem(rule[filters[i]].toString()));
        auto* type = new QComboBox; type->addItems(eventTypes); type->setCurrentText(rule["eventType"].toString("COMMAND_ACKNOWLEDGED")); table->setCellWidget(row, 2, type);
        auto* profile = new QComboBox; profile->addItem(QStringLiteral("当前配置"), ""); for (const auto& p : settings_.profiles) profile->addItem(qs(p.name), qs(p.id)); profile->setCurrentIndex(std::max(0, profile->findData(rule["profileId"].toString()))); table->setCellWidget(row, 6, profile);
        QStringList scenes; for (const auto& scene : rule["sceneIds"].toArray()) scenes.append(scene.toString()); table->setItem(row, 7, new QTableWidgetItem(scenes.join(',')));
        auto* mode = new QComboBox; mode->addItems({"save", "copy", "both"}); mode->setCurrentText(rule["mode"].toString("save")); table->setCellWidget(row, 8, mode);
    }; for (const auto& rule : eventRules_) add(rule.toObject());
    QStringList availableScenes; for (const auto& s : settings_.scenes) availableScenes.append(qs(s.id) + " = " + qs(s.name)); auto* references = new QLabel(QStringLiteral("可用 Scene IDs：") + availableScenes.join(" · ")); references->setWordWrap(true); references->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(references);
    auto* actions = new QHBoxLayout; auto* append = new QPushButton(QStringLiteral("新增禁用规则")); auto* remove = new QPushButton(QStringLiteral("移除选中规则")); actions->addWidget(append); actions->addWidget(remove); layout->addLayout(actions); connect(append, &QPushButton::clicked, &dialog, [add] { add(QJsonObject{{"enabled", false}, {"eventType", "COMMAND_ACKNOWLEDGED"}, {"sceneIds", QJsonArray{}}, {"mode", "save"}}); }); connect(remove, &QPushButton::clicked, &dialog, [table] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, table, note, &dialog] { QJsonArray rules; QString error;
        for (int row = 0; row < table->rowCount(); ++row) { QJsonArray scenes; for (const auto& id : table->item(row, 7)->text().split(',', Qt::SkipEmptyParts)) scenes.append(id.trimmed());
            QJsonObject rule{{"enabled", table->item(row, 0)->checkState() == Qt::Checked}, {"sourceSystem", table->item(row, 1)->text().trimmed()}, {"eventType", qobject_cast<QComboBox*>(table->cellWidget(row, 2))->currentText()}, {"sequenceDefinitionId", table->item(row, 3)->text().trimmed()}, {"stepId", table->item(row, 4)->text().trimmed()}, {"commandDefinitionId", table->item(row, 5)->text().trimmed()}, {"profileId", qobject_cast<QComboBox*>(table->cellWidget(row, 6))->currentData().toString()}, {"sceneIds", scenes}, {"mode", qobject_cast<QComboBox*>(table->cellWidget(row, 8))->currentText()}};
            if (!validateEventRule(rule, error)) { note->setText(error); return; } rules.append(rule);
        }
        if (rules.size() > 100) { note->setText(QStringLiteral("最多 100 条规则。")); return; }
        QSaveFile file(QFileInfo(configurationPath_).absolutePath() + "/host-rules-v1.json"); const auto bytes = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"rules", rules}}).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { note->setText(file.errorString()); return; } eventRules_ = rules; hostStatus_->setText(QStringLiteral("本地事件规则已保存：%1 条；第一条匹配规则生效。").arg(rules.size())); dialog.accept();
    }); dialog.exec();
}
} // namespace aitsnap
