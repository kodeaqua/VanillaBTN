#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#if !defined(_M_X64)
#error VanillaBTN requires the Windows x64 compiler target.
#endif

static int adjacent_file(wchar_t path[MAX_PATH], const wchar_t *filename) {
    /* shortcut: MAX_PATH bounds startup paths, add long-path support when required */
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return 0;
    }

    wchar_t *name = wcsrchr(path, L'\\');
    return name != NULL && wcscpy_s(name + 1, MAX_PATH - (name + 1 - path), filename) == 0;
}

static int import_disc(const wchar_t *disc) {
    DWORD attributes = GetFileAttributesW(disc);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        wcslen(disc) >= MAX_PATH || wcschr(disc, L'"')) {
        fputs("Could not open the supplied disc image.\n", stderr);
        return 0;
    }

    wchar_t output[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", output, MAX_PATH);
    if (length == 0 || length >= MAX_PATH || wcschr(output, L'"') ||
        wcscat_s(output, MAX_PATH, L"\\VanillaBTN\\disc") != 0) {
        fputs("Could not locate the local data directory.\n", stderr);
        return 0;
    }

    wchar_t script[MAX_PATH];
    if (!adjacent_file(script, L"tools\\psx_disc.py") ||
        GetFileAttributesW(script) == INVALID_FILE_ATTRIBUTES) {
        fputs("Could not locate the bundled disc importer.\n", stderr);
        return 0;
    }

    wchar_t *search_dirs = NULL;
    wchar_t python[MAX_PATH];
    DWORD python_length = 0;
    if (_wdupenv_s(&search_dirs, NULL, L"PATH") == 0 && search_dirs && *search_dirs) {
        python_length = SearchPathW(search_dirs, L"python.exe", NULL, MAX_PATH, python, NULL);
    }
    free(search_dirs);
    if (python_length == 0 || python_length >= MAX_PATH) {
        fputs("Python 3.10 or newer is required on PATH to import a disc.\n", stderr);
        return 0;
    }

    wchar_t command[5 * MAX_PATH];
    if (swprintf_s(command, _countof(command),
                   L"\"%ls\" -I \"%ls\" import --out \"%ls\" -- \"%ls\"", python, script, output,
                   disc) < 0) {
        return 0;
    }

    STARTUPINFOW startup = {0};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process;
    if (!CreateProcessW(python, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        fputs("Could not start Python for disc import.\n", stderr);
        return 0;
    }
    CloseHandle(process.hThread);
    DWORD status;
    int succeeded = WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0 &&
                    GetExitCodeProcess(process.hProcess, &status) && status == 0;
    CloseHandle(process.hProcess);
    if (!succeeded) {
        fputs("Reference disc import failed.\n", stderr);
    }
    return succeeded;
}

#ifdef VANILLABTN_DEBUG
static int write_startup_log(void) {
    wchar_t path[MAX_PATH];
    if (!adjacent_file(path, L"VanillaBTN.log")) {
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

int wmain(int argc, wchar_t *argv[]) {
#ifdef VANILLABTN_DEBUG
    if (!write_startup_log()) {
        fputs("Could not write VanillaBTN.log.\n", stderr);
        return 1;
    }
#endif
    if (argc != 1) {
        if (argc != 3 || wcscmp(argv[1], L"--import-disc") != 0) {
            fputs("Usage: vanillabtn [--import-disc PATH]\n", stderr);
            return 2;
        }
        if (!import_disc(argv[2])) {
            return 1;
        }
        puts("Reference disc imported.");
        return 0;
    }
    puts("VanillaBTN native build initialized.");
    return 0;
}
