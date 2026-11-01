// SPDX-License-Identifier: Apache-2.0
#include "settings_commands.h"
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static void removeSceneRefs(core::CaptureProfile& p, const QStringList& ids)
{
    const bool hadScenes = !p.sceneIds.empty();
    std::erase_if(p.sceneIds, [&](const auto& id) { return ids.contains(qs(id)); });
    if (p.clipboardSelection.starts_with("scene:") && ids.contains(qs(p.clipboardSelection).mid(6))) p.clipboardSelection.clear();
    if (hadScenes && p.sceneIds.empty()) p.outputMode = "originals";
}
void selectProfile(Settings& s, int index)
{
    s.activeProfile = index; const auto& p = s.profiles.at(std::size_t(index));
    s.selectedSceneIds.clear(); for (const auto& id : p.sceneIds) s.selectedSceneIds.append(qs(id));
    s.outputMode = qs(p.outputMode); s.clipboardRegion = qs(p.clipboardSelection);
}
core::CaptureProfile emptyProfile(const QString& name)
{
    core::CaptureProfile p; p.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); p.name = name.toStdString(); return p;
}
RemovalSummary removeRegions(Settings& s, QJsonArray& rules, const QString& profileId, const QStringList& ids)
{
    RemovalSummary result;
    for (auto& p : s.profiles) if (qs(p.id) == profileId) {
        QStringList removed;
        std::erase_if(p.regions, [&](const auto& r) { if (!ids.contains(qs(r.id))) return false; removed.append(qs(r.id)); return true; });
        result.removed = removed.size(); if (!result.removed) return result;
        QStringList affected;
        for (const auto& scene : s.scenes) if (std::any_of(scene.layers.begin(), scene.layers.end(), [&](const auto& l) { return l.visible && removed.contains(qs(l.sourceRegionId)); })) {
            affected.append(qs(scene.id)); result.affectedScenes.append(qs(scene.name));
        }
        removeSceneRefs(p, affected);
        if (removed.contains(qs(p.clipboardSelection))) p.clipboardSelection.clear();
        const bool noSources=std::none_of(p.regions.begin(),p.regions.end(),[](const auto& r){return r.enabled;});
        for(int i=0;i<rules.size();++i) {
            auto rule=rules[i].toObject();const auto target=rule["profileId"].toString();
            if(!target.isEmpty() && target!=profileId)continue;
            bool broken=noSources;for(const auto& id:rule["sceneIds"].toArray())broken|=affected.contains(id.toString());
            if(broken && rule["enabled"].toBool()){rule["enabled"]=false;rules[i]=rule;++result.disabledRules;}
        }
        ++p.revision; break;
    }
    selectProfile(s, s.activeProfile); return result;
}
RemovalSummary removeScenes(Settings& s, QJsonArray& rules, const QStringList& ids)
{
    RemovalSummary result; QStringList removed;
    std::erase_if(s.scenes, [&](const auto& scene) { if (!ids.contains(qs(scene.id))) return false; removed.append(qs(scene.id)); return true; });
    result.removed = removed.size(); if (!result.removed) return result;
    for (auto& p : s.profiles) { const auto old = p.sceneIds; const auto clip = p.clipboardSelection; removeSceneRefs(p, removed); if (p.sceneIds != old || clip != p.clipboardSelection) ++p.revision; }
    for (int i = 0; i < rules.size(); ++i) {
        auto rule = rules[i].toObject(); bool affected = false;
        for (const auto& id : rule["sceneIds"].toArray()) affected |= removed.contains(id.toString());
        if (affected && rule["enabled"].toBool()) { rule["enabled"] = false; rules[i] = rule; ++result.disabledRules; }
    }
    selectProfile(s, s.activeProfile); return result;
}
RemovalSummary removeProfiles(Settings& s, QJsonArray& rules, const QStringList& ids, const QString& fallbackName)
{
    RemovalSummary result; const auto activeId = s.profiles.at(std::size_t(s.activeProfile)).id; QStringList removed;
    std::erase_if(s.profiles, [&](const auto& p) { if (!ids.contains(qs(p.id))) return false; removed.append(qs(p.id)); return true; });
    result.removed = removed.size(); if (!result.removed) return result;
    if (s.profiles.empty()) s.profiles.push_back(emptyProfile(fallbackName));
    int active = 0; for (int i = 0; i < int(s.profiles.size()); ++i) if (s.profiles[std::size_t(i)].id == activeId) active = i;
    for (int i = 0; i < rules.size(); ++i) {
        auto rule = rules[i].toObject();
        bool affected=removed.contains(rule["profileId"].toString());
        if(rule["profileId"].toString().isEmpty() && removed.contains(qs(activeId))) {
            const auto& replacement=s.profiles[std::size_t(active)];
            affected=std::none_of(replacement.regions.begin(),replacement.regions.end(),[](const auto& r){return r.enabled;});
            for(const auto& id:rule["sceneIds"].toArray()) {
                const auto scene=std::find_if(s.scenes.begin(),s.scenes.end(),[&](const auto& value){return qs(value.id)==id.toString();});
                if(scene==s.scenes.end()){affected=true;continue;}
                affected|=std::any_of(scene->layers.begin(),scene->layers.end(),[&](const auto& layer){return layer.visible && std::none_of(replacement.regions.begin(),replacement.regions.end(),[&](const auto& r){return r.enabled && r.id==layer.sourceRegionId;});});
            }
        }
        if (affected && rule["enabled"].toBool()) { rule["enabled"] = false; rules[i] = rule; ++result.disabledRules; }
    }
    selectProfile(s, active); return result;
}
bool persistSettingsCommand(const QString& path, const Settings& s, const QJsonArray* rules, QString& error)
{
    if (!rules) return saveSettings(path, s, error);
    QFile original(path); const bool existed = original.exists();
    if (existed && !original.open(QIODevice::ReadOnly)) { error = original.errorString(); return false; }
    const auto previous = existed ? original.readAll() : QByteArray{}; original.close();
    const auto recovery = path + ".before-management.json";
    if (QFile::exists(recovery)) { error = "Previous management operation needs recovery: " + recovery; return false; }
    QSaveFile backup(recovery);
    if (!backup.open(QIODevice::WriteOnly) || backup.write(previous) != previous.size() || !backup.commit()) {
        error = "Cannot preserve configuration before changing rules: " + backup.errorString(); return false;
    }
    QSaveFile stagedRules(QFileInfo(path).absolutePath() + "/host-rules-v1.json");
    const auto bytes = QJsonDocument(QJsonObject{{"schemaVersion",1},{"rules",*rules}}).toJson();
    if (!stagedRules.open(QIODevice::WriteOnly) || stagedRules.write(bytes) != bytes.size()) {
        error = stagedRules.errorString(); QFile::remove(recovery); return false;
    }
    if (!saveSettings(path, s, error)) { stagedRules.cancelWriting(); QFile::remove(recovery); return false; }
    if (stagedRules.commit()) { QFile::remove(recovery); return true; }
    error = "Rules were not saved: " + stagedRules.errorString();
    bool restored = false;
    if (existed) {
        QSaveFile rollback(path); restored = rollback.open(QIODevice::WriteOnly) && rollback.write(previous) == previous.size() && rollback.commit();
    } else restored = QFile::remove(path);
    if (restored) QFile::remove(recovery);
    else error += "; configuration recovery required from " + recovery;
    return false;
}
}
