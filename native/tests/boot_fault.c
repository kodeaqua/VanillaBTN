#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>

static void (*state_hook)(void);

/* Test-only crash diagnostics keep private paths and asset bytes out of logs. */
static LONG WINAPI boot_fault(EXCEPTION_POINTERS *fault) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT context = *fault->ContextRecord;
    STACKFRAME64 frame = {0};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    SymInitialize(process, NULL, TRUE);
    if (state_hook) state_hook();
    fprintf(stderr, "fault: code=%08lX pc=%llX address=%llX\n",
        fault->ExceptionRecord->ExceptionCode, context.Rip,
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
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame,
            &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
    }
    SymCleanup(process);
    return EXCEPTION_EXECUTE_HANDLER;
}

void boot_fault_install(void (*hook)(void)) {
    state_hook = hook;
    SetUnhandledExceptionFilter(boot_fault);
}
