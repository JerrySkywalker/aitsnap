// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#include "local_host.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
namespace aitsnap {
namespace {
struct Handle { HANDLE value{}; ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); } operator HANDLE() const { return value; } };
QString logonSid(HANDLE token, QString& error) {
    DWORD count{}; GetTokenInformation(token, TokenGroups, nullptr, 0, &count); std::vector<BYTE> bytes(count);
    if (!count || !GetTokenInformation(token, TokenGroups, bytes.data(), count, &count)) { error = "Cannot query logon identity"; return {}; }
    const auto groups = reinterpret_cast<TOKEN_GROUPS*>(bytes.data());
    for (DWORD i = 0; i < groups->GroupCount; ++i) if ((groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID) {
        LPWSTR text{}; if (!ConvertSidToStringSidW(groups->Groups[i].Sid, &text)) break; const auto sid = QString::fromWCharArray(text); LocalFree(text); return sid;
    } error = "Interactive logon SID unavailable"; return {};
}
bool io(HANDLE pipe, bool writing, char* bytes, DWORD size, HANDLE stop, ULONGLONG deadline) {
    DWORD offset{};
    while (offset < size) {
        Handle event{CreateEventW(nullptr, TRUE, FALSE, nullptr)}; if (!event.value) return false; OVERLAPPED operation{}; operation.hEvent = event;
        DWORD done{}; BOOL ok = writing ? WriteFile(pipe, bytes + offset, size - offset, &done, &operation) : ReadFile(pipe, bytes + offset, size - offset, &done, &operation);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            HANDLE events[2]{stop ? stop : event.value, event.value}; const auto now = GetTickCount64(); const DWORD ms = deadline > now ? DWORD(std::min<ULONGLONG>(deadline - now, MAXDWORD - 1)) : 0;
            const auto wait = WaitForMultipleObjects(stop ? 2 : 1, events, FALSE, ms); const auto completed = stop ? WAIT_OBJECT_0 + 1 : WAIT_OBJECT_0;
            if (wait != completed) { CancelIoEx(pipe, &operation); GetOverlappedResult(pipe, &operation, &done, TRUE); return false; }
            ok = GetOverlappedResult(pipe, &operation, &done, FALSE);
        }
        if (!ok || !done) return false; offset += done;
    } return true;
}
bool readFrame(HANDLE pipe, QByteArray& payload, HANDLE stop, ULONGLONG deadline) {
    unsigned char header[4]{}; if (!io(pipe, false, reinterpret_cast<char*>(header), 4, stop, deadline)) return false;
    const auto size = quint32(header[0]) | (quint32(header[1]) << 8) | (quint32(header[2]) << 16) | (quint32(header[3]) << 24);
    if (!size || size > 1024 * 1024) return false; payload.resize(int(size)); return io(pipe, false, payload.data(), size, stop, deadline);
}
bool writeFrame(HANDLE pipe, const QJsonObject& object, HANDLE stop, ULONGLONG deadline) {
    auto payload = QJsonDocument(object).toJson(QJsonDocument::Compact); if (payload.isEmpty() || payload.size() > 1024 * 1024) return false;
    const auto size = quint32(payload.size()); char header[4]{char(size), char(size >> 8), char(size >> 16), char(size >> 24)};
    return io(pipe, true, header, 4, stop, deadline) && io(pipe, true, payload.data(), size, stop, deadline);
}
}
QString currentLogonSid(QString& error) { Handle token; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) { error = "Cannot open current process identity"; return {}; } return logonSid(token, error); }
QString defaultPipeName(QString& error) {
    const auto sid = currentLogonSid(error); if (sid.isEmpty()) return {}; DWORD session{}; if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) { error = "Cannot query session identity"; return {}; }
    return "AITSnap." + QString::fromLatin1(QCryptographicHash::hash(sid.toUtf8(), QCryptographicHash::Sha256).toHex().left(24)) + "." + QString::number(session);
}
bool validPipeName(const QString& name) { return name.size() <= 120 && QRegularExpression("^AITSnap\\.[A-Za-z0-9_.-]+$").match(name).hasMatch(); }
struct LocalPipeServer::Impl {
    QString name, diagnostic; HostHandler handler; RequestJournal journal;
    Handle pipe, stop; PSECURITY_DESCRIPTOR descriptor{}; std::thread thread;
    mutable std::mutex mutex;
    Impl(QString n, QString directory, HostHandler h) : name(std::move(n)), handler(std::move(h)), journal(std::move(directory)) {}
    ~Impl() { if (stop.value) SetEvent(stop); if (thread.joinable()) thread.join(); if (descriptor) LocalFree(descriptor); }
    void run() {
        while (WaitForSingleObject(stop, 0) != WAIT_OBJECT_0) {
            Handle event{CreateEventW(nullptr, TRUE, FALSE, nullptr)}; OVERLAPPED operation{}; operation.hEvent = event;
            BOOL connected = ConnectNamedPipe(pipe, &operation); const auto code = connected ? ERROR_SUCCESS : GetLastError();
            if (!connected && code == ERROR_IO_PENDING) { HANDLE events[]{stop.value, event.value}; const auto wait = WaitForMultipleObjects(2, events, FALSE, INFINITE); DWORD done{};
                if (wait != WAIT_OBJECT_0 + 1) { CancelIoEx(pipe, &operation); GetOverlappedResult(pipe, &operation, &done, TRUE); break; } connected = GetOverlappedResult(pipe, &operation, &done, FALSE);
            } else if (code == ERROR_PIPE_CONNECTED) connected = TRUE;
            if (!connected) { DisconnectNamedPipe(pipe); continue; }
            const auto deadline = GetTickCount64() + 20000; QByteArray payload;
            if (readFrame(pipe, payload, stop, deadline)) {
                QJsonParseError parse; const auto request = QJsonDocument::fromJson(payload, &parse);
                auto response = parse.error || !request.isObject() ? hostError("JSON", "A UTF-8 JSON object is required") : journal.dispatch(request.object(), handler);
                if (!response.contains("schemaVersion")) { response["schemaVersion"] = 1; response["correlationId"] = ""; }
                if (writeFrame(pipe, response, stop, deadline)) { char tail{}; io(pipe, false, &tail, 1, stop, deadline); } // wait for peer close after full response
            }
            DisconnectNamedPipe(pipe);
        }
    }
};
LocalPipeServer::LocalPipeServer(QString name, QString directory, HostHandler handler) : impl_(std::make_unique<Impl>(std::move(name), std::move(directory), std::move(handler))) {}
LocalPipeServer::~LocalPipeServer() = default;
bool LocalPipeServer::start(QString& error) {
    auto& d = *impl_; if (d.thread.joinable()) return true;
    if (!validPipeName(d.name)) { error = "Only the local AITSnap pipe namespace is admitted"; return false; }
    const auto sid = currentLogonSid(error); if (sid.isEmpty()) return false;
    const auto sddl = "D:P(A;;GA;;;" + sid + ")";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<const wchar_t*>(sddl.utf16()), SDDL_REVISION_1, &d.descriptor, nullptr)) { error = "Cannot create logon-only pipe descriptor"; return false; }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), d.descriptor, FALSE}; const auto path = "\\\\.\\pipe\\" + d.name;
    d.pipe.value = CreateNamedPipeW(reinterpret_cast<const wchar_t*>(path.utf16()), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 65536, 3000, &security);
    if (d.pipe.value == INVALID_HANDLE_VALUE) { error = "Cannot own local pipe (already owned or unavailable), Win32=" + QString::number(GetLastError()); return false; }
    d.stop.value = CreateEventW(nullptr, TRUE, FALSE, nullptr); if (!d.stop.value) { error = "Cannot create pipe stop event"; return false; }
    d.thread = std::thread([&d] { d.run(); }); return true;
}
QString LocalPipeServer::name() const { return impl_->name; }
QString LocalPipeServer::diagnostic() const { return impl_->journal.diagnostic(); }
QJsonObject callLocalHost(const QString& name, const QJsonObject& request, QString& error, int timeoutMs) {
    if (!validPipeName(name)) { error = "Only a local AITSnap pipe name is allowed"; return {}; }
    const auto path = "\\\\.\\pipe\\" + name; const auto deadline = GetTickCount64() + timeoutMs;
    Handle pipe;
    for (;;) {
        pipe.value = CreateFileW(reinterpret_cast<const wchar_t*>(path.utf16()), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe.value != INVALID_HANDLE_VALUE) break;
        const auto code = GetLastError(); if (code != ERROR_PIPE_BUSY || GetTickCount64() >= deadline || !WaitNamedPipeW(reinterpret_cast<const wchar_t*>(path.utf16()), 3000)) { error = "Local host unavailable/busy, Win32=" + QString::number(code); return {}; }
    }
    ULONG pid{}; Handle process, token;
    if (!GetNamedPipeServerProcessId(pipe, &pid) || !(process.value = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) || !OpenProcessToken(process, TOKEN_QUERY, &token.value)) { error = "Cannot verify pipe server logon identity"; return {}; }
    const auto serverSid = logonSid(token, error), clientSid = currentLogonSid(error);
    if (serverSid.isEmpty() || serverSid != clientSid) { error = "Pipe server is outside this interactive logon"; return {}; }
    QByteArray response; if (!writeFrame(pipe, request, nullptr, deadline) || !readFrame(pipe, response, nullptr, deadline)) { error = "Local request timed out, disconnected or exceeded frame bound"; return {}; }
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(response, &parse);
    if (parse.error || !document.isObject() || document.object()["schemaVersion"].toDouble() != 1 || document.object()["correlationId"] != request["correlationId"]) { error = "Invalid local host response version/correlation"; return {}; } return document.object();
}
} // namespace aitsnap
