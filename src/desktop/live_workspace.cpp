// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "adapters/live_preview.h"
#include "desktop/capture_visibility.h"
#include "desktop/live_output_window.h"
#include "desktop/scene_editor.h"
#include "desktop/appearance.h"
#include <QCheckBox>
#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QGuiApplication>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <algorithm>
namespace aitsnap {
void MainWindow::showEvent(QShowEvent* e){QMainWindow::showEvent(e);excludeOwnCaptureWindow(*this);QTimer::singleShot(0,this,[this]{refreshWorkspace();});}
void MainWindow::hideEvent(QHideEvent* e){QMainWindow::hideEvent(e);QTimer::singleShot(0,this,[this]{refreshWorkspace();});}
void MainWindow::syncLivePreview(const core::CaptureProfile& profile)
{
    if(!livePreview_)return;
    const bool workspace=livePreviewEnabled_->isChecked() && ((isVisible() && pages_->currentIndex()<=2) || liveEditor_);
    const bool output=std::any_of(liveOutputs_.begin(),liveOutputs_.end(),[](const auto* window){return window->isVisible() && !window->paused();});
    livePreview_->setFps(livePreviewRate_->currentData().toInt());livePreview_->configure(profile,unexcludedOwnWindowRects());
    livePreview_->setActive((workspace || output) && QGuiApplication::platformName()=="windows" && !busy_ && !paused_ && !editingRegions_ && !managingObjects_ && !configurationBlocked_);
    const auto& snapshot=livePreview_->snapshot();
    const auto time=QDateTime::fromString(snapshot.acquiredAtUtc,Qt::ISODateWithMs).toLocalTime().toString("HH:mm:ss.zzz");
    livePreviewStatus_->setText(livePreviewEnabled_->isChecked()?(snapshot.state+(time.isEmpty()?QString{}:" · "+time)):uiText("最近截图 · 实时预览已关闭"));
    if(liveEditor_)liveEditor_->setPreviewSources(snapshot.sources);
}
void MainWindow::openLiveOutput(const QString& explicitKey)
{
    if(busy_ || editingRegions_ || editingScene_)return;
    if(!applySettings())return;
    auto key=explicitKey.isEmpty()?overviewOutputKey_:explicitKey;
    if(explicitKey.isEmpty() && pages_->currentIndex()==2 && scenes_->currentItem())key="scene:"+scenes_->currentItem()->data(Qt::UserRole).toString();
    else if(explicitKey.isEmpty() && pages_->currentIndex()==1 && regions_->currentRow()>=0)key=regions_->item(regions_->currentRow(),1)->data(Qt::UserRole).toString();
    if(key.isEmpty() && regions_->currentRow()>=0)key=regions_->item(regions_->currentRow(),1)->data(Qt::UserRole).toString();
    if(key.isEmpty()){setStatus(uiText("请先选择源区域或拼图场景。"),true);return;}
    for(auto* window:liveOutputs_)if(window->key()==key){window->show();window->raise();refreshWorkspace();return;}
    liveOutputs_.erase(std::remove_if(liveOutputs_.begin(),liveOutputs_.end(),[](auto* window){if(window->isVisible())return false;delete window;return true;}),liveOutputs_.end());
    if(liveOutputs_.size()>=8){setStatus(uiText("最多保留 8 个实时图窗，请复用已打开的图窗。"),true);return;}
    QString label;
    if(key.startsWith("scene:")){const auto id=key.mid(6).toStdString();for(const auto& scene:settings_.scenes)if(scene.id==id)label=QString::fromStdString(scene.name);}
    else for(const auto& region:settings_.profiles[std::size_t(settings_.activeProfile)].regions)if(region.id==key.toStdString())label=regionLabel(region);
    if(label.isEmpty()){setStatus(uiText("当前配置中没有这个来源。"),true);return;}
    auto* window=new LiveOutputWindow(key,label,this);liveOutputs_.push_back(window);
    window->captureRequested=[this]{captureActions_[0]->trigger();};window->subscriptionChanged=[this]{QTimer::singleShot(0,this,[this]{refreshWorkspace();updateLiveOutputs();});};
    window->show();refreshWorkspace();updateLiveOutputs();
}
void MainWindow::updateLiveOutputs()
{
    if(!livePreview_)return;const auto& snapshot=livePreview_->snapshot();
    const auto time=QDateTime::fromString(snapshot.acquiredAtUtc,Qt::ISODateWithMs).toLocalTime().toString("HH:mm:ss.zzz");
    for(auto* window:liveOutputs_){window->setCaptureEnabled(captureActions_[0]->isEnabled());if(!window->isVisible() || window->paused())continue;
        QImage image;QString caption=snapshot.state+(time.isEmpty()?QString{}:" · "+time);const auto key=window->key();
        if(key.startsWith("scene:")){
            const auto id=key.mid(6).toStdString();const auto scene=std::find_if(settings_.scenes.begin(),settings_.scenes.end(),[&](const auto& s){return s.id==id;});
            if(scene!=settings_.scenes.end()){const auto rendered=renderScenePreview(*scene,snapshot.sources,{1600,1000});image=rendered.image;caption=QString::fromStdString(scene->name)+" · "+caption;if(!rendered.complete)caption+=" · "+uiText("部分来源不可用");}
            else caption=uiText("当前配置中没有这个来源。");
        }else{
            const auto source=snapshot.sources.find(key.toStdString());if(source!=snapshot.sources.end())image=source->second.image.scaled({1600,1000},Qt::KeepAspectRatio,Qt::SmoothTransformation);
            const auto issue=snapshot.issues.find(key.toStdString());if(issue!=snapshot.issues.end())caption=issue->second;
        }
        window->displayFrame(image,caption);
    }
}
}
