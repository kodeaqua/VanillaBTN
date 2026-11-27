#include <stdio.h>

#if !defined(_M_X64)
#error VanillaBTN requires the Windows x64 compiler target.
#endif

#ifdef VANILLABTN_DEBUG
#include <windows.h>
#include <wchar.h>

static int write_startup_log(void) {
    /* shortcut: MAX_PATH bounds the log path, add long-path support when required */
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return 0;
    }

    wchar_t *name = wcsrchr(path, L'\\');
    if (name == NULL || wcscpy_s(name + 1, MAX_PATH - (name + 1 - path), L"VanillaBTN.log") != 0) {
        return 0;
    }

    FILE *log;
    if (_wfopen_s(&log, path, L"w") != 0) {
        return 0;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);
    int written =
        fprintf(log, "%04d-%02d-%02dT%02d:%02d:%02d INFO startup VanillaBTN initialized\n",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    int flushed = fflush(log);
    int closed = fclose(log);
    return written >= 0 && flushed == 0 && closed == 0;
}
#endif

int main(void) {
#ifdef VANILLABTN_DEBUG
    if (!write_startup_log()) {
        fputs("Could not write VanillaBTN.log.\n", stderr);
        return 1;
    }
#endif
    puts("VanillaBTN native build initialized.");
    return 0;
}
