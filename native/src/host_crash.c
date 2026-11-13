/* Native Debug crash reports, separate from gameplay and the PS1 build.
 * Do not record absolute paths, original asset bytes or card contents. */
#include "host_crash.h"
#include "host_debug.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <crtdbg.h>
#include <signal.h>
#include <stdlib.h>
#include <wchar.h>

static void (*write_game_state)(FILE *);
static volatile LONG reporting;
static int has_system_error;
static unsigned char system_error_type;
static long system_error_code;

/* The Windows API belongs in this diagnostic-only translation unit: game
 * headers define PS1 types (including RECT) that collide with windef.h. */
static FILE *open_report(const wchar_t *name, const char *fallback) {
    wchar_t path[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, path);
    FILE *file = NULL;

    if (length > 0 && length < MAX_PATH &&
        length + wcslen(name) + 1 <= MAX_PATH) {
        wcscat_s(path, MAX_PATH, name);
        if (_wfopen_s(&file, path, L"w") == 0) return file;
    }
    /* Read-only installation directories need not be writable; the temporary
     * folder is preferred, with the working directory as a last resort. */
    fopen_s(&file, fallback, "w");
    return file;
}

static FILE *open_crash_report(void) {
    return open_report(L"VanillaBTN-crash.txt", "VanillaBTN-crash.txt");
}

FILE *host_crash_open_audio_report(void) {
    return open_report(L"VanillaBTN-audio.txt", "VanillaBTN-audio.txt");
}

static LONG WINAPI host_crash_exception(EXCEPTION_POINTERS *fault) {
    if (InterlockedCompareExchange(&reporting, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

    FILE *file = open_crash_report();
    if (file != NULL) {
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        CONTEXT context = *fault->ContextRecord;
        STACKFRAME64 frame = {0};
        DWORD64 executable = (DWORD64)GetModuleHandleW(NULL);
        int symbols_ready;

        /* An indirect call through NULL has no unwind metadata at RIP=0.
         * The return address is recoverable from its stack, if readable. */
        if (context.Rip == 0) {
            DWORD64 caller = 0;
            SIZE_T count = 0;
            if (ReadProcessMemory(process, (void *)context.Rsp, &caller,
                    sizeof(caller), &count) && count == sizeof(caller) && caller) {
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

        fprintf(file, "VanillaBTN Debug crash report\n");
        fprintf(file, "exception=0x%08lX", fault->ExceptionRecord->ExceptionCode);
        if (fault->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            fault->ExceptionRecord->NumberParameters >= 2) {
            fprintf(file, " access=%s address=0x%llX",
                fault->ExceptionRecord->ExceptionInformation[0] == 0 ? "read" :
                fault->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "execute",
                (unsigned long long)fault->ExceptionRecord->ExceptionInformation[1]);
        }
        fprintf(file, "\n");
        if (context.Rip >= executable && context.Rip - executable < 0x10000000)
            fprintf(file, "executable_rva=0x%llX\n", (unsigned long long)(context.Rip - executable));
        if (has_system_error)
            fprintf(file, "SystemError type=0x%02X code=0x%lX\n",
                system_error_type, system_error_code);
        if (write_game_state) write_game_state(file);

        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        symbols_ready = SymInitialize(process, NULL, TRUE);
        fprintf(file, "stack:\n");
        for (unsigned i = 0; i < 32 && frame.AddrPC.Offset; ++i) {
            union {
                SYMBOL_INFO symbol;
                unsigned char bytes[sizeof(SYMBOL_INFO) + 256];
            } scratch = {0};
            DWORD64 displacement = 0;
            SYMBOL_INFO *symbol = &scratch.symbol;
            DWORD64 base = symbols_ready ? SymGetModuleBase64(process, frame.AddrPC.Offset) : 0;

            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            if (symbols_ready && SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
                fprintf(file, "  #%u %s+0x%llX\n", i, symbol->Name,
                    (unsigned long long)displacement);
            else if (base)
                fprintf(file, "  #%u module+0x%llX\n", i,
                    (unsigned long long)(frame.AddrPC.Offset - base));
            else
                fprintf(file, "  #%u unresolved\n", i);

            DWORD64 previous = frame.AddrPC.Offset;
            if (!symbols_ready || !StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread,
                    &frame, &context, NULL, SymFunctionTableAccess64,
                    SymGetModuleBase64, NULL) || frame.AddrPC.Offset == previous) break;
        }
        if (symbols_ready) SymCleanup(process);
        fclose(file);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static void host_crash_abort(int signal_number) {
    (void)signal_number;
    RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

static int host_crash_crt_report(int kind, char *message, int *result) {
    (void)kind;
    (void)message;
    (void)result;
    host_crash_abort(0);
    return 1;
}

void host_crash_note_system_error(char type, long code) {
    has_system_error = 1;
    system_error_type = (unsigned char)type;
    system_error_code = code;
    HOST_DEBUG_ERROR("GAME", "SystemError type=0x%02X code=0x%lX",
        (unsigned char)type, code);
}

void host_crash_install(void (*state_writer)(FILE *)) {
    write_game_state = state_writer;
    SetUnhandledExceptionFilter(host_crash_exception);
    signal(SIGABRT, host_crash_abort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, host_crash_crt_report);
}
