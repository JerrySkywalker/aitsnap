// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QImage>
#include <functional>
class QPushButton;
namespace aitsnap {
class ImagePreview;
class LiveOutputWindow final:public QWidget {
public:
    LiveOutputWindow(QString key,QString label,QWidget* parent=nullptr);
    QString key()const{return key_;}bool paused()const;
    void displayFrame(QImage,QString caption);
    void setCaptureEnabled(bool);
    std::function<void()> subscriptionChanged,captureRequested;
protected:
    void closeEvent(QCloseEvent*)override;
    void showEvent(QShowEvent*)override;
private:
    QString key_,caption_;QImage image_;ImagePreview* preview_{};
    QPushButton *pause_{},*capture_{};
};
}
