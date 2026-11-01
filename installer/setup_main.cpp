// SPDX-License-Identifier: Apache-2.0
// A branded entry point only: the unchanged system MSI owns the full wizard and transaction.
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <bcrypt.h>
#include <array>
#include <string>
#include <vector>
#include "setup_config.h"

static std::wstring digest(const BYTE* data, DWORD size)
{
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; DWORD length{}, returned{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&length), sizeof(length), &returned, 0) < 0) { BCryptCloseAlgorithmProvider(algorithm, 0); return {}; }
    std::vector<BYTE> object(length); std::array<BYTE, 32> bytes{};
    bool ok = BCryptCreateHash(algorithm, &hash, object.data(), length, nullptr, 0, 0) >= 0;
    if (ok) ok = BCryptHashData(hash, const_cast<PUCHAR>(data), size, 0) >= 0 && BCryptFinishHash(hash, bytes.data(), DWORD(bytes.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok) return {};
    std::wstring result; constexpr wchar_t digits[] = L"0123456789abcdef";
    for (auto byte : bytes) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}
static int fail(const wchar_t* message, bool silent)
{
    if (!silent) MessageBoxW(nullptr, message, L"AITSnap " AITSNAP_SETUP_VERSION L" 安装", MB_OK | MB_ICONERROR);
    return 1;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    int count{}; auto** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool extractOnly = false; std::wstring directory;
    if (arguments && count == 3 && std::wstring(arguments[1]) == L"--extract-to") { extractOnly = true; directory = arguments[2]; }
    else if (count != 1) { if (arguments) LocalFree(arguments); return 2; }
    if (arguments) LocalFree(arguments);
    const auto resource = FindResourceW(instance, MAKEINTRESOURCEW(201), RT_RCDATA);
    if (!resource) return fail(L"安装内容缺失。请重新获取完整安装包。", extractOnly);
    const auto size = SizeofResource(instance, resource);
    const auto* data = static_cast<const BYTE*>(LockResource(LoadResource(instance, resource)));
    if (!data || digest(data, size) != AITSNAP_INSTALLER_SHA256) return fail(L"安装内容校验失败，未启动安装。请重新获取完整安装包。", extractOnly);
    if (!extractOnly) {
        wchar_t temporary[32768]{}; if (!GetTempPathW(32768, temporary)) return fail(L"无法获取临时目录。", false);
        GUID id{}; if (FAILED(CoCreateGuid(&id))) return fail(L"无法准备安装目录。", false);
        wchar_t token[40]{}; StringFromGUID2(id, token, 40); directory = std::wstring(temporary) + L"AITSnap-" + token;
    }
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return fail(L"无法创建安装包目录。", extractOnly);
    const auto path = directory + L"\\AITSnap-" AITSNAP_SETUP_VERSION L"-windows-x64.msi";
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return fail(L"无法写入安装内容。目标文件已存在时不会覆盖。", extractOnly);
    DWORD written{}; const bool saved = WriteFile(file, data, size, &written, nullptr) && written == size; CloseHandle(file);
    if (!saved) { DeleteFileW(path.c_str()); if (!extractOnly) RemoveDirectoryW(directory.c_str()); return fail(L"安装内容写入失败。请检查剩余空间。", extractOnly); }
    if (extractOnly) return 0; // Artifact verification only; never installs or launches an application.
    wchar_t system[32768]{}; if (!GetSystemDirectoryW(system, 32768)) return fail(L"无法找到 Windows Installer。", false);
    const auto installer = std::wstring(system) + L"\\msiexec.exe";
    const auto parameters = L"/i \"" + path + L"\"";
    SHELLEXECUTEINFOW action{sizeof(action)}; action.fMask = SEE_MASK_NOCLOSEPROCESS; action.lpVerb = L"open";
    action.lpFile = installer.c_str(); action.lpParameters = parameters.c_str(); action.nShow = SW_SHOWNORMAL;
    DWORD exitCode = 1;
    if (ShellExecuteExW(&action) && action.hProcess) { WaitForSingleObject(action.hProcess, INFINITE); GetExitCodeProcess(action.hProcess, &exitCode); CloseHandle(action.hProcess); }
    else fail(L"Windows Installer 未能启动。可以直接运行随附的 MSI 安装包。", false);
    DeleteFileW(path.c_str()); RemoveDirectoryW(directory.c_str());
    return int(exitCode);
}
