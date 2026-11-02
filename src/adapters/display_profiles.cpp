// SPDX-License-Identifier: Apache-2.0
#include "display_profiles.h"
#include "settings_commands.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <tuple>
namespace aitsnap {
static QString qs(const std::string& s){return QString::fromUtf8(s.c_str());}
static QString monitorIdentity(const std::string& id){
    auto text=qs(id);const auto colon=text.indexOf(':');
    if(text.startsWith("\\\\.\\DISPLAY",Qt::CaseInsensitive) && colon>=0 && colon+1<text.size())text=text.mid(colon+1);
    return text;
}
QString displayLayoutKey(const std::vector<core::Display>& displays){
    if(displays.empty())return {};
    auto sorted=displays;
    if(std::any_of(sorted.begin(),sorted.end(),[](const auto& d){return d.id.empty() || !d.rect.valid();}))return {};
    std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return std::make_tuple(monitorIdentity(a.id),a.rect.x,a.rect.y,a.rect.width,a.rect.height,a.rotation)<std::make_tuple(monitorIdentity(b.id),b.rect.x,b.rect.y,b.rect.width,b.rect.height,b.rotation);});
    QJsonArray rows;for(const auto& d:sorted)rows.append(QJsonArray{monitorIdentity(d.id),d.rect.x,d.rect.y,d.rect.width,d.rect.height,d.rotation});
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(rows).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
DisplayProfiles::DisplayProfiles(QString path):path_(std::move(path)){
    QFile file(path_);if(!file.exists())return;
    if(!file.open(QIODevice::ReadOnly) || file.size()>4*1024*1024){diagnostic_="Display layout memory unreadable/oversized; original preserved";return;}
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);const auto root=doc.object();
    if(parse.error || !doc.isObject() || root["schemaVersion"].toInt()!=1 || !root["enabled"].isBool() || !root["bindings"].isArray() || !root["forgotten"].isArray()){diagnostic_="Invalid display layout memory; original preserved";return;}
    enabled_=root["enabled"].toBool();
    for(const auto& item:root["bindings"].toArray()){
        const auto row=item.toObject();core::CaptureProfile profile;QString error;const auto key=row["key"].toString();
        if(key.isEmpty() || bindings_.contains(key) || !profileFromJson(row["profile"].toObject(),profile,error) || displayLayoutKey(profile.calibratedDisplays)!=key){diagnostic_="Invalid display binding snapshot; original preserved";bindings_.clear();return;}
        bindings_.insert(key,std::move(profile));
    }
    for(const auto& key:root["forgotten"].toArray()){if(!key.isString()){diagnostic_="Invalid forgotten layout; original preserved";return;}forgotten_.insert(key.toString());}
}
bool DisplayProfiles::persist(QString& error){
    if(!ready()){error=diagnostic_;return false;}
    QJsonArray rows,forgotten;for(auto it=bindings_.cbegin();it!=bindings_.cend();++it)rows.append(QJsonObject{{"key",it.key()},{"profile",profileJson(it.value())}});
    auto keys=forgotten_.values();std::sort(keys.begin(),keys.end());for(const auto& key:keys)forgotten.append(key);
    const auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",1},{"enabled",enabled_},{"bindings",rows},{"forgotten",forgotten}}).toJson();
    QSaveFile file(path_);if(!QDir().mkpath(QFileInfo(path_).absolutePath()) || !file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()){error="Cannot save display layout memory: "+file.errorString();return false;}return true;
}
bool DisplayProfiles::setEnabled(bool enabled,QString& error){const auto previous=enabled_;enabled_=enabled;if(persist(error))return true;enabled_=previous;return false;}
bool DisplayProfiles::seed(const Settings& settings,QString& error){
    if(!ready()){error=diagnostic_;return false;}
    auto before=bindings_;QMap<QString,std::vector<int>> groups;
    for(int i=0;i<int(settings.profiles.size());++i){const auto key=displayLayoutKey(settings.profiles[std::size_t(i)].calibratedDisplays);if(!key.isEmpty())groups[key].push_back(i);}
    for(auto it=groups.cbegin();it!=groups.cend();++it){
        if(bindings_.contains(it.key()) || forgotten_.contains(it.key()))continue;
        const auto& indexes=it.value();int chosen=-1;
        if(indexes.size()==1)chosen=indexes.front();else if(std::find(indexes.begin(),indexes.end(),settings.activeProfile)!=indexes.end())chosen=settings.activeProfile;
        if(chosen>=0)bindings_.insert(it.key(),settings.profiles[std::size_t(chosen)]);
    }
    if(bindings_.size()==before.size())return true;
    if(persist(error))return true;bindings_=std::move(before);return false;
}
bool DisplayProfiles::remember(const core::CaptureProfile& profile,QString& error){
    if(!ready()){error=diagnostic_;return false;}const auto key=displayLayoutKey(profile.calibratedDisplays);if(key.isEmpty())return true;
    auto current=profileJson(profile);current.remove("revision");
    if(bindings_.contains(key)){auto old=profileJson(bindings_[key]);old.remove("revision");if(old==current && !forgotten_.contains(key))return true;}
    const auto before=bindings_;const auto ignored=forgotten_;bindings_[key]=profile;forgotten_.remove(key);
    if(persist(error))return true;bindings_=before;forgotten_=ignored;return false;
}
bool DisplayProfiles::forget(const std::vector<core::Display>& displays,QString& error){
    if(!ready()){error=diagnostic_;return false;}const auto key=displayLayoutKey(displays);if(key.isEmpty())return true;
    const auto before=bindings_;const auto ignored=forgotten_;bindings_.remove(key);forgotten_.insert(key);
    if(persist(error))return true;bindings_=before;forgotten_=ignored;return false;
}
QString DisplayProfiles::boundProfile(const std::vector<core::Display>& displays) const{const auto it=bindings_.constFind(displayLayoutKey(displays));return it==bindings_.cend()?QString{}:qs(it.value().id);}
bool DisplayProfiles::restoration(const Settings& settings,const std::vector<core::Display>& displays,Settings& candidate,QString& error) const{
    if(!ready()){error=diagnostic_;return false;}if(!enabled_)return false;const auto key=displayLayoutKey(displays);const auto binding=bindings_.constFind(key);if(key.isEmpty() || binding==bindings_.cend())return false;
    const auto found=std::find_if(settings.profiles.begin(),settings.profiles.end(),[&](const auto& p){return p.id==binding.value().id;});
    if(found==settings.profiles.end()){error="Remembered display profile was deleted; bind another profile";return false;}
    const int index=int(found-settings.profiles.begin());
    if(index==settings.activeProfile && core::sameTopology(found->calibratedDisplays,displays))return false;
    candidate=settings;auto restored=displayLayoutKey(found->calibratedDisplays)==key?*found:binding.value();
    restored.name=found->name;restored.revision=found->revision+1;restored.nextRegionNumber=std::max(restored.nextRegionNumber,found->nextRegionNumber);restored.calibratedDisplays=displays;
    std::erase_if(restored.sceneIds,[&](const auto& id){return std::none_of(settings.scenes.begin(),settings.scenes.end(),[&](const auto& scene){return scene.id==id;});});
    if(restored.sceneIds.empty())restored.outputMode="originals";
    if(restored.clipboardSelection.starts_with("scene:") && std::none_of(restored.sceneIds.begin(),restored.sceneIds.end(),[&](const auto& id){return "scene:"+id==restored.clipboardSelection;}))restored.clipboardSelection.clear();
    candidate.profiles[std::size_t(index)]=std::move(restored);selectProfile(candidate,index);return true;
}
}
