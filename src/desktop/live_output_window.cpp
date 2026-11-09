// SPDX-License-Identifier: Apache-2.0
#include "live_output_window.h"
#include "workspace_views.h"
#include "capture_visibility.h"
#include "appearance.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
namespace aitsnap {
LiveOutputWindow::LiveOutputWindow(QString key,QString label,QWidget* parent):QWidget(parent,Qt::Window),key_(std::move(key))
{
    setObjectName("liveOutputWindow");setWindowTitle(uiText("AITSnap · 实时图窗")+" · "+label);if(parent)setWindowIcon(parent->windowIcon());setAttribute(Qt::WA_ShowWithoutActivating);resize(560,380);
    auto* layout=new QVBoxLayout(this);preview_=new ImagePreview;preview_->setObjectName("liveOutputPreview");layout->addWidget(preview_,1);
    rate_=new QLabel;rate_->setObjectName("liveOutputRate");layout->addWidget(rate_);
    auto* controls=new QHBoxLayout;auto* topmost=new QCheckBox(uiText("置顶"));controls->addWidget(topmost);pause_=new QPushButton(uiText("暂停"));pause_->setObjectName("liveOutputPause");pause_->setCheckable(true);controls->addWidget(pause_);controls->addStretch();capture_=new QPushButton(uiText("截图并保存全部"));capture_->setObjectName("liveOutputCapture");controls->addWidget(capture_);layout->addLayout(controls);
    connect(topmost,&QCheckBox::toggled,this,[this](bool on){const bool visible=isVisible();setWindowFlag(Qt::WindowStaysOnTopHint,on);if(visible)show();});
    connect(pause_,&QPushButton::toggled,this,[this](bool on){pause_->setText(on?uiText("恢复"):uiText("暂停"));preview_->setImage(image_,on?uiText("预览已暂停")+" · "+caption_:caption_);if(subscriptionChanged)subscriptionChanged();});
    connect(capture_,&QPushButton::clicked,this,[this]{if(captureRequested)captureRequested();});
    preview_->imagePainted=[this](qint64 id){if(id!=lastPainted_ && !paused() && frameTime_){lastPainted_=id;++painted_;++totalPainted_;}};
    auto* timer=new QTimer(this);timer->setInterval(1000);
    connect(timer,&QTimer::timeout,this,[this]{
        const auto elapsed=paintClock_.restart();
        const int actual=elapsed>0?qRound(1000.0*painted_/elapsed):0;
        painted_=0;
        rate_->setText(targetFps_>0 ? uiText("目标 %1 Hz · 新画面 %2 FPS").arg(targetFps_).arg(actual)+
            (compatibility_?uiText(" · 兼容取帧"):QString{}) : QString{});
    });
    paintClock_.start();cadence_.start();timer->start();
}
bool LiveOutputWindow::paused()const{return pause_->isChecked();}
int LiveOutputWindow::screenRate() const
{
    const auto* screen=windowHandle()?windowHandle()->screen():nullptr;
    return screen && screen->refreshRate()>1 ? qRound(screen->refreshRate()) : 60;
}
void LiveOutputWindow::observeScreen()
{
    if(!windowHandle())return;
    if(observedWindow_!=windowHandle()){
        if(observedWindow_)disconnect(observedWindow_,&QWindow::screenChanged,this,nullptr);
        observedWindow_=windowHandle();
        connect(observedWindow_,&QWindow::screenChanged,this,[this]{observeScreen();if(subscriptionChanged)subscriptionChanged();});
    }
    disconnect(refreshConnection_);
    if(auto* screen=windowHandle()->screen())
        refreshConnection_=connect(screen,&QScreen::refreshRateChanged,this,[this]{if(subscriptionChanged)subscriptionChanged();});
}
void LiveOutputWindow::displayFrame(QImage image,QString caption,qint64 frameTime,int targetFps,const QString& compatibilityReason)
{
    if(paused())return;
    if(targetFps_!=targetFps)nextFrame_=0;
    targetFps_=targetFps;compatibility_=!compatibilityReason.isEmpty();
    if(rate_->toolTip()!=compatibilityReason)rate_->setToolTip(compatibilityReason);
    if(targetFps>0 && !image.isNull() && frameTime){
        const auto now=cadence_.nsecsElapsed();
        if(frameTime==frameTime_ || now<nextFrame_)return;
        if(image==image_){frameTime_=frameTime;return;}
        const qint64 period=1000000000LL/targetFps;
        nextFrame_=std::max(nextFrame_+period,now-period/2);
    }else nextFrame_=0;
    frameTime_=frameTime;
    image_=std::move(image);caption_=std::move(caption);preview_->setImage(image_,caption_);
}
void LiveOutputWindow::setCaptureEnabled(bool enabled){capture_->setEnabled(enabled);}
void LiveOutputWindow::showEvent(QShowEvent* event){QWidget::showEvent(event);excludeOwnCaptureWindow(*this);observeScreen();if(subscriptionChanged)subscriptionChanged();}
void LiveOutputWindow::moveEvent(QMoveEvent* event){QWidget::moveEvent(event);if(subscriptionChanged)subscriptionChanged();}
void LiveOutputWindow::changeEvent(QEvent* event){QWidget::changeEvent(event);if(event->type()==QEvent::WindowStateChange && subscriptionChanged)subscriptionChanged();}
void LiveOutputWindow::closeEvent(QCloseEvent* event){hide();image_=QImage{};preview_->setImage({},uiText("预览已暂停"));event->ignore();if(subscriptionChanged)subscriptionChanged();}
}
