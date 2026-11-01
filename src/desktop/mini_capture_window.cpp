// SPDX-License-Identifier: Apache-2.0
#include "mini_capture_window.h"
#include "appearance.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSignalBlocker>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
namespace aitsnap {
MiniCaptureWindow::MiniCaptureWindow(QString path,QWidget* parent)
    :QWidget(parent,Qt::Window),statePath_(std::move(path))
{
    setObjectName("miniCaptureWindow");setWindowTitle(uiText("AITSnap · 迷你截图"));
    setWindowIcon(parent?parent->windowIcon():QIcon());setAttribute(Qt::WA_ShowWithoutActivating);
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(12,10,12,10);layout->setSpacing(6);
    profile_=new QLabel;profile_->setWordWrap(true);layout->addWidget(profile_);
    auto* row=new QHBoxLayout;mode_=new QComboBox;mode_->setObjectName("miniCaptureMode");mode_->addItems({uiText("截图并保存"),uiText("截图并复制"),uiText("截图、保存并复制")});row->addWidget(mode_,1);
    capture_=new QPushButton(uiText("截图"));capture_->setObjectName("miniCaptureButton");capture_->setProperty("primary",true);capture_->setFocusPolicy(Qt::NoFocus);row->addWidget(capture_);layout->addLayout(row);
    status_=new QLabel(uiText("就绪"));status_->setObjectName("miniCaptureStatus");status_->setWordWrap(true);status_->setMaximumWidth(330);layout->addWidget(status_);
    auto* bottom=new QHBoxLayout;topmost_=new QCheckBox(uiText("置顶"));topmost_->setObjectName("miniAlwaysOnTop");bottom->addWidget(topmost_);bottom->addStretch();pause_=new QPushButton(uiText("暂停"));pause_->setCheckable(true);pause_->setObjectName("miniPause");bottom->addWidget(pause_);auto* main=new QPushButton(uiText("打开主窗"));main->setObjectName("miniOpenMain");bottom->addWidget(main);layout->addLayout(bottom);
    QFile preferences(statePath_);QJsonObject saved;if(preferences.open(QIODevice::ReadOnly))saved=QJsonDocument::fromJson(preferences.readAll()).object();
    mode_->setCurrentIndex(std::clamp(saved["mode"].toInt(),0,2));topmost_->setChecked(saved["alwaysOnTop"].toBool(true));
    setWindowFlag(Qt::WindowStaysOnTopHint,topmost_->isChecked());resize(310,160);
    if(saved.contains("x") && saved.contains("y"))move(saved["x"].toInt(),saved["y"].toInt());
    saveTimer_=new QTimer(this);saveTimer_->setSingleShot(true);saveTimer_->setInterval(500);connect(saveTimer_,&QTimer::timeout,this,[this]{savePreferences();});
    connect(mode_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{savePreferences();});
    connect(topmost_,&QCheckBox::toggled,this,[this](bool on){const bool visible=isVisible();setWindowFlag(Qt::WindowStaysOnTopHint,on);if(visible)show();savePreferences();});
    connect(capture_,&QPushButton::clicked,this,[this]{if(captureRequested)captureRequested(mode_->currentIndex());});
    connect(pause_,&QPushButton::toggled,this,[this](bool on){if(pauseRequested)pauseRequested(on);});
    connect(main,&QPushButton::clicked,this,[this]{savePreferences();hide();if(openMain)openMain();});
}
void MiniCaptureWindow::keepVisible()
{
    const auto screens=QGuiApplication::screens();if(screens.empty())return;
    auto area=screens.front()->availableGeometry();
    for(auto* screen:screens)if(screen->availableGeometry().contains(frameGeometry().center())){area=screen->availableGeometry();break;}
    move(std::clamp(x(),area.left(),std::max(area.left(),area.right()-width()+1)),std::clamp(y(),area.top(),std::max(area.top(),area.bottom()-height()+1)));
}
void MiniCaptureWindow::showMini(){keepVisible();show();savePreferences();}
void MiniCaptureWindow::setState(const QString& profile,const QString& status,bool canCapture,bool paused)
{
    profile_->setText(profile);status_->setText(status);capture_->setEnabled(canCapture);mode_->setEnabled(canCapture);
    const QSignalBlocker block(pause_);pause_->setChecked(paused);pause_->setText(paused?uiText("恢复"):uiText("暂停"));
}
void MiniCaptureWindow::savePreferences()
{
    const QJsonObject value{{"version",1},{"mode",mode_->currentIndex()},{"alwaysOnTop",topmost_->isChecked()},{"x",x()},{"y",y()}};
    QSaveFile file(statePath_);const auto bytes=QJsonDocument(value).toJson();
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())status_->setText(uiText("迷你窗偏好未保存，请检查配置目录。"));
}
void MiniCaptureWindow::closeEvent(QCloseEvent* event){savePreferences();hide();event->ignore();if(!QSystemTrayIcon::isSystemTrayAvailable() && openMain)openMain();}
void MiniCaptureWindow::moveEvent(QMoveEvent* event){QWidget::moveEvent(event);if(saveTimer_)saveTimer_->start();}
}
