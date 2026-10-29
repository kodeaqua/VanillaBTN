#include "platform/crash_handler.h"
#include "platform/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static const char *hm_exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "Access Violation (Memory Read/Write Fault)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "Array Bounds Exceeded";
    case EXCEPTION_BREAKPOINT:
        return "Breakpoint Trap";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "Datatype Misalignment";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "Floating-point Divide by Zero";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "Illegal Instruction";
    case EXCEPTION_IN_PAGE_ERROR:
        return "In Page Error";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "Integer Divide by Zero";
    case EXCEPTION_INT_OVERFLOW:
        return "Integer Overflow";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "Privileged Instruction";
    case EXCEPTION_STACK_OVERFLOW:
        return "Stack Overflow";
    default:
        return "Unknown Fatal Exception";
    }
}

static LONG WINAPI hm_unhandled_exception_filter(EXCEPTION_POINTERS *ep) {
    DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    void *addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : NULL;
    const char *name = hm_exception_name(code);

#if defined(NDEBUG)
    /* Release mode: show native graphical message box */
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
        "Harvest Moon: Back to Nature (%s) has encountered an error and needs to close.\n\n"
        "Fault:   %s\n"
        "Code:    0x%08lX\n"
        "Address: %p\n\n"
        "Please report this crash with the details above.",
        hm_version_window_title(), name, (unsigned long)code, addr);

    MessageBoxA(NULL, buffer, "Harvest Moon - Crash Handler", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
#else
    /* Debug mode: print to console */
    fprintf(stderr,
        "\n================ [CRASH DETECTED (DEBUG MODE)] ================\n"
        "Fault:   %s\n"
        "Code:    0x%08lX\n"
        "Address: %p\n"
        "===============================================================\n",
        name, (unsigned long)code, addr);
    fflush(stderr);
#endif

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void hm_signal_handler(int sig) {
    const char *sig_name = "Unknown Signal";
    switch (sig) {
    case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault)"; break;
    case SIGABRT: sig_name = "SIGABRT (Abnormal Termination)"; break;
    case SIGFPE:  sig_name = "SIGFPE (Arithmetic Exception)"; break;
    case SIGILL:  sig_name = "SIGILL (Illegal Instruction)"; break;
    }

#if defined(NDEBUG) && defined(_WIN32)
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
        "Harvest Moon: Back to Nature (%s) encountered a fatal signal.\n\n"
        "Signal: %s (%d)\n\n"
        "Please report this crash to the developers.",
        hm_version_window_title(), sig_name, sig);
    MessageBoxA(NULL, buffer, "Harvest Moon - Crash Handler", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
#else
    fprintf(stderr, "\n[CRASH SIGNAL] %s (%d)\n", sig_name, sig);
    fflush(stderr);
#endif

    exit(128 + sig);
}

void hm_crash_handler_install(void) {
#if defined(_WIN32)
    SetUnhandledExceptionFilter(hm_unhandled_exception_filter);
#endif
    signal(SIGSEGV, hm_signal_handler);
    signal(SIGABRT, hm_signal_handler);
    signal(SIGFPE,  hm_signal_handler);
    signal(SIGILL,  hm_signal_handler);
}
