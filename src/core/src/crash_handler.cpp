#define NOMINMAX
#include "core/crash_handler.hpp"

#include <windows.h>
#include <DbgHelp.h>
#include <psapi.h>
#include <signal.h>

#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <mutex>
#include <string>
#include <filesystem>
#include <exception>

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "Psapi.lib")

namespace {
std::mutex g_lock;
std::wstring g_dir = L".";
LONG WINAPI writeDump(EXCEPTION_POINTERS* ep);

constexpr DWORD kSigAbortCode = 0xE0000001;
constexpr DWORD kSigSegvCode = 0xE0000002;
constexpr DWORD kInvalidParamCode = 0xE0000003;
constexpr DWORD kPureCallCode = 0xE0000004;
constexpr DWORD kTerminateCode = 0xE0000005;

void ensureDir() {
    std::scoped_lock lk(g_lock);
    try {
        std::filesystem::create_directories(g_dir);
    } catch (...) {
        // Directory creation failure is non-fatal for crash logging
    }
}

std::wstring timestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf(buf, 64, L"%04u%02u%02u_%02u%02u%02u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

void writeStackTXT(EXCEPTION_POINTERS* /*ep*/) {
    ensureDir();
    std::wstring path = g_dir + L"\\crash_" + timestamp() + L".stack.txt";
    FILE* f = _wfopen(path.c_str(), L"wt, ccs=UTF-8");
    if (!f) {
        return;
    }

    fwprintf(f, L"--- Crash stack (thread %lu) ---\n", GetCurrentThreadId());

    void* frames[128]{};
    USHORT n = RtlCaptureStackBackTrace(0, 128, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        fwprintf(f, L"%02u: %p\n", i, frames[i]);
    }

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        fwprintf(f, L"\nPrivateBytes: %zu MiB\n", static_cast<size_t>(pmc.PrivateUsage) / (1024 * 1024));
        fwprintf(f, L"WorkingSet : %zu MiB\n", static_cast<size_t>(pmc.WorkingSetSize) / (1024 * 1024));
    }

    fclose(f);
}

LONG WINAPI writeDump(EXCEPTION_POINTERS* ep) {
    ensureDir();
    std::wstring dmp = g_dir + L"\\crash_" + timestamp() + L".dmp";
    HANDLE hFile = CreateFileW(dmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithIndirectlyReferencedMemory |
            MiniDumpScanMemory |
            MiniDumpWithDataSegs |
            MiniDumpWithThreadInfo |
            MiniDumpWithHandleData);

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, type, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    writeStackTXT(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

void sigAbort(int) {
    __try {
        RaiseException(kSigAbortCode, 0, 0, nullptr);
    } __except(writeDump(GetExceptionInformation())) {}
    _exit(3);
}

void sigSegv(int) {
    __try {
        RaiseException(kSigSegvCode, 0, 0, nullptr);
    } __except(writeDump(GetExceptionInformation())) {}
    _exit(3);
}

void invalidParamHandler(const wchar_t* expression, const wchar_t* function,
                         const wchar_t* file, unsigned int line, uintptr_t) {
    __try {
        RaiseException(kInvalidParamCode, 0, 0, nullptr);
    } __except(writeDump(GetExceptionInformation())) {}
    (void)expression;
    (void)function;
    (void)file;
    (void)line;
    _exit(4);
}

void purecallHandler() {
    __try {
        RaiseException(kPureCallCode, 0, 0, nullptr);
    } __except(writeDump(GetExceptionInformation())) {}
    _exit(5);
}

void terminateHandler() {
    __try {
        RaiseException(kTerminateCode, 0, 0, nullptr);
    } __except(writeDump(GetExceptionInformation())) {}
    _exit(6);
}

} // namespace

void CrashHandler::install(const std::wstring& dumpDir) {
    std::scoped_lock lk(g_lock);
    g_dir = dumpDir.empty() ? L"." : dumpDir;

    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        return writeDump(ep);
    });

    _set_invalid_parameter_handler(invalidParamHandler);
    _set_purecall_handler(purecallHandler);
    std::set_terminate(terminateHandler);

    signal(SIGABRT, sigAbort);
    signal(SIGSEGV, sigSegv);
}
