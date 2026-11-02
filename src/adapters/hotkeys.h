// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QAbstractNativeEventFilter>
#include <QStringList>
#include <functional>
#include <vector>
namespace aitsnap {
struct Settings;
struct HotkeyBinding { QString shortcut; int action{}; };
std::vector<HotkeyBinding> hotkeyBindings(const Settings&);
class Hotkeys final : public QAbstractNativeEventFilter {
public:
    std::function<void(int)> triggered;
    std::function<void()> taskbarCreated;
    std::function<void()> displayChanged;
    ~Hotkeys() override;
    bool configure(const QStringList& shortcuts, QString& error);
    bool configureBindings(const std::vector<HotkeyBinding>&, QString& error);
    bool pause(bool paused, QString& error);
    bool registered() const { return registered_; }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
#else
    bool nativeEventFilter(const QByteArray&, void*, long*) override;
#endif
private:
    struct Key { unsigned int modifiers{}, vk{}; int action{}; QString shortcut; };
    std::vector<Key> keys_;
    bool registered_{};
    bool paused_{};
    bool registerKeys(const std::vector<Key>&, QString& error);
    void unregisterKeys();
};
} // namespace aitsnap
