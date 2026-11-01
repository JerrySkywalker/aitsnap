// SPDX-License-Identifier: Apache-2.0
#include "word_dispatch.h"
#include "report_queue.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <psapi.h>
#include <iostream>
using namespace aitsnap;
using namespace aitsnap::word;
namespace {
struct Session {
    Object app, document;
    bool ownsApp{}, ownsDocument{};
    void closeDocument() { if (ownsDocument && document) { document.call(L"Close", {Value(0)}); document = {}; ownsDocument = false; } }
    ~Session() {
        try { closeDocument(); } catch (...) {}
        try { if (ownsApp && app && app.get(L"Documents").object().get(L"Count").integer() == 0) app.call(L"Quit", {Value(0)}); } catch (...) {}
    }
};
Object runningDocument(const QString& path)
{
    IRunningObjectTable* table{}; require(GetRunningObjectTable(0, &table), "Running object table");
    std::shared_ptr<IRunningObjectTable> owner(table, [](auto* p) { p->Release(); });
    const auto native = QDir::toNativeSeparators(path);
    IMoniker* moniker{}; require(CreateFileMoniker(reinterpret_cast<const wchar_t*>(native.utf16()), &moniker), "Exact document moniker");
    std::shared_ptr<IMoniker> key(moniker, [](auto* p) { p->Release(); });
    IUnknown* unknown{};
    if (table->GetObject(moniker, &unknown) != S_OK) return {};
    std::shared_ptr<IUnknown> object(unknown, [](auto* p) { p->Release(); });
    IDispatch* dispatch{}; require(unknown->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&dispatch)), "Document dispatch"); return Object(dispatch);
}
void createOwnedApp(Session& session)
{
    session.app = createApplication();
    if (session.app.get(L"Documents").object().get(L"Count").integer() != 0) throw std::runtime_error("Unexpected documents in new Word application; preserved");
    session.ownsApp = true; session.app.put(L"Visible", Value(false)); session.app.put(L"DisplayAlerts", Value(0));
    session.app.put(L"AutomationSecurity", Value(3)); // disable document macros in this owned instance
}
Object endRange(const Object& document)
{
    const int end = document.get(L"Content").object().get(L"End").integer() - 1;
    return document.call(L"Range", {Value(end), Value(end)}).object();
}
QString markerValue(const Object& document, const QString& name)
{
    auto variables = document.get(L"Variables").object();
    const int count = variables.get(L"Count").integer();
    for (int i = 1; i <= count; ++i) {
        auto variable = variables.invoke(L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, {Value(i)}).object();
        if (variable.get(L"Name").text() == name) return variable.get(L"Value").text();
    }
    return {};
}
void setMarker(const Object& document, const QString& name, const QString& value)
{
    auto variables = document.get(L"Variables").object();
    if (markerValue(document, name).isEmpty()) variables.call(L"Add", {Value(name), Value(value)});
    else variables.invoke(L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, {Value(name)}).object().put(L"Value", Value(value));
}
void insertPicture(const Object& document, const Object& range, const QString& path, double width, double height)
{
    auto pictures = document.get(L"InlineShapes").object(); const auto before = pictures.get(L"Count").integer();
    const auto inserted = pictures.call(L"AddPicture", {filePath(path), Value(false), Value(true), Value(range.raw())});
    if (inserted.value.vt != VT_DISPATCH || !inserted.value.pdispVal) throw std::runtime_error("AddPicture returned null; inline before=" + std::to_string(before) + " after=" + std::to_string(pictures.get(L"Count").integer()) + " pathChars=" + std::to_string(path.size()));
    auto picture = inserted.object();
    picture.put(L"LockAspectRatio", Value(-1));
    if (picture.get(L"Width").number() > width) picture.put(L"Width", Value(width));
    if (picture.get(L"Height").number() > height) picture.put(L"Height", Value(height));
}
QJsonObject execute(const QJsonObject& job, bool recovery)
{
    const auto ext = job["extensions"].toObject(), target = job["target"].toObject();
    const auto path = target["documentPath"].toString();
    const auto id = job["jobId"].toString(), fingerprint = ext["fingerprint"].toString();
    const auto marker = "AITSnap_" + QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
    QJsonObject result{{"schemaVersion", 1}, {"jobId", id}, {"dispatchId", ext["dispatchId"]}, {"state", "FAILED"}, {"errorCode", "PRECHECK"}};
    bool mutation = false; Session session; QString stage = "precheck";
    try {
        const bool exists = QFile::exists(path), create = target["mode"] == "create" && !exists;
        if (recovery && !exists) { result["state"] = "UNCERTAIN"; result["errorCode"] = "RECOVERY_TARGET_MISSING"; return result; }
        if (!exists && !create) throw std::runtime_error("Append target is missing");
        if (exists) {
            stage = "open-exact-target";
            session.document = runningDocument(path);
            if (session.document) session.app = session.document.get(L"Application").object();
            else { createOwnedApp(session); session.document = session.app.get(L"Documents").object().call(L"Open", {filePath(path), Value(false), Value(false), Value(false)}).object(); session.ownsDocument = true; }
            if (QFileInfo(session.document.get(L"FullName").text()).canonicalFilePath().compare(QFileInfo(path).canonicalFilePath(), Qt::CaseInsensitive)) throw std::runtime_error("Word bound to a different document");
            const auto value = markerValue(session.document, marker);
            if (value == "COMMITTED:" + fingerprint && session.document.get(L"Saved").integer() != 0) {
                result["state"] = "COMMITTED"; result["errorCode"] = ""; result["marker"] = marker; result["recovered"] = true; return result;
            }
            if (recovery || !value.isEmpty()) { result["state"] = "UNCERTAIN"; result["errorCode"] = "MARKER_ABSENT_PREPARED_OR_CONFLICTING"; return result; }
            if (target["mode"] == "create") throw std::runtime_error("Create target already exists without matching committed marker");
            if (session.document.get(L"ReadOnly").integer() || session.document.get(L"ProtectionType").integer() != -1) throw std::runtime_error("Target is read-only or protected");
            if (session.document.get(L"Saved").integer() == 0) throw std::runtime_error("Target has unsaved user changes; save it before retry");
        }
        // Verify every exact source before any document mutation. Sources stay read-only.
        stage = "verify-sources"; const auto sources = ext["sources"].toArray(), items = job["items"].toArray();
        for (const auto& value : sources) {
            const auto source = value.toObject(); QFile file(source["path"].toString()); QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file) || QString::fromLatin1(hash.result().toHex()) != source["sha256"].toString()) throw std::runtime_error("Saved source SHA mismatch or source unavailable");
            QImageReader reader(source["path"].toString()); const auto size = reader.size();
            if (size.width() <= 0 || size.height() <= 0 || qint64(size.width()) * size.height() > 100000000) throw std::runtime_error("Invalid source image dimensions");
        }
        const auto staging = QDir(QFileInfo(path).absolutePath()).filePath(".aitsnap-" + id + ".pending.docx");
        if (create) {
            stage = "create-staging-document";
            if (QFile::exists(staging)) { result["state"] = "UNCERTAIN"; result["errorCode"] = "CREATE_STAGING_EXISTS"; return result; }
            createOwnedApp(session); session.document = session.app.get(L"Documents").object().call(L"Add").object(); session.ownsDocument = true;
        }
        result["wordVersion"] = session.app.get(L"Version").text();
        // PREPARED is durably saved before inserting content. Creation uses a
        // job-specific staging document and publishes by no-overwrite rename.
        stage = "save-prepared"; mutation = true; setMarker(session.document, marker, "PREPARED:" + fingerprint);
        if (create) session.document.call(L"SaveAs2", {filePath(staging), Value(12)}); else session.document.call(L"Save");
        const bool configurableText=ext.contains("textPolicy");
        stage = "insert-heading"; if(!configurableText || !ext["heading"].toString().isEmpty())endRange(session.document).call(L"InsertAfter", {Value("\r" + ext["heading"].toString() + "\r")});
        stage = "insertion-section-page-setup";
        const auto page = endRange(session.document).get(L"Sections").object().invoke(L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, {Value(1)}).object().get(L"PageSetup").object();
        const double availableWidth = std::max(36.0, page.get(L"PageWidth").number() - page.get(L"LeftMargin").number() - page.get(L"RightMargin").number());
        const double availableHeight = std::max(36.0, page.get(L"PageHeight").number() - page.get(L"TopMargin").number() - page.get(L"BottomMargin").number() - 48);
        Object table;
        stage = "create-table";
        if (ext["layout"] == "table") table = session.document.get(L"Tables").object().call(L"Add", {Value(endRange(session.document).raw()), Value(int((items.size() + 1) / 2)), Value(2)}).object();
        for (int i = 0; i < items.size(); ++i) {
            stage = "insert-picture-" + QString::number(i);
            const auto caption = items[i].toObject()["caption"].toString() + "\r";
            const bool writeCaption=!configurableText || (ext["textPolicy"].toObject()["captions"].toBool() && !items[i].toObject()["caption"].toString().isEmpty());
            if (table) {
                auto cell = table.call(L"Cell", {Value(i / 2 + 1), Value(i % 2 + 1)}).object();
                const int start = cell.get(L"Range").object().get(L"Start").integer();
                insertPicture(session.document, session.document.call(L"Range", {Value(start), Value(start)}).object(), sources[i].toObject()["path"].toString(), availableWidth / 2 - 12, availableHeight);
                const int end = cell.get(L"Range").object().get(L"End").integer() - 2;
                if(writeCaption)session.document.call(L"Range", {Value(end), Value(end)}).object().call(L"InsertAfter", {Value("\r" + caption)});
            } else {
                insertPicture(session.document, endRange(session.document), sources[i].toObject()["path"].toString(), availableWidth, availableHeight);
                if(writeCaption)endRange(session.document).call(L"InsertAfter", {Value("\r" + caption)});
            }
        }
        stage = "save-committed"; setMarker(session.document, marker, "COMMITTED:" + fingerprint); session.document.call(L"Save");
        if (session.ownsApp) { // process peak, not an attribution to other user documents
            try { DWORD pid{}; GetWindowThreadProcessId(reinterpret_cast<HWND>(quintptr(quint32(session.document.get(L"ActiveWindow").object().get(L"Hwnd").integer()))), &pid);
                HANDLE process = pid ? OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid) : nullptr;
                if (process) { PROCESS_MEMORY_COUNTERS memory{}; memory.cb = sizeof(memory); if (GetProcessMemoryInfo(process, &memory, sizeof(memory))) result["wordProcessPeakWorkingSetBytes"] = qint64(memory.PeakWorkingSetSize); CloseHandle(process); }
            } catch (...) { result["wordMemoryStatus"] = "NOT_MEASURED"; } // optional measurement never changes insertion outcome
        }
        if (create) { session.closeDocument(); if (!QFile::rename(staging, path)) throw std::runtime_error("Cannot publish new document without overwriting target; staging retained"); }
        result["state"] = "COMMITTED"; result["errorCode"] = ""; result["marker"] = marker; result["itemsInserted"] = items.size(); return result;
    } catch (const std::exception& error) {
        result["state"] = mutation || recovery ? "UNCERTAIN" : "RETRYABLE";
        result["errorCode"] = mutation ? "WORD_MUTATION_UNCONFIRMED" : "WORD_PREMUTATION"; result["stage"] = stage; result["message"] = QString::fromUtf8(error.what()); return result;
    }
}
} // namespace
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); const auto args = app.arguments();
    const int jobIndex = args.indexOf("--job"), resultIndex = args.indexOf("--result");
    if (jobIndex < 0 || jobIndex + 1 >= args.size() || resultIndex < 0 || resultIndex + 1 >= args.size()) return 2;
    QJsonObject job; QString error;
    if (!readReportJob(args[jobIndex + 1], job, error)) { std::cerr << error.toStdString() << '\n'; return 2; }
    if (!args.contains("--recover") && job["state"] != "RUNNING") { std::cerr << "Only a durably RUNNING job may insert\n"; return 2; }
    const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); if (FAILED(hr)) return 3;
    QElapsedTimer clock; clock.start(); auto result = execute(job, args.contains("--recover")); result["wordExecutionMonotonicMs"] = double(clock.nsecsElapsed()) / 1000000.0;
    PROCESS_MEMORY_COUNTERS memory{}; memory.cb = sizeof(memory); if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) result["helperPeakWorkingSetBytes"] = qint64(memory.PeakWorkingSetSize); CoUninitialize();
    if (!saveBytesNew(QJsonDocument(result).toJson(), args[resultIndex + 1], error)) { std::cerr << error.toStdString() << '\n'; return 4; }
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << '\n'; return 0;
}
