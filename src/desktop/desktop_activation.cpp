// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "desktop_activation.h"
#include <QApplication>
#include <QWidget>
namespace aitsnap {
static UINT activationMessage(){static const UINT value=RegisterWindowMessageW(L"AITSnap.QtActivate.v1");return value;}
static constexpr LRESULT activationAcknowledged=0xA175;
DesktopActivation::DesktopActivation(QWidget& window):window_(window),handle_(quintptr(window.winId())){qApp->installNativeEventFilter(this);}
DesktopActivation::~DesktopActivation(){qApp->removeNativeEventFilter(this);}
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
bool DesktopActivation::nativeEventFilter(const QByteArray&,void* native,qintptr* result)
#else
bool DesktopActivation::nativeEventFilter(const QByteArray&,void* native,long* result)
#endif
{
    const auto* message=static_cast<MSG*>(native);
    if(!activationMessage() || message->message!=activationMessage() || reinterpret_cast<quintptr>(message->hwnd)!=handle_)return false;
    // Restore Qt's logical widget state, not just the underlying HWND visibility.
    QWidget* target=QApplication::activeModalWidget();if(!target)target=&window_;
    target->showNormal();target->raise();target->activateWindow();
    if(result)*result=target->isVisible()?activationAcknowledged:0;return true;
}
struct ExistingWindow{DWORD pid{};HWND handle{};};
static BOOL CALLBACK findWindow(HWND window,LPARAM parameter)
{
    auto& found=*reinterpret_cast<ExistingWindow*>(parameter);DWORD pid{};GetWindowThreadProcessId(window,&pid);
    if(pid!=found.pid || GetWindow(window,GW_OWNER) || (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return TRUE;
    wchar_t title[256]{};GetWindowTextW(window,title,256);
    if(QString::fromWCharArray(title).startsWith("AITSnap")){found.handle=window;return FALSE;}return TRUE;
}
bool activateDesktopProcess(unsigned long pid)
{
    ExistingWindow found{DWORD(pid)};
    // A second launch may acquire the lock receipt before the first has a HWND.
    for(int attempt=0;attempt<40 && !found.handle;++attempt){EnumWindows(findWindow,reinterpret_cast<LPARAM>(&found));if(!found.handle)Sleep(50);}
    if(!found.handle)return false;
    if(!activationMessage())return false;AllowSetForegroundWindow(found.pid);DWORD_PTR result{};
    return SendMessageTimeoutW(found.handle,activationMessage(),0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,2000,&result) && result==activationAcknowledged;
}
}
