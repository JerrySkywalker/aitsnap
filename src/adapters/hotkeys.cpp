// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include "hotkeys.h"
#include <QCoreApplication>
#include <algorithm>
namespace aitsnap {
static constexpr int firstId = 0x4150;
Hotkeys::~Hotkeys() { unregisterKeys(); QCoreApplication::instance()->removeNativeEventFilter(this); }
void Hotkeys::unregisterKeys()
{
    if (registered_) for (int i = 0; i < int(keys_.size()); ++i) UnregisterHotKey(nullptr, firstId + i);
    registered_ = false;
}
bool Hotkeys::registerKeys(const std::vector<Key>& keys, QString& error)
{
    for (int i = 0; i < int(keys.size()); ++i) {
        if (!RegisterHotKey(nullptr, firstId + i, keys[std::size_t(i)].modifiers | MOD_NOREPEAT, keys[std::size_t(i)].vk)) {
            const auto code = GetLastError();
            for (int j = 0; j < i; ++j) UnregisterHotKey(nullptr, firstId + j);
            error = QStringLiteral("快捷键 %1 注册失败（冲突或系统限制，Win32 %2）").arg(i + 1).arg(code);
            return false;
        }
    }
    registered_ = true; return true;
}
bool Hotkeys::configure(const QStringList& shortcuts, QString& error)
{
    std::vector<Key> candidate;
    for (const auto& text : shortcuts) {
        const auto parts = text.trimmed().toUpper().split('+', Qt::SkipEmptyParts);
        if (parts.isEmpty()) { error = "Empty hotkey"; return false; }
        Key key;
        for (int i = 0; i + 1 < parts.size(); ++i) {
            const auto p = parts[i].trimmed();
            if (p == "CTRL") key.modifiers |= MOD_CONTROL;
            else if (p == "ALT") key.modifiers |= MOD_ALT;
            else if (p == "SHIFT") key.modifiers |= MOD_SHIFT;
            else if (p == "WIN") key.modifiers |= MOD_WIN;
            else { error = "Unknown modifier: " + p; return false; }
        }
        const auto value = parts.last().trimmed();
        if (value.size() == 1 && ((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= '0' && value[0] <= '9'))) key.vk = value[0].unicode();
        else if (value.startsWith('F')) { bool ok{}; int number = value.mid(1).toInt(&ok); if (ok && number >= 1 && number <= 24) key.vk = VK_F1 + unsigned(number - 1); }
        else if (value == "PRINTSCREEN") key.vk = VK_SNAPSHOT;
        else if (value == "SPACE") key.vk = VK_SPACE;
        if (!key.vk) { error = "Use A–Z, 0–9, F1–F24, PrintScreen or Space"; return false; }
        candidate.push_back(key);
    }
    if(registered_ && candidate.size()==keys_.size() && std::equal(candidate.begin(),candidate.end(),keys_.begin(),[](const Key& a,const Key& b){return a.modifiers==b.modifiers && a.vk==b.vk;}))return true;
    const auto old = keys_; const bool wasRegistered = registered_;
    unregisterKeys();
    if (!registerKeys(candidate, error)) {
        if (wasRegistered) { QString restore; if (!registerKeys(old, restore)) error += "; previous shortcuts also unavailable: " + restore; }
        return false;
    }
    keys_ = candidate; return true;
}
bool Hotkeys::pause(bool paused, QString& error)
{
    if (paused) { unregisterKeys(); return true; }
    if (registered_) return true;
    return registerKeys(keys_, error);
}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool Hotkeys::nativeEventFilter(const QByteArray&, void* message, qintptr*)
#else
bool Hotkeys::nativeEventFilter(const QByteArray&, void* message, long*)
#endif
{
    const auto* m = static_cast<MSG*>(message);
    if (m->message == WM_HOTKEY && registered_ && m->wParam >= firstId && m->wParam < firstId + keys_.size()) {
        if (triggered) triggered(int(m->wParam) - firstId);
        return true;
    }
    static const UINT taskbarMessage = RegisterWindowMessageW(L"TaskbarCreated");
    if (m->message == taskbarMessage && taskbarCreated) taskbarCreated();
    return false;
}
} // namespace aitsnap
