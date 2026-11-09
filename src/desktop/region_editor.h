// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/domain.h"
#include <QObject>
#include <functional>
class QWidget;
namespace aitsnap {
QString regionCoverageError(const core::Region& region);
// Native cursor coordinates are physical; Qt coordinates are used only to paint.
void openRegionEditor(QWidget* owner, const core::CaptureProfile&,
                      std::function<void(bool accepted, core::CaptureProfile)> finished, int selectedRegion = -1);
} // namespace aitsnap
