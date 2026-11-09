// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QImage>
#include <QElapsedTimer>
#include <QPointer>
#include <functional>
class QPushButton; class QLabel; class QWindow;
namespace aitsnap {
class ImagePreview;
class LiveOutputWindow final:public QWidget {
public:
    LiveOutputWindow(QString key,QString label,QWidget* parent=nullptr);
    QString key()const{return key_;}bool paused()const;
    void displayFrame(QImage,QString caption,qint64 frameTime,int targetFps,const QString& compatibilityReason);
    int screenRate() const;
    quint64 paintedFrames() const { return totalPainted_; }
    void setCaptureEnabled(bool);
    std::function<void()> subscriptionChanged,captureRequested;
protected:
    void closeEvent(QCloseEvent*)override;
    void showEvent(QShowEvent*)override;
    void moveEvent(QMoveEvent*)override;
    void changeEvent(QEvent*)override;
private:
    void observeScreen();
    QString key_,caption_;QImage image_;ImagePreview* preview_{};
    QPushButton *pause_{},*capture_{};
    QLabel* rate_{};
    QMetaObject::Connection refreshConnection_;
    QPointer<QWindow> observedWindow_;
    QElapsedTimer paintClock_, cadence_;
    qint64 frameTime_{}, lastPainted_{}, nextFrame_{};
    int painted_{}, targetFps_{};
    quint64 totalPainted_{};
    bool compatibility_{};
};
}
