// SPDX-License-Identifier: Apache-2.0
#include "adapters/word_dispatch.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <iostream>
using namespace aitsnap::word;
static void stage(const char* text) { std::cout << "WORD_NATIVE_STAGE=" << text << std::endl; }
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void run(const QString& root)
{
    const auto directory = QDir(root).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    check(QDir().mkpath(directory), "create probe output");
    const auto png = directory + "/synthetic.png", docx = directory + "/synthetic-native.docx";
    QImage image(320, 160, QImage::Format_RGB32); image.fill(Qt::blue); check(image.save(png, "PNG"), "synthetic PNG");
    Object app, document; bool ownsApp = false;
    try {
        stage("activate"); app = createApplication(); stage("activated");
        auto documents = app.get(L"Documents").object();
        check(documents.get(L"Count").integer() == 0, "new Word instance has unexpected documents; preserve it"); ownsApp = true;
        app.put(L"Visible", Value(false)); app.put(L"DisplayAlerts", Value(0));
        const auto version = app.get(L"Version").text(); document = documents.call(L"Add").object(); stage("added");
        document.get(L"Content").object().call(L"InsertAfter", {Value(QStringLiteral("Synthetic AITSnap native COM report\r"))});
        for (int i = 1; i <= 2; ++i) {
            int end = document.get(L"Content").object().get(L"End").integer() - 1;
            auto range = document.call(L"Range", {Value(end), Value(end)}).object();
            document.get(L"InlineShapes").object().call(L"AddPicture", {filePath(png), Value(false), Value(true), Value(range.raw())});
            end = document.get(L"Content").object().get(L"End").integer() - 1;
            document.call(L"Range", {Value(end), Value(end)}).object().call(L"InsertAfter", {Value(QString("\rSynthetic caption %1\r").arg(i))});
        }
        stage("pictures-added");
        auto variables = document.get(L"Variables").object();
        variables.call(L"Add", {Value(QStringLiteral("AITSnap_native_probe")), Value(QStringLiteral("COMMITTED"))}); stage("marker-added");
        check(!QFile::exists(docx), "no-overwrite guard");
        stage("saving"); document.call(L"SaveAs2", {filePath(docx), Value(12)}); stage("saved");
        document.call(L"Close", {Value(0)}); document = {};
        document = documents.call(L"Open", {filePath(docx), Value(false), Value(false), Value(false)}).object(); stage("reopened");
        check(document.get(L"InlineShapes").object().get(L"Count").integer() == 2, "reopened image count");
        const auto marker = document.get(L"Variables").object().invoke(L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, {Value(QStringLiteral("AITSnap_native_probe"))}).object().get(L"Value").text();
        check(marker == "COMMITTED", "marker roundtrip");
        QJsonObject receipt{{"status", "PASS"}, {"wordVersion", version}, {"inlineShapes", 2}, {"markerReopened", true}, {"document", docx}};
        QFile file(directory + "/receipt.json"); check(file.open(QIODevice::WriteOnly | QIODevice::NewOnly), "receipt output"); file.write(QJsonDocument(receipt).toJson()); file.close();
        document.call(L"Close", {Value(0)}); document = {};
        if (documents.get(L"Count").integer() == 0) app.call(L"Quit", {Value(0)}); app = {};
        std::cout << "WORD_NATIVE_PASS " << QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    } catch (...) {
        try { if (document) document.call(L"Close", {Value(0)}); } catch (...) {}
        try { if (ownsApp && app && app.get(L"Documents").object().get(L"Count").integer() == 0) app.call(L"Quit", {Value(0)}); } catch (...) {}
        throw;
    }
}
int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 2;
    int result = 0;
    try { run(application.arguments()[1]); }
    catch (const std::exception& e) { std::cerr << "WORD_NATIVE_FAIL " << e.what() << std::endl; result = 1; }
    CoUninitialize(); return result;
}
