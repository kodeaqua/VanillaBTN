#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <strsafe.h>

#ifdef VANILLABTN_HAS_GAME
#include <shlobj.h>
#include <psyz/cd.h>

int vanillabtn_game_main(void);
#ifdef VANILLABTN_DEBUG
void vanillabtn_start_debug_menu(void);
#endif
#endif

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
    return name != NULL && SUCCEEDED(StringCchCopyW(name + 1, MAX_PATH - (name + 1 - path),
                                                    filename));
}

enum DiscPriority {
    NOT_A_DISC,
    DISC_IMAGE,
    DISC_DESCRIPTOR,
};

static enum DiscPriority disc_priority(const wchar_t *name) {
    const wchar_t *extension = wcsrchr(name, L'.');
    if (extension == NULL) {
        return NOT_A_DISC;
    }
    if (_wcsicmp(extension, L".cue") == 0) {
        return DISC_DESCRIPTOR;
    }
    return _wcsicmp(extension, L".bin") == 0 || _wcsicmp(extension, L".iso") == 0 ? DISC_IMAGE
                                                                                : NOT_A_DISC;
}

static int run_disc_tool(const wchar_t *operation, const wchar_t *disc, const wchar_t *output,
                         const wchar_t *user_data) {
    DWORD attributes = GetFileAttributesW(disc);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        wcslen(disc) >= MAX_PATH || wcschr(disc, L'"')) {
        fputs("Could not open the supplied disc image.\n", stderr);
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

    wchar_t data_option[MAX_PATH + 32] = L"";
    if (user_data != NULL &&
        swprintf_s(data_option, _countof(data_option), L" --user-data \"%ls\"", user_data) < 0) {
        return 0;
    }
    wchar_t command[5 * MAX_PATH + 64];
    if (swprintf_s(command, _countof(command),
                   L"\"%ls\" -I \"%ls\" %ls --out \"%ls\"%ls -- \"%ls\"", python, script,
                   operation, output, data_option, disc) < 0) {
        return 0;
    }

    STARTUPINFOW startup = {0};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process;
    if (!CreateProcessW(python, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        fputs("Could not start Python for disc verification.\n", stderr);
        return 0;
    }
    CloseHandle(process.hThread);
    DWORD status;
    int succeeded = WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0 &&
                    GetExitCodeProcess(process.hProcess, &status) && status == 0;
    CloseHandle(process.hProcess);
    return succeeded;
}

static int import_disc(const wchar_t *disc) {
    wchar_t output[MAX_PATH];
    return adjacent_file(output, L"disc") && run_disc_tool(L"import", disc, output, NULL);
}

/* Prefer a CUE to its companion image. Require an explicit path if the choice is ambiguous. */
static int find_adjacent_disc(wchar_t path[MAX_PATH]) {
    wchar_t pattern[MAX_PATH];
    WIN32_FIND_DATAW entry;
    if (!adjacent_file(pattern, L"*")) {
        return -1;
    }
    HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) {
        return -1;
    }
    enum DiscPriority best = NOT_A_DISC;
    unsigned int count = 0;
    wchar_t name[MAX_PATH];
    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        enum DiscPriority priority = disc_priority(entry.cFileName);
        if (priority > best) {
            best = priority;
            count = 1;
            StringCchCopyW(name, MAX_PATH, entry.cFileName);
        } else if (priority == best && priority != NOT_A_DISC) {
            count++;
        }
    } while (FindNextFileW(search, &entry));
    DWORD error = GetLastError();
    FindClose(search);
    if (error != ERROR_NO_MORE_FILES || count > 1) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    return adjacent_file(path, name) ? 1 : -1;
}

#ifdef VANILLABTN_HAS_GAME
static int documents_data_directory(wchar_t path[MAX_PATH]) {
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
        return 0;
    }
    wchar_t *documents = NULL;
    HRESULT result = SHGetKnownFolderPath(&FOLDERID_Documents, KF_FLAG_DONT_VERIFY, NULL,
                                          &documents);
    int found = SUCCEEDED(result) &&
                SUCCEEDED(StringCchPrintfW(path, MAX_PATH, L"%ls\\VanillaBTN", documents));
    CoTaskMemFree(documents);
    CoUninitialize();
    return found;
}

