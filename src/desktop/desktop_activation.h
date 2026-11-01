// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QAbstractNativeEventFilter>
class QWidget;
namespace aitsnap {
class DesktopActivation final : public QAbstractNativeEventFilter {
public:
    explicit DesktopActivation(QWidget&);
    ~DesktopActivation() override;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    bool nativeEventFilter(const QByteArray&,void*,qintptr*) override;
#else
    bool nativeEventFilter(const QByteArray&,void*,long*) override;
#endif
private:
    QWidget& window_;
    quintptr handle_{};
};
bool activateDesktopProcess(unsigned long pid);
}
