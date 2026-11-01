// SPDX-License-Identifier: Apache-2.0
#include "appearance.h"
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>
#define NOMINMAX
#include <windows.h>

static void initializeAitsnapResources() { Q_INIT_RESOURCE(aitsnap); }

namespace aitsnap {
namespace {
class CatalogTranslator final : public QTranslator {
public:
    QJsonObject catalog;
    QString translate(const char*, const char* source, const char*, int) const override
    { return catalog.value(QString::fromUtf8(source)).toString(); }
    bool isEmpty() const override { return catalog.isEmpty(); }
};

class ColorController final : public QObject, public QAbstractNativeEventFilter {
public:
    QString preference{"system"}, effective;
    explicit ColorController(QObject* parent) : QObject(parent) { qApp->installNativeEventFilter(this); }
    ~ColorController() override { qApp->removeNativeEventFilter(this); }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
#else
    bool nativeEventFilter(const QByteArray&, void* message, long*) override
#endif
    {
        auto* msg = static_cast<MSG*>(message);
        if (preference == "system" && (msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED))
            QTimer::singleShot(0, this, [this] { apply(); });
        return false;
    }
    void apply()
    {
        const bool dark = preference == "dark" || (preference == "system" &&
            QSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat)
                .value("AppsUseLightTheme", 1).toInt() == 0);
        const QString next = dark ? "dark" : "light";
        if (effective == next) return;
        effective = next;
        QPalette p;
        const QColor window(dark ? "#151d29" : "#edf2f7"), base(dark ? "#1e2938" : "#ffffff"),
            text(dark ? "#e6edf6" : "#17283d"), muted(dark ? "#a5b4c7" : "#52677e"),
            border(dark ? "#3b4b61" : "#ced9e5"), accent(dark ? "#82b4ff" : "#2364d2");
        p.setColor(QPalette::Window, window); p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base); p.setColor(QPalette::AlternateBase, window);
        p.setColor(QPalette::Text, text); p.setColor(QPalette::Button, base); p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::ToolTipBase, base); p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::Highlight, dark ? QColor("#304e78") : QColor("#dbe9ff"));
        p.setColor(QPalette::HighlightedText, text); p.setColor(QPalette::Link, accent);
        p.setColor(QPalette::PlaceholderText, muted); p.setColor(QPalette::Mid, border);
        for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) p.setColor(QPalette::Disabled, role, muted);
        qApp->setPalette(p);
        qApp->setStyleSheet(QStringLiteral(
            "QPushButton{padding:7px 12px;border:1px solid %1;border-radius:6px;background:%2;}"
            "QPushButton:hover{border-color:%3;} QPushButton:focus{border:2px solid %3;}"
            "QPushButton:disabled{color:%4;} QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{padding:5px;border:1px solid %1;border-radius:4px;}"
            "QListWidget,QTreeWidget,QTableWidget,QScrollArea{border:1px solid %1;border-radius:6px;}"
            "QTabWidget::pane{border:1px solid %1;} QTabBar::tab{padding:9px 16px;}"
            "QTabBar::tab:selected{background:%2;color:%3;} QMenu{padding:4px;border:1px solid %1;}"
            "QMenu::item{padding:7px 24px;} QMenu::item:selected{background:%5;}"
            "QLabel[muted=\"true\"]{color:%4;} QWidget[card=\"true\"]{background:%2;border:1px solid %1;border-radius:8px;}"
            "QPushButton[primary=\"true\"]{background:#2364d2;color:white;border:1px solid #2364d2;}"
            "QLabel#captureStatus[statusError=\"true\"]{color:%6;} QLabel#captureStatus[statusError=\"false\"]{color:%7;}"
        ).arg(border.name(), base.name(), accent.name(), muted.name(), p.color(QPalette::Highlight).name(), dark ? "#ff9a91" : "#b42318", dark ? "#96dfb5" : "#275b43"));
    }
};
ColorController* colors{};
CatalogTranslator* translator{};
}

QString effectiveLanguage(const QString& preference)
{
    if (preference != "system") return preference;
    return QLocale::system().language() == QLocale::Chinese ? "zh-CN" : "en-US";
}
void installLanguage(const QString& preference)
{
    initializeAitsnapResources();
    if (translator) { qApp->removeTranslator(translator); delete translator; translator = nullptr; }
    if (effectiveLanguage(preference) == "en-US") {
        translator = new CatalogTranslator; translator->setParent(qApp);
        QFile f(":/i18n/en-US.json");
        if (f.open(QIODevice::ReadOnly)) translator->catalog = QJsonDocument::fromJson(f.readAll()).object();
        qApp->installTranslator(translator);
    }
}
void applyColorMode(const QString& mode)
{
    if (!colors) { qApp->setStyle(QStyleFactory::create("Fusion")); colors = new ColorController(qApp); }
    colors->preference = mode; colors->apply();
}
QString activeColorMode() { return colors ? colors->effective : "light"; }
QString uiText(const char* source) { return QCoreApplication::translate("AITSnap", source); }
QString regionLabel(const core::Region& region)
{
    const auto name = QString::fromUtf8(region.name.c_str());
    return region.displayNumber > 0 ? QString("#%1 %2").arg(region.displayNumber).arg(name) : name;
}
}
