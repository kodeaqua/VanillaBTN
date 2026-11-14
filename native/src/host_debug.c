#include "host_debug.h"

#ifdef _DEBUG

#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

static HANDLE debug_pipe = INVALID_HANDLE_VALUE;
static ULONGLONG debug_started_ms;
static CRITICAL_SECTION debug_lock;
static bool debug_lock_ready;

static bool companion_path(wchar_t *path, size_t capacity) {
    DWORD length = GetModuleFileNameW(NULL, path, (DWORD)capacity);
    if (!length || length >= capacity) return false;
    wchar_t *separator = wcsrchr(path, L'\\');
    if (!separator) return false;
    separator[1] = L'\0';
    return wcscat_s(path, capacity, L"vanillabtn_debug_console.exe") == 0;
}

bool host_debug_start(void) {
    wchar_t pipe_name[128];
    wchar_t viewer_path[MAX_PATH];
    wchar_t command[2 * MAX_PATH];
    PROCESS_INFORMATION process = {0};
    STARTUPINFOW startup = {sizeof(startup)};

    if (debug_pipe != INVALID_HANDLE_VALUE) return true;
    if (!companion_path(viewer_path, MAX_PATH)) return false;
    if (_snwprintf_s(pipe_name, _countof(pipe_name), _TRUNCATE,
            L"\\\\.\\pipe\\VanillaBTN-debug-%lu-%llu", GetCurrentProcessId(),
            (unsigned long long)GetTickCount64()) < 0) return false;

    debug_pipe = CreateNamedPipeW(pipe_name, PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 16384, 16384, 0, NULL);
    if (debug_pipe == INVALID_HANDLE_VALUE) return false;
    if (_snwprintf_s(command, _countof(command), _TRUNCATE, L"\"%s\" \"%s\"",
            viewer_path, pipe_name) < 0 ||
        !CreateProcessW(viewer_path, command, NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE, NULL, NULL, &startup, &process)) {
        CloseHandle(debug_pipe);
        debug_pipe = INVALID_HANDLE_VALUE;
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    BOOL connected = ConnectNamedPipe(debug_pipe, NULL);
    if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(debug_pipe);
        debug_pipe = INVALID_HANDLE_VALUE;
        return false;
    }
    InitializeCriticalSection(&debug_lock);
    debug_lock_ready = true;
    debug_started_ms = GetTickCount64();
    HOST_DEBUG_INFO("HOST", "Debug logger connected (pid=%lu)", GetCurrentProcessId());
    HOST_DEBUG_INFO("HOST", "An unexpected exit leaves this console open; crash details are also written to VanillaBTN-crash.txt in the temporary folder");
    return true;
}

void host_debug_log(const char *level, const char *area, const char *format, ...) {
    if (debug_pipe == INVALID_HANDLE_VALUE || !format) return;
    char message[1536];
    ULONGLONG elapsed = GetTickCount64() - debug_started_ms;
    int prefix = snprintf(message, sizeof(message), "[%6llu.%03llu] [%-5s] [%-8s] ",
        (unsigned long long)(elapsed / 1000),
        (unsigned long long)(elapsed % 1000), level ? level : "INFO",
        area ? area : "HOST");
    if (prefix < 0 || (size_t)prefix >= sizeof(message)) return;

    va_list args;
    va_start(args, format);
    int body = vsnprintf(message + prefix, sizeof(message) - (size_t)prefix, format, args);
    va_end(args);
    if (body < 0) return;
    size_t length = (size_t)prefix + (size_t)body;
    if (length >= sizeof(message) - 2) length = sizeof(message) - 2;
    message[length++] = '\r';
    message[length++] = '\n';

    EnterCriticalSection(&debug_lock);
    DWORD written;
    if (!WriteFile(debug_pipe, message, (DWORD)length, &written, NULL)) {
        CloseHandle(debug_pipe);
        debug_pipe = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&debug_lock);
    OutputDebugStringA(message);
}

void host_debug_stop(int exit_code) {
    if (debug_pipe == INVALID_HANDLE_VALUE) return;
    HOST_DEBUG_INFO("HOST", "Game process exited normally with code %d", exit_code);
    EnterCriticalSection(&debug_lock);
    FlushFileBuffers(debug_pipe);
    DisconnectNamedPipe(debug_pipe);
    CloseHandle(debug_pipe);
    debug_pipe = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&debug_lock);
    if (debug_lock_ready) {
        DeleteCriticalSection(&debug_lock);
        debug_lock_ready = false;
    }
}

#endif
