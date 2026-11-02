// SPDX-License-Identifier: Apache-2.0
#include "adapters/display_profiles.h"
#include "adapters/settings_commands.h"
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "main_window.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <algorithm>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static void settle(){for(int i=0;i<35;++i){QCoreApplication::processEvents();QThread::msleep(10);}}
static std::vector<core::Display> layout(int n){std::vector<core::Display> out;for(int i=0;i<n;++i)out.push_back({"monitor"+std::to_string(i),{(i%2)*800,(i/2)*600,800,600}});return out;}
static core::CaptureProfile profile(const char* id,int screens,int x=10){core::CaptureProfile p;p.id=id;p.name=id;p.calibratedDisplays=layout(screens);p.regions={{"r","Voltage",{x,30,32,16},true,1}};p.nextRegionNumber=2;return p;}
static Settings fixture(const QString& output){auto s=defaultSettings();s.outputDirectory=output;s.profiles={profile("four",4),profile("two",2)};s.hotkeysEnabled={false,false,false};selectProfile(s,0);return s;}
static QImage render(DisplayCanvas& canvas){QImage image(canvas.size(),QImage::Format_ARGB32);image.fill(Qt::transparent);canvas.render(&image);return image;}
int main(int argc,char** argv){QApplication app(argc,argv);try{
    QTemporaryDir temp;check(temp.isValid(),"temporary directory");QString error;auto s=fixture(temp.path());
    auto two=layout(2),four=layout(4);auto reverse=two;std::reverse(reverse.begin(),reverse.end());check(displayLayoutKey(two)==displayLayoutKey(reverse),"order independent");
    auto alias=two;alias[0].id="\\\\.\\DISPLAY7:monitor0";check(displayLayoutKey(two)==displayLayoutKey(alias),"logical display alias normalized");
    for(int kind=0;kind<4;++kind){auto changed=two;if(kind==0)++changed[0].rect.x;if(kind==1)++changed[0].rect.width;if(kind==2)changed[0].rotation=90;if(kind==3)changed[0].id="different-hardware";check(displayLayoutKey(two)!=displayLayoutKey(changed),"physical identity geometry rotation distinguish layouts");}
    check(displayLayoutKey({}).isEmpty(),"zero monitors unknown");auto invalid=two;invalid[0].rect.width=0;check(displayLayoutKey(invalid).isEmpty(),"invalid monitor unknown");
    const auto memoryPath=temp.filePath("memory.json");DisplayProfiles memory(memoryPath);check(memory.seed(s,error) && memory.count()==2,"unique historical layout seed");Settings next;
    check(memory.restoration(s,two,next,error) && next.activeProfile==1,"four to known two selects bound profile");check(!memory.restoration(next,layout(3),s,error),"unknown layout does not switch");
    auto same=fixture(temp.path());same.profiles={profile("same",4,11)};selectProfile(same,0);DisplayProfiles snapshots(temp.filePath("snapshots.json"));check(snapshots.remember(same.profiles[0],error),"remember four snapshot");
    same.profiles[0]=profile("same",2,22);same.profiles[0].revision=50;same.profiles[0].nextRegionNumber=99;selectProfile(same,0);check(snapshots.remember(same.profiles[0],error),"remember same profile two snapshot");
    DisplayProfiles restarted(temp.filePath("snapshots.json"));check(restarted.restoration(same,four,next,error) && next.profiles[0].regions[0].rect.x==11,"persisted same-profile four geometry restored");
    check(next.profiles[0].revision==51 && next.profiles[0].nextRegionNumber==99,"revision and region numbering monotonic");same=next;
    check(restarted.restoration(same,two,next,error) && next.profiles[0].regions[0].rect.x==22,"same profile two geometry restored");
    auto latest=fixture(temp.path());latest.profiles[1].regions[0].name="offline edit";check(memory.restoration(latest,two,next,error) && next.profiles[1].regions[0].name=="offline edit","latest calibrated profile wins over old snapshot");
    auto deleted=fixture(temp.path());deleted.profiles.erase(deleted.profiles.begin()+1);error.clear();check(!memory.restoration(deleted,two,next,error) && !error.isEmpty(),"deleted profile not resurrected");
    auto stale=profile("same",4);stale.sceneIds={"deleted-scene"};stale.outputMode="both";stale.clipboardSelection="scene:deleted-scene";check(snapshots.remember(stale,error),"scene snapshot");
    same=fixture(temp.path());same.profiles={profile("same",2)};selectProfile(same,0);check(snapshots.restoration(same,four,next,error) && next.profiles[0].sceneIds.empty() && next.profiles[0].outputMode=="originals" && next.clipboardRegion.isEmpty(),"deleted scene references filtered");
    check(memory.forget(two,error),"forget layout");DisplayProfiles forgotten(memoryPath);auto seed=fixture(temp.path());check(forgotten.seed(seed,error) && forgotten.boundProfile(two).isEmpty(),"forgotten binding remains forgotten after restart seed");
    check(forgotten.remember(seed.profiles[1],error) && forgotten.boundProfile(two)=="two","explicit remember restores forgotten binding");check(forgotten.setEnabled(false,error),"disable restore");DisplayProfiles disabled(memoryPath);check(!disabled.enabled() && !disabled.restoration(seed,two,next,error),"disabled persists");
    auto ambiguous=fixture(temp.path());ambiguous.profiles.push_back(profile("other-two",2));DisplayProfiles amb(temp.filePath("amb.json"));check(amb.seed(ambiguous,error) && amb.boundProfile(two).isEmpty(),"ambiguous inactive layout not guessed");
    DisplayProfiles active(temp.filePath("active.json"));selectProfile(ambiguous,2);check(active.seed(ambiguous,error) && active.boundProfile(two)=="other-two","active wins ambiguous historical seed");
    QFile bad(temp.filePath("bad.json"));check(bad.open(QIODevice::WriteOnly),"corrupt file open");bad.write("broken-original");bad.close();DisplayProfiles corrupt(bad.fileName());check(!corrupt.ready() && !corrupt.seed(seed,error),"corrupt memory lane unavailable");check(bad.open(QIODevice::ReadOnly) && bad.readAll()=="broken-original","corrupt original preserved");bad.close();
    QFile blocker(temp.filePath("blocker"));check(blocker.open(QIODevice::WriteOnly),"block path");blocker.close();DisplayProfiles unwritable(blocker.fileName()+"/memory.json");check(!unwritable.remember(seed.profiles[0],error) && unwritable.count()==0,"failed write rolls back in-memory binding");
    DisplayCanvas canvas;canvas.resize(800,600);canvas.setLayout(profile("four",4),two,0);const auto historyFour=render(canvas);canvas.setLayout(profile("four",2),two,0);const auto historyTwo=render(canvas);
    check(historyFour.copy(0,65,800,535)==historyTwo.copy(0,65,800,535),"canvas body only uses current two displays regardless of four history");canvas.setLayout(profile("four",4),{},0);const auto zero=render(canvas);canvas.setLayout({}, {},-1);check(zero==render(canvas) && canvas.regionAt({20,30})==-1,"zero displays never draw or select historical screens");
    const auto config=temp.filePath("ui/settings.json");auto ui=fixture(temp.path());check(saveSettings(config,ui,error),"UI fixture save");auto inventory=two;
    {
        MainWindow window(nullptr,config,"-",{},false,[&]{return inventory;});settle();auto* profiles=window.findChild<QComboBox*>("profiles");auto* status=window.findChild<QLabel*>("displayBindingStatus");auto* timer=window.findChild<QTimer*>("displayProfileRefresh");
        check(profiles->currentIndex()==1,"startup restores known current two profile");check(status->text().contains("2"),"current display count status");
        inventory=four;QMetaObject::invokeMethod(timer,"timeout");settle();check(profiles->currentIndex()==0,"runtime event restores four");
        auto* directory=window.findChild<QLineEdit*>("outputDirectory");check(directory,"output directory control");directory->setText(temp.filePath("draft-output"));inventory=two;QMetaObject::invokeMethod(timer,"timeout");settle();
        check(profiles->currentIndex()==0 && directory->text().endsWith("draft-output"),"unsaved draft defers restore without losing edits");window.findChild<QPushButton*>("applySettings")->click();settle();check(profiles->currentIndex()==1,"apply draft retries pending restore");
        Settings saved;check(loadSettings(config,saved,error) && saved.outputDirectory.endsWith("draft-output"),"global draft saved before restore");
        bool deferredInEditor=false;QTimer::singleShot(40,&window,[&]{inventory=four;QMetaObject::invokeMethod(timer,"timeout");deferredInEditor=profiles->currentIndex()==1;for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QDialog*>(widget))dialog->reject();});
        window.findChild<QPushButton*>("newScene")->click();settle();check(deferredInEditor && profiles->currentIndex()==0,"modal editor defers restore until editor closes");
        inventory=two;QMetaObject::invokeMethod(timer,"timeout");settle();check(profiles->currentIndex()==1,"return to two after editor regression");
        window.findChild<QPushButton*>("forgetDisplayProfile")->click();settle();DisplayProfiles removed(temp.filePath("ui/display-layouts-v1.json"));check(removed.boundProfile(two).isEmpty(),"UI forget persisted");
        window.findChild<QPushButton*>("bindDisplayProfile")->click();settle();DisplayProfiles rebound(temp.filePath("ui/display-layouts-v1.json"));check(rebound.boundProfile(two)=="two","UI explicit binding persisted");
        window.findChild<QCheckBox*>("displayAutoRestore")->setChecked(false);inventory=four;QMetaObject::invokeMethod(timer,"timeout");settle();check(profiles->currentIndex()==1,"disabled auto restore keeps selected profile");
    }
    std::cout<<"PASS display layout fingerprints, snapshots, preservation, current-only canvas and UI deferred restoration\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
