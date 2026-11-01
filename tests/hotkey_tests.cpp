// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "adapters/hotkeys.h"
#include "adapters/json_store.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    try{
        QString error;Hotkeys keys;
        std::vector<HotkeyBinding> bindings{{"Ctrl+Alt+Shift+F13",0},{"F14",0},{"Ctrl+Alt+Shift+F15",1},{"F16",1},{"Ctrl+Alt+Shift+F17",2},{"F18",2}};
        check(keys.configureBindings(bindings,error),"six bindings register");
        int action=-1;keys.triggered=[&](int a){action=a;};
        for(int slot=0;slot<6;++slot){MSG message{};message.message=WM_HOTKEY;message.wParam=0x4150+slot;keys.nativeEventFilter({},&message,nullptr);check(action==slot/2,"both slots map to same action");}
        auto duplicate=bindings;duplicate.push_back({"ctrl+alt+shift+f13",0});check(keys.configureBindings(duplicate,error),"same action normalized duplicate merged");
        duplicate.push_back({"CTRL+ALT+SHIFT+F13",2});check(!keys.configureBindings(duplicate,error) && !error.isEmpty(),"cross-action duplicate rejected");
        check(!keys.configureBindings({{"F12",0}},error),"reserved F12 rejected");
        check(RegisterHotKey(nullptr,0x4600,MOD_ALT|MOD_SHIFT,VK_F19),"reserve external collision");
        check(!keys.configureBindings({{"Alt+Shift+F19",1}},error),"registration conflict");
        check(!RegisterHotKey(nullptr,0x4601,MOD_CONTROL|MOD_ALT|MOD_SHIFT,VK_F13),"old binding restored after failure");
        UnregisterHotKey(nullptr,0x4600);
        check(keys.pause(true,error),"pause all slots");
        check(RegisterHotKey(nullptr,0x4600,MOD_CONTROL|MOD_ALT|MOD_SHIFT,VK_F13),"paused old key released");
        check(keys.configureBindings(bindings,error) && !keys.registered(),"configuration while paused never re-registers");
        check(!keys.pause(false,error),"resume conflict visible");UnregisterHotKey(nullptr,0x4600);
        check(keys.pause(false,error),"resume all slots after conflict removed");
        check(keys.configureBindings({},error),"all actions can disable global keys");
        check(RegisterHotKey(nullptr,0x4600,0,VK_F14),"disabled alternate released");UnregisterHotKey(nullptr,0x4600);
        QTemporaryDir temp;check(temp.isValid(),"settings temp");auto s=defaultSettings();
        core::CaptureProfile profile;profile.id="p";profile.name="P";profile.regions.push_back({"r","R",{0,0,16,16},true});s.profiles={profile};
        s.saveHotkey="Ctrl+Alt+Shift+F13";s.saveHotkeyAlternate="F14";s.copyHotkeyAlternate="F16";s.bothHotkeyAlternate="F18";s.hotkeysEnabled[1]=false;
        const auto path=temp.filePath("settings.json");check(saveSettings(path,s,error),"save dual settings");Settings loaded;check(loadSettings(path,loaded,error),"load dual settings");
        check(loaded.saveHotkey==s.saveHotkey && loaded.saveHotkeyAlternate=="F14" && !loaded.hotkeysEnabled[1] && hotkeyBindings(loaded).size()==4,"restart bindings and enabled state");
        QFile file(path);check(file.open(QIODevice::ReadOnly),"read v5");auto old=QJsonDocument::fromJson(file.readAll()).object();file.close();old["schemaVersion"]=4;old.remove("alternateHotkeys");old.remove("hotkeysEnabled");
        const auto oldBytes=QJsonDocument(old).toJson();check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"write legacy fixture");file.write(oldBytes);file.close();
        check(loadSettings(path,loaded,error) && loaded.saveHotkey==s.saveHotkey && loaded.saveHotkeyAlternate.isEmpty() && loaded.hotkeysEnabled[1],"legacy defaults preserve primary");
        check(saveSettings(path,loaded,error),"migrate v4");const auto backups=QDir(temp.path()).entryList({"*.v4-preserved-*"},QDir::Files);check(backups.size()==1,"migration backup");
        QFile backup(temp.filePath(backups[0]));check(backup.open(QIODevice::ReadOnly) && backup.readAll()==oldBytes,"old bytes preserved");
        std::cout<<"DUAL_HOTKEY_PASS six-slot dispatch, normalized conflicts, atomic rollback, pause/configure/resume, disabled release, legacy migration\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
