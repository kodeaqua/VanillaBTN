#include "sdk_card.h"
#include <libmcrd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static unsigned char data[16384], snapshot[PS1_CARD_SIZE];
static unsigned int buffer[8];

int main(void) {
    Ps1Card *card = sdk_card_get(0);
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)i;
    CHECK(ps1_card_write_file(card, "TEST", data, sizeof(data)) == PS1_CARD_OK);
    CHECK(MemCardReadFile(0, "TEST", buffer, 8184, sizeof(buffer)) == 0);
    CHECK(memcmp(buffer, data + 8184, sizeof(buffer)) == 0);
    card->dirty = false;
    memcpy(snapshot, card->data, sizeof(snapshot));
    CHECK(MemCardWriteFile(0, "TEST", buffer, -1, 4) == -1);
    CHECK(MemCardWriteFile(0, "TEST", buffer, sizeof(data) - 2, 4) == -1);
    CHECK(MemCardWriteFile(42, "TEST", buffer, 0, 4) == -1);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, -1) == -1);
    card->present = false;
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, 4) == -1);
    card->present = true;
    /* A checksum-valid truncated chain must fail before even its first byte. */
    int first = ps1_card_find_file(card, "TEST", NULL);
    unsigned char *entry = card->data + first * PS1_CARD_FRAME_SIZE;
    entry[8] = entry[9] = 0xFF;
    entry[127] = 0;
    for (int i = 0; i < 127; ++i) entry[127] ^= entry[i];
    memcpy(snapshot, card->data, sizeof(snapshot));
    memset(buffer, 0xA5, sizeof(buffer));
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, 4) == -1);
    CHECK(buffer[0] == 0xA5A5A5A5u);
    CHECK(MemCardWriteFile(0, "TEST", buffer, 0, 4) == -1);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    return 0;
}
