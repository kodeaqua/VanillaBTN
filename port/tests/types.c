#include "compat/types.h"

#include <limits.h>

_Static_assert(CHAR_BIT == 8, "Asset formats require eight-bit bytes");
_Static_assert(sizeof(s8) == 1 && sizeof(u8) == 1, "Byte layout");
_Static_assert(sizeof(s16) == 2 && sizeof(u16) == 2, "Halfword layout");
_Static_assert(sizeof(s64) == 8 && sizeof(u64) == 8, "Doubleword layout");

int main(void) {
    const u32 word = UINT32_C(0x04030201);
    const unsigned char *bytes = (const unsigned char *)&word;
    return bytes[0] == 1 && bytes[1] == 2 && bytes[2] == 3 && bytes[3] == 4 ? 0 : 1;
}
