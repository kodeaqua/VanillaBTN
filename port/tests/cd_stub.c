#include <libcd.h>

#include <string.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    if (strcmp(argv[1], "CdInit") == 0) {
        return CdInit();
    }
    if (strcmp(argv[1], "CdSearchFile") == 0) {
        return CdSearchFile(NULL, "\\A_FILE.BIN;1") != NULL;
    }
    if (strcmp(argv[1], "CdControl") == 0) {
        return CdControl(CdlSetloc, NULL, NULL);
    }
    if (strcmp(argv[1], "CdRead") == 0) {
        return CdRead(1, NULL, 0x80);
    }
    if (strcmp(argv[1], "CdReadSync") == 0) {
        return CdReadSync(0, NULL);
    }
    if (strcmp(argv[1], "CdSync") == 0) {
        return CdSync(0, NULL);
    }
    if (strcmp(argv[1], "CdReadBreak") == 0) {
        return CdReadBreak();
    }
    if (strcmp(argv[1], "CdFlush") == 0) {
        return CdFlush();
    }
    return 2;
}
