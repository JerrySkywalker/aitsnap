// SPDX-License-Identifier: Apache-2.0
#include "live_output_window.h"
#include "workspace_views.h"
#include "capture_visibility.h"
#include "appearance.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
namespace aitsnap {
LiveOutputWindow::LiveOutputWindow(QString key,QString label,QWidget* parent):QWidget(parent,Qt::Window),key_(std::move(key))
{
    setObjectName("liveOutputWindow");setWindowTitle(uiText("AITSnap · 实时图窗")+" · "+label);if(parent)setWindowIcon(parent->windowIcon());setAttribute(Qt::WA_ShowWithoutActivating);resize(560,380);
    auto* layout=new QVBoxLayout(this);preview_=new ImagePreview;preview_->setObjectName("liveOutputPreview");layout->addWidget(preview_,1);
    auto* controls=new QHBoxLayout;auto* topmost=new QCheckBox(uiText("置顶"));controls->addWidget(topmost);pause_=new QPushButton(uiText("暂停"));pause_->setObjectName("liveOutputPause");pause_->setCheckable(true);controls->addWidget(pause_);controls->addStretch();capture_=new QPushButton(uiText("截图并保存全部"));capture_->setObjectName("liveOutputCapture");controls->addWidget(capture_);layout->addLayout(controls);
    connect(topmost,&QCheckBox::toggled,this,[this](bool on){const bool visible=isVisible();setWindowFlag(Qt::WindowStaysOnTopHint,on);if(visible)show();});
    connect(pause_,&QPushButton::toggled,this,[this](bool on){pause_->setText(on?uiText("恢复"):uiText("暂停"));preview_->setImage(image_,on?uiText("预览已暂停")+" · "+caption_:caption_);if(subscriptionChanged)subscriptionChanged();});
    connect(capture_,&QPushButton::clicked,this,[this]{if(captureRequested)captureRequested();});
}
bool LiveOutputWindow::paused()const{return pause_->isChecked();}
void LiveOutputWindow::displayFrame(QImage image,QString caption){if(paused())return;image_=std::move(image);caption_=std::move(caption);preview_->setImage(image_,caption_);}
void LiveOutputWindow::setCaptureEnabled(bool enabled){capture_->setEnabled(enabled);}
void LiveOutputWindow::showEvent(QShowEvent* event){QWidget::showEvent(event);excludeOwnCaptureWindow(*this);if(subscriptionChanged)subscriptionChanged();}
void LiveOutputWindow::closeEvent(QCloseEvent* event){hide();image_=QImage{};preview_->setImage({},uiText("预览已暂停"));event->ignore();if(subscriptionChanged)subscriptionChanged();}
}
