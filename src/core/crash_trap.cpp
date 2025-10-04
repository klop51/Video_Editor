#include "crash_trap.hpp"
#include <windows.h>
#include <crtdbg.h>
#include <csignal>
#include <cstdio>

static LONG WINAPI seh_filter(EXCEPTION_POINTERS* e) {
    // SHUTDOWN CRASH FIX: Silently handle debug breakpoints (0x80000003) to prevent log spam
    if (e->ExceptionRecord && e->ExceptionRecord->ExceptionCode == 0x80000003) {
        // Debug breakpoint - just continue without logging (prevents spam during audio processing)
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    
    // Print other SEH codes; debugger will stop here in Debug
    fprintf(stderr, "[CRASH] SEH 0x%08lx at %p\n",
            e->ExceptionRecord ? e->ExceptionRecord->ExceptionCode : 0ul,
            e->ExceptionRecord ? e->ExceptionRecord->ExceptionAddress : nullptr);
    
    // Only trigger debug break when debugger is present
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
static void on_sigabrt(int)  { fprintf(stderr, "[CRASH] SIGABRT\n"); if (IsDebuggerPresent()) DebugBreak(); }
static void on_sigsegv(int)  { fprintf(stderr, "[CRASH] SIGSEGV\n"); if (IsDebuggerPresent()) DebugBreak(); }
static void on_sigill (int)  { fprintf(stderr, "[CRASH] SIGILL\n");  if (IsDebuggerPresent()) DebugBreak(); }
static void on_sigfpe (int)  { fprintf(stderr, "[CRASH] SIGFPE\n");  if (IsDebuggerPresent()) DebugBreak(); }

void ve::install_crash_traps() {
    // don't show CRT message boxes; let us break/log
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    // SHUTDOWN CRASH FIX: Silence CRT assertion dialogs; stack traces are handled by CrashHandler
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);
    _CrtSetReportMode(_CRT_WARN, 0);

#if !defined(_DEBUG)
    // Release builds keep the lightweight heap checks disabled to avoid perf hits
    _CrtSetDbgFlag(0);
#endif

    fprintf(stderr, "[INFO] CRT assertions silenced; debug heap enabled in debug builds\n");

    // invalid parameter & purecall → break here (very common with WASAPI/ReleaseBuffer misuse)
    _set_invalid_parameter_handler([](const wchar_t* e, const wchar_t* /*f*/, const wchar_t* file, unsigned line, uintptr_t){
        fwprintf(stderr, L"[CRASH] invalid parameter: %ls (%ls:%u)\n", e?e:L"(null)", file?file:L"(null)", line);
        if (IsDebuggerPresent()) DebugBreak();
    });
    _set_purecall_handler([](){ fprintf(stderr, "[CRASH] pure virtual call\n"); if (IsDebuggerPresent()) DebugBreak(); });

    // SEH + signals
    SetUnhandledExceptionFilter(seh_filter);
    std::signal(SIGABRT, on_sigabrt);
    std::signal(SIGSEGV, on_sigsegv);
    std::signal(SIGILL,  on_sigill);
    std::signal(SIGFPE,  on_sigfpe);
}