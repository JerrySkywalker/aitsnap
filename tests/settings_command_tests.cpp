// SPDX-License-Identifier: Apache-2.0
#include "adapters/settings_commands.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv)
{
    QCoreApplication app(argc,argv);
    try {
        Settings s = defaultSettings(); auto a = emptyProfile("One"); a.id = "p1"; a.nextRegionNumber = 9;
        a.regions = {{"r1","One",{0,0,20,10},true,1},{"r2","Two",{20,0,20,10},true,2}};
        a.sceneIds = {"s1","s2"}; a.outputMode = "both"; a.clipboardSelection = "scene:s1";
        auto b = a; b.id = "p2"; b.name = "Two"; s.profiles = {a,b};
        core::SceneTemplate first; first.id="s1"; first.name="First"; first.layers.push_back({"l1","r1"});
        auto second=first; second.id="s2"; second.name="Second"; second.layers[0].sourceRegionId="r2"; s.scenes={first,second}; selectProfile(s,0);
        QJsonArray rules{QJsonObject{{"enabled",true},{"sceneIds",QJsonArray{"s1"}},{"profileId","p1"}},QJsonObject{{"enabled",true},{"sceneIds",QJsonArray{"s2"}},{"profileId","p2"}}};
        {
            auto fallback=s;QJsonArray dynamic{QJsonObject{{"enabled",true},{"profileId",""},{"sceneIds",QJsonArray{"s1"}}}};
            removeProfiles(fallback,dynamic,{"p1"},"Default");check(dynamic[0].toObject()["enabled"].toBool(),"dynamic rule keeps valid replacement profile");
            removeProfiles(fallback,dynamic,{"p2"},"Default");check(!dynamic[0].toObject()["enabled"].toBool(),"dynamic rule disabled when replacement profile has no capture sources");
            fallback=s;fallback.profiles[1].regions.erase(fallback.profiles[1].regions.begin());dynamic[0]=QJsonObject{{"enabled",true},{"profileId",""},{"sceneIds",QJsonArray{"s1"}}};
            removeProfiles(fallback,dynamic,{"p1"},"Default");check(!dynamic[0].toObject()["enabled"].toBool(),"dynamic scene override disabled when replacement profile lacks its source");
        }
        auto result=removeRegions(s,rules,"p1",{"r1"});
        check(result.removed==1 && s.profiles[0].regions.size()==1 && s.profiles[0].nextRegionNumber==9,"region identity/highwater preserved");
        check(s.scenes.size()==2 && s.profiles[0].sceneIds==std::vector<std::string>{"s2"} && s.clipboardRegion.isEmpty(),"keep templates, remove only affected current outputs");
        check(s.profiles[1].regions.size()==2 && s.profiles[1].sceneIds.size()==2 && s.profiles[1].clipboardSelection=="scene:s1","shared ids in another profile unaffected");
        check(result.disabledRules==1 && !rules[0].toObject()["enabled"].toBool() && rules[1].toObject()["enabled"].toBool(),"region deletion disables dependent rule only in its profile scope");
        result=removeScenes(s,rules,{"s1"});
        check(result.removed==1 && result.disabledRules==0 && !rules[0].toObject()["enabled"].toBool() && rules[1].toObject()["enabled"].toBool(),"disabled rule is not accidentally re-enabled or counted twice");
        check(rules[0].toObject()["sceneIds"].toArray()[0]=="s1" && s.profiles[1].clipboardSelection.empty() && s.profiles[1].sceneIds==std::vector<std::string>{"s2"},"retain disabled-rule diagnostic, clear all profile references");
        removeProfiles(s,rules,{"p1"},"Default");
        check(s.profiles.size()==1 && s.profiles[0].id=="p2" && s.activeProfile==0 && s.selectedSceneIds==QStringList{"s2"},"active profile rebased by identity");
        removeProfiles(s,rules,{"p2"},"Default");
        check(s.profiles.size()==1 && s.profiles[0].name=="Default" && s.profiles[0].regions.empty() && s.selectedSceneIds.empty(),"delete all retains explicit empty container");
        check(!rules[1].toObject()["enabled"].toBool() && !core::validateProfileDefinition(s.profiles[0]) && bool(core::validateProfile(s.profiles[0])),"empty fallback persists but cannot capture");
        QTemporaryDir root; QString error; auto path=root.filePath("settings.json"); check(saveSettings(path,s,error),"persist candidate");
        Settings disk; check(loadSettings(path,disk,error) && disk.profiles.size()==1 && disk.profiles[0].regions.empty() && disk.outputMode=="originals","delete-all survives reload without fake regions");
        std::cout<<"SETTINGS_COMMANDS_PASS scoped region removal global scene cleanup disabled rules active identity empty fallback persistence\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
