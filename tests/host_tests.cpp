// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include "adapters/local_host.h"
#include "adapters/json_store.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <atomic>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static QJsonObject request(const QString& key = "key-one") { return {{"schemaVersion", 1}, {"correlationId", "correlation-one"}, {"action", "capture"}, {"idempotencyKey", key}, {"payload", QJsonObject{{"mode", "save"}}}}; }
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temp; check(temp.isValid(), "host test root"); QString error; std::atomic<int> calls{}; std::atomic<bool> busy{true};
        HostHandler handler = [&](const QString& action, const QJsonObject&, bool preflight) -> QJsonObject {
            if (action == "query-status") return {{"status", "OK"}, {"result", QJsonObject{{"captures", calls.load()}}}};
            if (preflight) return busy ? hostError("BUSY", "Synthetic capture busy", "BUSY") : QJsonObject{{"status", "READY"}};
            return {{"status", "ACCEPTED"}, {"result", QJsonObject{{"requestId", "synthetic-" + QString::number(++calls)}}}};
        };
        const auto journalRoot = temp.path() + "/journal";
        {
            RequestJournal journal(journalRoot); auto packet = request();
            check(journal.dispatch(packet, handler)["status"] == "BUSY" && calls == 0, "busy does not execute/consume key"); busy = false;
            check(journal.dispatch(packet, handler)["status"] == "ACCEPTED" && calls == 1, "same busy key can retry");
            packet["correlationId"] = "retry-correlation"; const auto duplicate = journal.dispatch(packet, handler);
            check(duplicate["replayed"].toBool() && duplicate["correlationId"] == "retry-correlation" && calls == 1, "durable key no duplicate/echo current correlation");
            auto conflicting = packet; conflicting["payload"] = QJsonObject{{"mode", "copy"}}; check(journal.dispatch(conflicting, handler)["error"].toObject()["code"] == "KEY_CONFLICT", "changed instruction under key rejected");
            auto forbidden = packet; forbidden["action"] = "execute-command"; check(journal.dispatch(forbidden, handler)["status"] == "ERROR" && calls == 1, "command intent rejected before handler");
            forbidden = packet; forbidden["shell"] = "raw-intent"; check(journal.dispatch(forbidden, handler)["status"] == "ERROR" && calls == 1, "unknown structure rejected");
        }
        {
            RequestJournal restarted(journalRoot); check(restarted.dispatch(request(), handler)["replayed"].toBool() && calls == 1, "key survives host restart");
        }
        const auto pending = request("pending-key"); const auto digest = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonObject{{"action", pending["action"]}, {"payload", pending["payload"]}}).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
        check(saveBytesNew(QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"idempotencyKey", "pending-key"}, {"fingerprint", digest}, {"state", "PENDING"}}).toJson(), journalRoot + "/pending.json", error), "interrupted intent fixture");
        { RequestJournal restarted(journalRoot); check(restarted.dispatch(pending, handler)["status"] == "UNCERTAIN" && calls == 1, "pending intent never blindly repeats"); }
        { RequestJournal bounded(temp.path() + "/bounded", 1); check(bounded.dispatch(request(), handler)["status"] == "ACCEPTED", "capacity first"); const auto count = calls.load(); check(bounded.dispatch(request("new-key"), handler)["status"] == "BUSY" && bounded.dispatch(request(), handler)["replayed"].toBool() && calls == count, "full capacity retains known duplicate"); }
        calls = 0; const auto name = "AITSnap.test-" + QUuid::createUuid().toString(QUuid::WithoutBraces); const auto nativePath = "\\\\.\\pipe\\" + name;
        bool restrictedDenied = false;
        {
            LocalPipeServer server(name, temp.path() + "/native", handler); check(server.start(error), "start native logon-SID pipe");
            const auto first = callLocalHost(name, request(), error); check(first["status"] == "ACCEPTED" && calls == 1, "native frame request/response");
            check(callLocalHost(name, request(), error)["replayed"].toBool() && calls == 1, "native duplicate idempotent");
            QProcess cli; cli.start(QCoreApplication::applicationDirPath() + "/aitsnap-cli.exe", {"query-status", "--pipe-name", name}); check(cli.waitForFinished(5000) && cli.exitCode() == 0 && QJsonDocument::fromJson(cli.readAllStandardOutput()).object()["result"].toObject()["captures"] == 1, "actual CLI subprocess roundtrip");
            check(WaitNamedPipeW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), 1000), "pipe available for ACL inspection");
            HANDLE pipe = CreateFileW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); check(pipe != INVALID_HANDLE_VALUE, "ACL inspection connection");
            PSECURITY_DESCRIPTOR descriptor{}; PACL acl{};
            const auto security = GetSecurityInfo(pipe, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &descriptor);
            bool exactAcl = false;
            if (security == ERROR_SUCCESS && acl && acl->AceCount == 1) { void* ace{}; PSID sid{}; const auto logon = currentLogonSid(error);
                if (GetAce(acl, 0, &ace) && ConvertStringSidToSidW(reinterpret_cast<const wchar_t*>(logon.utf16()), &sid)) { exactAcl = reinterpret_cast<ACE_HEADER*>(ace)->AceType == ACCESS_ALLOWED_ACE_TYPE && EqualSid(&reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart, sid); LocalFree(sid); }
            }
            if (descriptor) LocalFree(descriptor); CloseHandle(pipe); check(exactAcl, "actual pipe DACL contains only current logon SID");
            check(WaitNamedPipeW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), 1000), "pipe available for restricted token");
            HANDLE token{}, restricted{}; DWORD sidBytes = SECURITY_MAX_SID_SIZE; DWORD anonymous[(SECURITY_MAX_SID_SIZE + 3) / 4]{};
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &token) && CreateWellKnownSid(WinAnonymousSid, nullptr, anonymous, &sidBytes)) {
                SID_AND_ATTRIBUTES restricting{anonymous, 0};
                if (CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 0, nullptr, 0, nullptr, 1, &restricting, &restricted) && ImpersonateLoggedOnUser(restricted)) {
                    const auto denied = CreateFileW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); const auto code = GetLastError();
                    const bool reverted = RevertToSelf() != FALSE; if (denied != INVALID_HANDLE_VALUE) CloseHandle(denied); check(reverted, "restore own thread token"); restrictedDenied = denied == INVALID_HANDLE_VALUE && code == ERROR_ACCESS_DENIED;
                }
            }
            if (restricted) CloseHandle(restricted); if (token) CloseHandle(token);
            check(restrictedDenied, "actual restricted-token pipe open must be denied");
            check(!validPipeName("\\\\remote\\pipe\\AITSnap.host") && !validPipeName("other-service"), "client never targets remote/unrelated namespace");
        }
        std::cout << "HOST_PASS journal busy-retry duplicate conflict restart-pending capacity native-frame CLI logon-only-DACL restricted-token-denied; cross-user/remote-actual=NOT_RUN\n";
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
