// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include "core/domain.h"

namespace aitsnap {
// Apply before constructing widgets. Language changes take effect on restart.
void installLanguage(const QString& preference);
QString effectiveLanguage(const QString& preference);
void applyColorMode(const QString& mode);
QString activeColorMode();
QString uiText(const char* source);
QString regionLabel(const core::Region& region);
}
