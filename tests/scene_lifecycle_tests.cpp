// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "main_window.h"
#include "desktop/scene_editor.h"
#include "desktop/appearance.h"
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int main(int argc, char** argv)
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    QApplication app(argc, argv);
    try {
        check(app.platformName() == "offscreen", "lifecycle test requires offscreen; no desktop inputs");
        installLanguage("en-US"); applyColorMode("dark");
        QTemporaryDir root; check(root.isValid(), "fixture directory");
        auto settings = defaultSettings(); settings.outputDirectory = root.path();
        settings.saveHotkey="Ctrl+Alt+Shift+F20"; settings.copyHotkey="Ctrl+Alt+Shift+F21"; settings.bothHotkey="Ctrl+Alt+Shift+F22";
        core::CaptureProfile profile; profile.id="lifecycle-profile"; profile.name="Lifecycle";
        profile.regions={{"r1","One",{0,0,64,32},true,1},{"r2","Two",{64,0,64,32},true,2}};
        settings.profiles.push_back(profile); QString error; const auto path=root.filePath("settings.json");
        check(saveSettings(path,settings,error), "fixture persistence");
        MainWindow window(nullptr,path,"-");
        for (int i=0; i<12; ++i) {
            bool handled=false; const bool cancel=i%3==2;
            QTimer::singleShot(0, &window, [&] {
                auto* editor=dynamic_cast<SceneEditor*>(app.activeModalWidget());
                if (!editor) return;
                auto* layers=editor->findChild<QListWidget*>("sceneLayers");
                if (i%2==0) layers->selectAll();
                handled=true;
                if (cancel) editor->reject(); else editor->findChild<QPushButton*>("saveScene")->click();
            });
            std::cout << "OPEN " << i << " cancel=" << cancel << std::endl;
            window.findChild<QPushButton*>("newScene")->click();
            std::cout << "RETURN " << i << std::endl;
            check(handled,"modal Save/Cancel handled");
            for (int tick=0; tick<5; ++tick) { app.processEvents(); QThread::msleep(10); }
            Settings disk; check(loadSettings(path,disk,error), "settings readable after editor destruction");
            const int expected=(i+1)-(i+1)/3;
            check(int(disk.scenes.size())==expected,"save persists exactly once and cancel persists nothing");
        }
        for (int i=0; i<4; ++i) {
            Settings before; check(loadSettings(path,before,error),"read existing scene");
            window.findChild<QListWidget*>("enabledScenes")->setCurrentRow(0);
            const bool cancel=i==3; bool handled=false;
            QTimer::singleShot(0,&window,[&] {
                auto* editor=dynamic_cast<SceneEditor*>(app.activeModalWidget()); if (!editor) return;
                editor->findChild<QListWidget*>("sceneLayers")->selectAll(); handled=true;
                if (cancel) editor->reject(); else editor->findChild<QPushButton*>("saveScene")->click();
            });
            window.findChild<QPushButton*>("editScene")->click(); check(handled,"existing scene editor handled");
            app.processEvents(); Settings after; check(loadSettings(path,after,error),"existing scene reload");
            check(after.scenes.size()==before.scenes.size(),"editing does not duplicate scene");
            check(after.scenes.front().revision==before.scenes.front().revision+(cancel?0:1),"exact existing scene save/cancel revision");
        }
        Settings saved; check(loadSettings(path,saved,error),"real-image scene fixture");
        QImage image(64,32,QImage::Format_RGB32); image.fill(Qt::red);
        SceneSources images{{"r1",{image,"fixture-one"}},{"r2",{image,"fixture-two"}}};
        for (bool cancel : {false,true}) {
            SceneEditor editor(saved.scenes.front(),profile,images);
            editor.findChild<QListWidget*>("sceneLayers")->selectAll();
            QTimer::singleShot(0,&editor,[&] { if (cancel) editor.reject(); else editor.findChild<QPushButton*>("saveScene")->click(); });
            check(editor.exec()==(cancel?QDialog::Rejected:QDialog::Accepted),"real image save/cancel returns");
        }
        std::cout << "SCENE_LIFECYCLE_PASS save/cancel selected/unselected persist/rebuild/destruction/reopen" << std::endl;
    } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
