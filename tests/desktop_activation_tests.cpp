// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "desktop/desktop_activation.h"
#include <QApplication>
#include <QPushButton>
#include <QWidget>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){QApplication app(argc,argv);try{
    if(app.platformName()!="windows")throw std::runtime_error("Native Windows platform required");
    QWidget window;window.setWindowTitle("AITSnap activation regression");window.resize(480,240);QPushButton child("Visible controls",&window);child.move(24,24);
    window.winId();aitsnap::DesktopActivation activation(window);
    for(int pass=0;pass<3;++pass){window.hide();app.processEvents();if(window.isVisible())throw std::runtime_error("Fixture must be Qt hidden");
        if(!aitsnap::activateDesktopProcess(GetCurrentProcessId()))throw std::runtime_error("Activation was not acknowledged");app.processEvents();
        if(!window.isVisible() || !child.isVisible())throw std::runtime_error("Native HWND visible but Qt widget/children remain hidden: white-window regression");
    }
    window.showMinimized();app.processEvents();if(!aitsnap::activateDesktopProcess(GetCurrentProcessId()) || window.isMinimized() || !child.isVisible())throw std::runtime_error("Minimized Qt window not restored");
    std::cout<<"DESKTOP_ACTIVATION_PASS Qt logical visibility and child visibility restored across repeated tray-style hides\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
