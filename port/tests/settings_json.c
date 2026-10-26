#include "platform/settings_json.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    const char *value;
    size_t length;
    int number;
    const char original[] = "{\"schema_version\":1, "
                            "\"other\":{\"text\":\"a\\\"b\",\"list\":[true,null,-1.5e2]},"
                            "\"graphics\":{\"width\":640}}\n";
    CHECK(hm_json_property(original, strlen(original), "graphics", &value, &length) == 1);
    CHECK(hm_json_integer(value, length, "width", &number) && number == 640);
    char output[512], roundtrip[512];
    CHECK(hm_json_replace(original, strlen(original), "graphics", "{\"width\":1920}", output,
                          sizeof(output)));
    CHECK(strstr(output, "\"other\":{\"text\":\"a\\\"b\",\"list\":[true,null,-1.5e2]}") != NULL);
    CHECK(strstr(output, "{\"width\":1920}") != NULL);
    CHECK(hm_json_replace(output, strlen(output), "graphics", "{\"width\":640}", roundtrip,
                          sizeof(roundtrip)));
    CHECK(!strcmp(roundtrip, original));
    CHECK(hm_json_replace("{}", 2, "graphics", "{}", output, sizeof(output)));
    CHECK(hm_json_property(output, strlen(output), "graphics", &value, &length) == 1 &&
          length == 2);
    CHECK(hm_json_replace("{ \"a\":1 }", strlen("{ \"a\":1 }"), "graphics", "{}", output,
                          sizeof(output)));
    CHECK(hm_json_property(output, strlen(output), "a", &value, &length) == 1);
    CHECK(!hm_json_replace(original, strlen(original), "graphics", "{}", output, 2));
    const char *invalid[] = {
        "{",         "[]",         "{\"x\":01}", "{\"x\":true,}",    "{\"x\":\"\\q\"}",
        "{\"x\":-}", "{\"x\":1.}", "{\"x\":1e}", "{\"x\":null}junk", "{\"x\":1,\"x\":2}"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        CHECK(hm_json_property(invalid[i], strlen(invalid[i]), "x", &value, &length) == -1);
    CHECK(!hm_json_replace("{}", 2, "graphics", "[", output, sizeof(output)));
    CHECK(!hm_json_integer("{\"x\":1.5}", 9, "x", &number));
    const char huge[] = "{\"x\":99999999999999999999}";
    CHECK(!hm_json_integer(huge, strlen(huge), "x", &number));
    const char bad_utf8[] = "{\"x\":\"\xc0\xaf\"}";
    CHECK(hm_json_property(bad_utf8, strlen(bad_utf8), "x", &value, &length) == -1);
    const char unicode[] = "{\"x\":\"\xc3\xa9\"}";
    CHECK(hm_json_property(unicode, strlen(unicode), "x", &value, &length) == 1);
    return 0;
}
