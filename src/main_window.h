// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#pragma once

#include <QMainWindow>

namespace aitsnap {

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace aitsnap
