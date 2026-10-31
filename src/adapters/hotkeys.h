// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QAbstractNativeEventFilter>
#include <QStringList>
#include <functional>
#include <vector>
namespace aitsnap {
class Hotkeys final : public QAbstractNativeEventFilter {
public:
    std::function<void(int)> triggered;
    std::function<void()> taskbarCreated;
    ~Hotkeys() override;
    bool configure(const QStringList& shortcuts, QString& error);
    bool pause(bool paused, QString& error);
    bool registered() const { return registered_; }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
#else
    bool nativeEventFilter(const QByteArray&, void*, long*) override;
#endif
private:
    struct Key { unsigned int modifiers{}, vk{}; };
    std::vector<Key> keys_;
    bool registered_{};
    bool registerKeys(const std::vector<Key>&, QString& error);
    void unregisterKeys();
};
} // namespace aitsnap
