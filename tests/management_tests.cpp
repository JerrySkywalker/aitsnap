// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "main_window.h"
#include "desktop/appearance.h"
#include "adapters/settings_commands.h"
#include <QApplication>
#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QJsonDocument>
#include <QListWidget>
#include <QMenu>
#include <QComboBox>
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
static QByteArray bytes(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"read fixture");return f.readAll();}
static void settle(){for(int i=0;i<10;++i){QCoreApplication::processEvents();QThread::msleep(10);}}
static void confirm(bool yes){QTimer::singleShot(0,[yes]{auto* box=dynamic_cast<QDialog*>(QApplication::activeModalWidget());if(box)box->findChild<QPushButton*>(yes?"confirmDeleteObjects":"cancelDeleteObjects")->click();});}
int main(int argc,char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);QApplication app(argc,argv);
    try{
        check(app.platformName()=="offscreen","offscreen required");installLanguage("en-US");applyColorMode("dark");
        QTemporaryDir root;auto s=defaultSettings();s.outputDirectory=root.path();s.saveHotkey="Ctrl+Alt+Shift+F20";s.copyHotkey="Ctrl+Alt+Shift+F21";s.bothHotkey="Ctrl+Alt+Shift+F22";
        auto p=emptyProfile("One");p.id="p1";p.nextRegionNumber=7;p.regions={{"r1","One",{0,0,64,32},true,1},{"r2","Two",{64,0,64,32},true,2}};p.sceneIds={"s1","s2"};p.outputMode="both";
        auto other=p;other.id="p2";other.name="Other";s.profiles={p,other};
        core::SceneTemplate a;a.id="s1";a.name="Scene one";a.layers.push_back({"l1","r1"});auto b=a;b.id="s2";b.name="Scene two";b.layers[0].sourceRegionId="r2";s.scenes={a,b};selectProfile(s,0);
        QString error;const auto config=root.filePath("settings.json");check(saveSettings(config,s,error),"save fixture");
        const auto rulePath=root.filePath("host-rules-v1.json");QJsonArray rules{QJsonObject{{"enabled",true},{"eventType","COMMAND_ACKNOWLEDGED"},{"sceneIds",QJsonArray{"s1"}},{"profileId","p1"},{"mode","save"}},QJsonObject{{"enabled",true},{"eventType","COMMAND_ACKNOWLEDGED"},{"sceneIds",QJsonArray{"s2"}},{"profileId","p2"},{"mode","save"}}};check(saveBytesNew(QJsonDocument(QJsonObject{{"schemaVersion",1},{"rules",rules}}).toJson(),rulePath,error),"rules fixture");
        const auto evidence=root.filePath("original-evidence.png");check(saveBytesNew("immutable-source-sentinel",evidence,error),"source sentinel");const auto source=bytes(evidence);
        {
        MainWindow window(nullptr,config,"-");auto* regions=window.findChild<QListWidget*>("sourceCards");auto* scenes=window.findChild<QListWidget*>("enabledScenes");
        regions->selectAll();applyColorMode("light");settle();check(regions->selectedItems().size()==2,"multi-selection survives palette/preview refresh");
        bool emptyMenuChecked=false;
        QTimer::singleShot(0,[&]{auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());check(menu,"real context popup opened");applyColorMode("dark");settle();check(regions->selectedItems().empty() && !window.findChild<QAction*>("sourceCards.delete")->isEnabled() && window.findChild<QAction*>("sourceCards.new")->isEnabled(),"empty context stays unselected during palette refresh");emptyMenuChecked=true;menu->close();});
        regions->customContextMenuRequested(QPoint(-1,-1));settle();check(emptyMenuChecked,"empty context callback completed");regions->selectAll();
        auto before=bytes(config);confirm(false);window.findChild<QPushButton*>("deleteRegions")->click();settle();check(bytes(config)==before && regions->count()==2,"cancel deletion has zero persistent changes");
        confirm(true);window.findChild<QPushButton*>("deleteRegions")->click();settle();Settings disk;check(loadSettings(config,disk,error),"reload region removal");
        check(disk.profiles[0].regions.empty()&&disk.profiles[1].regions.size()==2&&disk.scenes.size()==2,"UI region batch deletion scopes profile and retains templates");
        check(!window.findChild<QPushButton*>("captureSave")->isEnabled(),"capture disabled after delete-all");
        window.findChild<QPushButton*>("addRegion")->click();settle();check(loadSettings(config,disk,error)&&disk.profiles[0].regions.size()==1&&disk.profiles[0].regions[0].displayNumber==7,"new command retains number highwater");
        regions->setCurrentRow(0);window.findChild<QAction*>("sourceCards.duplicate")->trigger();settle();check(loadSettings(config,disk,error)&&disk.profiles[0].regions.size()==2&&disk.profiles[0].regions[1].displayNumber==8,"context duplicate uses fresh identity");
        check(QFile::rename(rulePath,root.filePath("retained-rules.json"))&&QDir().mkdir(rulePath),"inject rules write failure");
        scenes->selectAll();before=bytes(config);confirm(true);window.findChild<QPushButton*>("deleteScenes")->click();settle();
        check(bytes(config)==before && scenes->count()==2,"rules staging failure preserves settings and UI");
        check(QDir().rmdir(rulePath)&&QFile::rename(root.filePath("retained-rules.json"),rulePath),"restore fixture rules path");
        scenes->selectAll();confirm(true);window.findChild<QPushButton*>("deleteScenes")->click();settle();
        check(loadSettings(config,disk,error)&&disk.scenes.empty()&&disk.profiles[1].sceneIds.empty(),"scene batch delete clears all profile references");
        check(!QJsonDocument::fromJson(bytes(rulePath)).object()["rules"].toArray()[0].toObject()["enabled"].toBool(),"dependent rule persisted disabled");
        QTimer::singleShot(0,[&]{auto* dialog=dynamic_cast<QDialog*>(app.activeModalWidget());if(!dialog)return;auto* list=dialog->findChild<QListWidget*>("managedProfiles");list->selectAll();confirm(true);dialog->findChild<QPushButton*>("profiles.delete")->click();check(list->count()==1,"manager reload after batch delete");dialog->reject();});
        window.findChild<QPushButton*>("manageProfiles")->click();settle();
        check(loadSettings(config,disk,error)&&disk.profiles.size()==1&&disk.profiles[0].regions.empty(),"delete all profiles retains empty container after reload");
        QTimer::singleShot(0,[]{auto* input=dynamic_cast<QInputDialog*>(QApplication::activeModalWidget());if(input){input->setTextValue("New blank");input->accept();}});
        window.findChild<QPushButton*>("newProfile")->click();settle();check(loadSettings(config,disk,error)&&disk.profiles.size()==2&&disk.profiles[disk.activeProfile].regions.empty(),"new profile is blank rather than implicit clone");
        check(bytes(evidence)==source,"management never changes source evidence");
        }
        {
            MainWindow reopened(nullptr,config,"-");const auto originalRules=bytes(rulePath);bool checked=false;
            QTimer::singleShot(0,[&]{auto* dialog=dynamic_cast<QDialog*>(app.activeModalWidget());if(!dialog)return;auto* table=dialog->findChild<QTableWidget*>("eventRules");
                check(table && table->rowCount()==2,"disabled dangling rules survive restart");
                check(qobject_cast<QComboBox*>(table->cellWidget(0,6))->currentData().toString()=="p1","missing profile reference retained in editor");
                table->setCurrentCell(0,1);dialog->findChild<QAction*>("rules.duplicate")->trigger();check(table->rowCount()==3,"duplicate rule via action");
                table->selectAll();dialog->findChild<QAction*>("rules.delete")->trigger();check(table->rowCount()==0,"rule batch delete via action");checked=true;dialog->reject();});
            reopened.findChild<QPushButton*>("editEventRules")->click();check(checked && bytes(rulePath)==originalRules,"cancel rule editing preserves exact file bytes");
            QTimer::singleShot(0,[&]{auto* dialog=dynamic_cast<QDialog*>(app.activeModalWidget());if(dialog)dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
            reopened.findChild<QPushButton*>("editEventRules")->click();const auto retained=QJsonDocument::fromJson(bytes(rulePath)).object()["rules"].toArray();
            check(retained.size()==2 && retained[0].toObject()["profileId"]=="p1" && !retained[0].toObject()["enabled"].toBool(),"saving disabled missing reference is explicit and remains disabled");
        }
        check(saveBytesNew(bytes(config),config+".before-management.json",error),"interrupted operation recovery fixture");
        { MainWindow held(nullptr,config,"-");settle();const auto previous=bytes(config);held.findChild<QPushButton*>("addRegion")->click();held.findChild<QPushButton*>("applySettings")->click();held.triggerCapture(0);settle();check(bytes(config)==previous,"restart preserves interrupted operation instead of silently overwriting"); }
        std::cout<<"MANAGEMENT_PASS real actions cancel/batch/scoped refs rules-write failure empty restart new/duplicate\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
