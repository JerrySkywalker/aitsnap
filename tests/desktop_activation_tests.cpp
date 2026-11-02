// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "desktop/desktop_activation.h"
#include <QApplication>
#include <QPushButton>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QScreen>
#include "desktop/appearance.h"
#include "desktop/capture_visibility.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){QApplication app(argc,argv);try{
    if(app.platformName()!="windows")throw std::runtime_error("Native Windows platform required");
    if(app.arguments().contains("--display-probe")){
        aitsnap::applyColorMode("system");
        QWidget normal,excluded;const auto area=app.primaryScreen()->availableGeometry();
        const auto setup=[&](QWidget& w,const QString& title,int offset){
            w.setWindowTitle(title);w.setWindowFlag(Qt::WindowStaysOnTopHint);w.resize(390,230);w.move(area.x()+30+offset,area.y()+40);
            auto* layout=new QVBoxLayout(&w);auto* label=new QLabel(title+"\nBLUE + GREEN / visible text",&w);label->setStyleSheet("background:#2474d2;color:white;font-size:18px;padding:15px;");layout->addWidget(label);
            auto* button=new QPushButton("GREEN BUTTON",&w);button->setStyleSheet("background:#16833e;color:white;font-size:18px;padding:20px;");layout->addWidget(button);w.show();
        };
        setup(normal,"AITSnap A - NORMAL",0);setup(excluded,"AITSnap B - CAPTURE EXCLUDED",420);
        const bool applied=SetWindowDisplayAffinity(reinterpret_cast<HWND>(excluded.winId()),0x11)!=FALSE;
        DWORD first{},second{};GetWindowDisplayAffinity(reinterpret_cast<HWND>(normal.winId()),&first);GetWindowDisplayAffinity(reinterpret_cast<HWND>(excluded.winId()),&second);
        std::cout<<"DISPLAY_PROBE_READY normalAffinity="<<first<<" excludedAffinity="<<second<<" applied="<<applied<<std::endl;
        QTimer::singleShot(300000,&app,&QApplication::quit);return app.exec();
    }
    QWidget window;window.setWindowTitle("AITSnap activation regression");window.resize(480,240);QPushButton child("Visible controls",&window);child.move(24,24);
    if(aitsnap::excludeOwnCaptureWindow(window))throw std::runtime_error("Default compatibility display must not enable content exclusion");
    DWORD affinity{};if(!GetWindowDisplayAffinity(reinterpret_cast<HWND>(window.winId()),&affinity) || affinity!=0)throw std::runtime_error("Default window must remain normally displayable");
    window.winId();aitsnap::DesktopActivation activation(window);
    for(int pass=0;pass<3;++pass){window.hide();app.processEvents();if(window.isVisible())throw std::runtime_error("Fixture must be Qt hidden");
        if(!aitsnap::activateDesktopProcess(GetCurrentProcessId()))throw std::runtime_error("Activation was not acknowledged");app.processEvents();
        if(!window.isVisible() || !child.isVisible())throw std::runtime_error("Native HWND visible but Qt widget/children remain hidden: white-window regression");
    }
    window.showMinimized();app.processEvents();if(!aitsnap::activateDesktopProcess(GetCurrentProcessId()) || window.isMinimized() || !child.isVisible())throw std::runtime_error("Minimized Qt window not restored");
    std::cout<<"DESKTOP_ACTIVATION_PASS Qt logical visibility and child visibility restored across repeated tray-style hides\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
