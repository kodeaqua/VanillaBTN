#include "sdk_card.h"
#include <libmcx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

static unsigned char snapshot[PS1_CARD_SIZE];

static void completion(long command, long expected) {
    long actual_command = -1, result = -1;
    CHECK(McxSync(1, &actual_command, &result) == 1);
    /* Numeric SDK contract values, independent of this facade's macros. */
    CHECK(actual_command == command && result == expected);
}

int main(void) {
    Ps1Card *card = sdk_card_get(0);
    CHECK(card && card->present);
    memcpy(snapshot, card->data, sizeof(snapshot));
    unsigned long serial = 0xCAFEBABE;
    CHECK(McxGetSerial(0, &serial) == 1);
    completion(10, 2); /* Standard PS1 card is not a PocketStation. */
    CHECK(serial == 0xCAFEBABE);
    unsigned char output[128], unchanged[128];
    memset(output, 0xA5, sizeof(output));
    memcpy(unchanged, output, sizeof(output));
    CHECK(McxAllInfo(0, output) == 1);
    completion(12, 2);
    CHECK(McxGetTime(0, output) == 1);
    completion(3, 2);
    CHECK(McxGetMem(0, output, 0, sizeof(output)) == 1);
    completion(4, 2);
    CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
    CHECK(McxSetMem(0, output, 0, sizeof(output)) == 1);
    completion(5, 2);
    CHECK(McxExecApl(0, 0, 0) == 1);
    completion(2, 2);
    CHECK(McxExecFlag(0, 1, 1) == 1);
    completion(11, 2);
    CHECK(McxReadDev(0, 0, output, output) == 1);
    completion(14, 2);
    CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
    CHECK(McxWriteDev(0, 0, output, output) == 1);
    completion(15, 2);
    CHECK(memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    card->present = false;
    CHECK(McxGetTime(0, output) == 1);
    completion(3, 1); /* Missing card, distinct from a non-PDA card. */
    card->present = true;
    CHECK(McxAllInfo(42, output) == 1);
    completion(12, 2); /* Unknown port must not alias a real card. */
    CHECK(McxErrNoCard == 1 && McxErrInvalid == 2 && McxErrNewCard == 3);
    CHECK(McxFuncExecFlag == 11);
    return 0;
}
