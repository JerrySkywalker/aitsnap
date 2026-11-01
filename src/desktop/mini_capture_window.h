// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <functional>
class QComboBox;class QPushButton;class QLabel;class QCheckBox;class QTimer;
namespace aitsnap {
class MiniCaptureWindow final : public QWidget {
public:
    explicit MiniCaptureWindow(QString statePath,QWidget* parent=nullptr);
    void showMini();
    void setState(const QString& profile,const QString& status,bool canCapture,bool paused);
    std::function<void(int)> captureRequested;
    std::function<void()> openMain;
    std::function<void(bool)> pauseRequested;
protected:
    void closeEvent(QCloseEvent*) override;
    void moveEvent(QMoveEvent*) override;
private:
    QString statePath_;
    QComboBox* mode_{};QPushButton* capture_{};QLabel *profile_{},*status_{};
    QCheckBox* topmost_{};QPushButton* pause_{};QTimer* saveTimer_{};
    void savePreferences();void keepVisible();
};
}