static int launch_game(const wchar_t *disc) {
    wchar_t directory[MAX_PATH];
    if (!documents_data_directory(directory)) {
        fputs("Could not locate Documents for VanillaBTN saves.\n", stderr);
        return 1;
    }

    wchar_t temp_directory[MAX_PATH];
    wchar_t cue[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, temp_directory);
    if (length == 0 || length >= MAX_PATH || wcschr(temp_directory, L'"') ||
        GetTempFileNameW(temp_directory, L"vbt", 0, cue) == 0) {
        fputs("Could not create a temporary disc descriptor.\n", stderr);
        return 1;
    }

    char mount_path[MAX_PATH];
    BOOL substituted = FALSE;
    UINT code_page = GetACP();
    DWORD flags = code_page == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    BOOL *used_default = code_page == CP_UTF8 ? NULL : &substituted;
    int mounted = WideCharToMultiByte(code_page, flags, cue, -1, mount_path, sizeof(mount_path),
                                      NULL, used_default) &&
                  !substituted && run_disc_tool(L"prepare-cue", disc, cue, directory) &&
                  Psyz_CdSetDiskPath(mount_path) == 0;
    DeleteFileW(cue);
    if (!mounted) {
        fputs("Could not mount the USA reference disc.\n", stderr);
        return 1;
    }
    if (!SetCurrentDirectoryW(directory)) {
        fputs("Could not use the Documents save directory.\n", stderr);
        return 1;
    }
    return vanillabtn_game_main();
}
#endif

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
#ifdef VANILLABTN_HAS_GAME
        if (argc == 3 && wcscmp(argv[1], L"--disc") == 0) {
            return launch_game(argv[2]);
        }
#ifdef VANILLABTN_DEBUG
        if (argc == 4 && wcscmp(argv[1], L"--disc") == 0 &&
            wcscmp(argv[3], L"--debug-menu") == 0) {
            vanillabtn_start_debug_menu();
            return launch_game(argv[2]);
        }
        if (argc == 4 && wcscmp(argv[1], L"--debug-menu") == 0 &&
            wcscmp(argv[2], L"--disc") == 0) {
            vanillabtn_start_debug_menu();
            return launch_game(argv[3]);
        }
#endif
#endif
        int install = wcscmp(argv[1], L"--install") == 0;
        int dropped_disc = argc == 2 && disc_priority(argv[1]) != NOT_A_DISC;
        if (!dropped_disc && !(install && argc == 2) &&
            (argc != 3 || (!install && wcscmp(argv[1], L"--import-disc") != 0))) {
#ifdef VANILLABTN_HAS_GAME
            fputs("Usage: vanillabtn [PATH | --disc PATH | --install [PATH]]\n", stderr);
#else
            fputs("Usage: vanillabtn [PATH | --install [PATH]]\n", stderr);
#endif
            return 2;
        }
        wchar_t adjacent[MAX_PATH];
        if (install && argc == 2 && find_adjacent_disc(adjacent) != 1) {
            fputs("Could not select a disc next to the executable. Use --install PATH.\n", stderr);
            return 1;
        }
        const wchar_t *disc = argc == 3 ? argv[2] : dropped_disc ? argv[1] : adjacent;
        if (!import_disc(disc)) {
            fputs("Reference disc import failed.\n", stderr);
            return 1;
        }
        puts("Reference disc imported.");
#ifdef VANILLABTN_HAS_GAME
        if (dropped_disc) {
            return launch_game(disc);
        }
#endif
        return 0;
    }
#ifdef VANILLABTN_HAS_GAME
    wchar_t disc[MAX_PATH];
    int found = find_adjacent_disc(disc);
    if (found > 0) {
        return launch_game(disc);
    }
    if (found < 0) {
        fputs("Could not select a disc next to the executable. Use --disc PATH.\n", stderr);
        return 1;
    }
    puts("Place a USA disc image next to the executable or drop it onto the executable.");
#else
    puts("VanillaBTN native build initialized.");
#endif
    return 0;
}
