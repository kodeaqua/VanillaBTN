#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

static void (*state_hook)(void);

/* Test-only crash diagnostics keep private paths and asset bytes out of logs. */
static LONG WINAPI boot_fault(EXCEPTION_POINTERS *fault) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT context = *fault->ContextRecord;
    STACKFRAME64 frame = {0};
    /* A null indirect call has a valid caller return address but no unwind
     * metadata at RIP=0. Recover that frame without dereferencing unsafe RAM. */
    if (context.Rip == 0) {
        DWORD64 caller = 0;
        SIZE_T bytes = 0;
        if (ReadProcessMemory(process, (void *)context.Rsp, &caller, sizeof(caller), &bytes)
            && bytes == sizeof(caller) && caller) {
            context.Rip = caller;
            context.Rsp += sizeof(caller);
        }
    }
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    SymInitialize(process, NULL, TRUE);
    fprintf(stderr, "caller rva=%llX\n", context.Rip - (DWORD64)GetModuleHandle(NULL));
    if (state_hook) state_hook();
    fprintf(stderr, "fault: code=%08lX pc=%llX address=%llX\n",
        fault->ExceptionRecord->ExceptionCode, fault->ContextRecord->Rip,
        (unsigned long long)(fault->ExceptionRecord->NumberParameters > 1 ?
            fault->ExceptionRecord->ExceptionInformation[1] : 0));
    for (unsigned i = 0; i < 24; ++i) {
        unsigned char storage[sizeof(SYMBOL_INFO) + 256] = {0};
        SYMBOL_INFO *symbol = (SYMBOL_INFO *)storage;
        DWORD64 offset = 0;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        if (SymFromAddr(process, frame.AddrPC.Offset, &offset, symbol))
            fprintf(stderr, "  %s+%llu\n", symbol->Name, offset);
        else if (frame.AddrPC.Offset >= (DWORD64)GetModuleHandle(NULL)
            && frame.AddrPC.Offset - (DWORD64)GetModuleHandle(NULL) < 0x10000000)
            fprintf(stderr, "  executable+%llX\n", frame.AddrPC.Offset - (DWORD64)GetModuleHandle(NULL));
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame,
            &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
    }
    SymCleanup(process);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void boot_abort(int signal_number) {
    (void)signal_number;
    RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, NULL);
}
#ifdef _DEBUG
static int boot_crt_report(int kind, char *message, int *result) {
    (void)message; (void)result;
    fprintf(stderr, "CRT report kind=%d\n", kind);
    boot_abort(0);
    return 1;
}
#endif

void boot_fault_install(void (*hook)(void)) {
    state_hook = hook;
    SetUnhandledExceptionFilter(boot_fault);
    signal(SIGABRT, boot_abort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, boot_crt_report);
#endif
}
