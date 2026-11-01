// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/domain.h"
#include <vector>
class QWidget;
namespace aitsnap {
bool excludeOwnCaptureWindow(QWidget&);
std::vector<core::Rect> unexcludedOwnWindowRects();
}
