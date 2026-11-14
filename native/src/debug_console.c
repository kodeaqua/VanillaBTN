#include <Windows.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, wchar_t **argv) {
    if (argc == 2 && wcscmp(argv[1], L"--check-build") == 0) return 0;
    if (argc != 2) return 2;

    SetConsoleTitleW(L"VanillaBTN Debug Log");
    HANDLE pipe = CreateFileW(argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Could not connect to the VanillaBTN debug logger (error %lu).\n",
            GetLastError());
        return 1;
    }

    puts("VanillaBTN Debug Log");
    puts("This window is a separate process and remains open if the game crashes.\n");
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    char buffer[4096];
    DWORD received;
    while (ReadFile(pipe, buffer, sizeof(buffer), &received, NULL) && received != 0) {
        DWORD written;
        WriteFile(output, buffer, received, &written, NULL);
    }
    CloseHandle(pipe);

    puts("\nThe game process has ended or the logging connection was lost.");
    puts("Review the final ERROR/WARN entries and the Debug crash report if present.");
    puts("Press Enter to close this window.");
    (void)getchar();
    return 0;
}
