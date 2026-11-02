// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "adapters/display_profiles.h"
#include "adapters/settings_commands.h"
#include "adapters/live_preview.h"
#include "desktop/appearance.h"
#include <QApplication>
#include <QCheckBox>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QScreen>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>
#include <exception>
namespace aitsnap {
static QByteArray draftIdentity(const Settings& settings){
    auto root=settingsJson(settings);root.remove("sequence");auto profiles=root["profiles"].toArray();
    for(int i=0;i<profiles.size();++i){auto p=profiles[i].toObject();p.remove("revision");profiles[i]=p;}
    root["profiles"]=profiles;return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
void MainWindow::captureDisplayDraftBaseline(){Settings draft=settings_;QString error;if(collectSettings(draft,error))displayDraftBaseline_=draftIdentity(draft);else displayDraftBaseline_.clear();}
bool MainWindow::hasDisplayDraft(){Settings draft=settings_;QString error;return displayDraftBaseline_.isEmpty() || !collectSettings(draft,error) || draftIdentity(draft)!=displayDraftBaseline_;}
void MainWindow::setupDisplayProfiles(){
    displayProfiles_=std::make_unique<DisplayProfiles>(QFileInfo(configurationPath_).absolutePath()+"/display-layouts-v1.json");
    QString error;if(!configurationBlocked_ && !displayProfiles_->seed(settings_,error))displayMemoryWarning_=error;
    {const QSignalBlocker block(autoDisplayProfile_);autoDisplayProfile_->setChecked(displayProfiles_->enabled());autoDisplayProfile_->setEnabled(displayProfiles_->ready());}
    displayChangeTimer_=new QTimer(this);displayChangeTimer_->setObjectName("displayProfileRefresh");displayChangeTimer_->setSingleShot(true);displayChangeTimer_->setInterval(150);
    connect(displayChangeTimer_,&QTimer::timeout,this,[this]{processDisplayChange();});
    const auto watch=[this](QScreen* screen){
        connect(screen,&QScreen::geometryChanged,this,[this]{scheduleDisplayChange();});
        connect(screen,&QScreen::availableGeometryChanged,this,[this]{scheduleDisplayChange();});
        connect(screen,&QScreen::logicalDotsPerInchChanged,this,[this]{scheduleDisplayChange();});
        connect(screen,&QScreen::physicalDotsPerInchChanged,this,[this]{scheduleDisplayChange();});
    };
    for(auto* screen:QGuiApplication::screens())watch(screen);
    connect(qApp,&QGuiApplication::screenAdded,this,[this,watch](QScreen* screen){watch(screen);scheduleDisplayChange();});
    connect(qApp,&QGuiApplication::screenRemoved,this,[this]{scheduleDisplayChange();});
    connect(qApp,&QGuiApplication::primaryScreenChanged,this,[this]{scheduleDisplayChange();});
    hotkeys_.displayChanged=[this]{scheduleDisplayChange();};
    connect(autoDisplayProfile_,&QCheckBox::toggled,this,[this](bool enabled){
        QString error;if(!displayProfiles_->setEnabled(enabled,error)){const QSignalBlocker block(autoDisplayProfile_);autoDisplayProfile_->setChecked(displayProfiles_->enabled());displayMemoryWarning_=error;setStatus(error,true);return;}
        displayMemoryWarning_.clear();displayRestorePending_=enabled;scheduleDisplayChange();
    });
    scheduleDisplayChange();
}
void MainWindow::scheduleDisplayChange(){if(displayChangeTimer_)displayChangeTimer_->start();}
void MainWindow::rememberDisplayProfile(){
    if(configurationBlocked_ || !displayProfiles_ || settings_.profiles.empty() || !displayProfiles_->ready())return;
    try{
        const auto current=displayInventory_();const auto& profile=settings_.profiles[std::size_t(settings_.activeProfile)];
        if(!current.empty() && displayLayoutKey(profile.calibratedDisplays)==displayLayoutKey(current)){
            QString error;if(!displayProfiles_->remember(profile,error))displayMemoryWarning_=error;
        }
    }catch(...){/* Event refresh will report unavailable hardware. */}
}
void MainWindow::processDisplayChange(){
    if(processingDisplays_ || !displayProfiles_)return;QScopedValueRollback<bool> processing(processingDisplays_,true);
    std::vector<core::Display> current;try{current=displayInventory_();}catch(const std::exception& e){setStatus(QString::fromUtf8(e.what()),true);return;}
    if(displayLayoutKey(current)!=displayLayoutKey(observedDisplays_)){
        const auto& profile=settings_.profiles[std::size_t(settings_.activeProfile)];
        if(!configurationBlocked_ && !observedDisplays_.empty() && displayLayoutKey(profile.calibratedDisplays)==displayLayoutKey(observedDisplays_)){
            QString error;if(!displayProfiles_->remember(profile,error))displayMemoryWarning_=error;
        }
        observedDisplays_=current;displayRestorePending_=true;if(livePreview_)livePreview_->setActive(false);
    }
    const bool canRestore=!busy_ && !editingRegions_ && !editingScene_ && !managingObjects_ && !loadingProfile_ && !configurationBlocked_ && !hasDisplayDraft();
    if(displayRestorePending_ && canRestore && displayProfiles_->ready() && displayMemoryWarning_.isEmpty()){
        displayRestorePending_=false;Settings next;QString error;
        if(displayProfiles_->restoration(settings_,current,next,error)){
            if(commitObjects(next,eventRules_))setStatus(uiText("已恢复显示器组合的方案：%1").arg(QString::fromStdString(settings_.profiles[std::size_t(settings_.activeProfile)].name)));
        }else if(!error.isEmpty())setStatus(error,true);
    }
    refreshWorkspace();
}
void MainWindow::refreshDisplayProfileStatus(const std::vector<core::Display>& displays){
    if(!displayProfiles_ || !displayBindingStatus_)return;
    const auto& profile=settings_.profiles[std::size_t(settings_.activeProfile)];
    QString text=uiText("当前 %1 个显示器 · 当前方案记忆 %2 个").arg(displays.size()).arg(profile.calibratedDisplays.size());
    if(!displayProfiles_->ready() || !displayMemoryWarning_.isEmpty())text+=" · "+uiText("布局记忆不可用：%1").arg(displayProfiles_->ready()?displayMemoryWarning_:displayProfiles_->diagnostic());
    else {
        const auto id=displayProfiles_->boundProfile(displays);const auto found=std::find_if(settings_.profiles.begin(),settings_.profiles.end(),[&](const auto& p){return QString::fromStdString(p.id)==id;});
        if(displays.empty())text+=" · "+uiText("当前无活动显示器，未切换方案");
        else if(id.isEmpty())text+=" · "+uiText("当前组合未绑定，请核对区域后确认布局");
        else if(found==settings_.profiles.end())text+=" · "+uiText("绑定方案已删除，请重新绑定");
        else text+=" · "+uiText("本组合绑定：%1").arg(QString::fromStdString(found->name));
        if(displayRestorePending_ && hasDisplayDraft())text+=" · "+uiText("存在未保存设置，自动恢复已延后");
        else if(displayRestorePending_ && (busy_ || editingRegions_ || editingScene_ || managingObjects_))text+=" · "+uiText("正在操作，自动恢复已延后");
        if(!displayProfiles_->enabled())text+=" · "+uiText("自动恢复已关闭");
    }
    displayBindingStatus_->setText(text);
    if(!processingDisplays_ && !loadingProfile_ && displayChangeTimer_ && !displayChangeTimer_->isActive() && displayMemoryWarning_.isEmpty() && displayProfiles_->ready()){
        if(displayLayoutKey(displays)!=displayLayoutKey(observedDisplays_) || (displayRestorePending_ && !busy_ && !editingRegions_ && !editingScene_ && !managingObjects_ && !configurationBlocked_ && !hasDisplayDraft()))scheduleDisplayChange();
    }
}
void MainWindow::bindCurrentDisplayProfile(){
    if(busy_ || editingRegions_ || editingScene_ || managingObjects_){setStatus(uiText("请等待当前截图或编辑完成。"),true);return;}
    if(!displayProfiles_ || !displayProfiles_->ready()){setStatus(uiText("布局记忆不可用，请先核对记忆文件。"),true);return;}
    std::vector<core::Display> current;try{current=displayInventory_();}catch(const std::exception& e){setStatus(QString::fromUtf8(e.what()),true);return;}
    if(current.empty()){setStatus(uiText("当前没有活动显示器，未绑定方案。"),true);return;}
    if(!applySettings())return;auto next=settings_;auto& profile=next.profiles[std::size_t(next.activeProfile)];profile.calibratedDisplays=current;++profile.revision;
    QString error;if(!saveSettings(configurationPath_,next,error)){setStatus(error,true);return;}
    settings_=next;displayRestorePending_=false;
    if(!displayProfiles_->remember(profile,error)){displayMemoryWarning_=error;loadProfile();setStatus(error,true);return;}
    displayMemoryWarning_.clear();loadProfile();setStatus(uiText("已绑定当前显示器组合与方案，请核对物理区域后截图。"));
}
void MainWindow::forgetDisplayProfile(){
    if(!displayProfiles_)return;QString error;std::vector<core::Display> current;try{current=displayInventory_();}catch(const std::exception& e){setStatus(QString::fromUtf8(e.what()),true);return;}
    if(!displayProfiles_->forget(current,error)){displayMemoryWarning_=error;setStatus(error,true);return;}
    displayMemoryWarning_.clear();displayRestorePending_=false;refreshWorkspace();setStatus(uiText("已解除当前组合绑定，方案和历史图片保留。"));
}
}
