#include "platform/settings_json.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p, *end;
} hm_json_cursor;
static void hm_space(hm_json_cursor *c) {
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\r' || *c->p == '\n' || *c->p == '\t'))
        ++c->p;
}
static int hm_string(hm_json_cursor *c) {
    if (c->p == c->end || *c->p++ != '"')
        return 0;
    while (c->p < c->end) {
        unsigned char ch = (unsigned char)*c->p++;
        if (ch == '"')
            return 1;
        if (ch < 32)
            return 0;
        if (ch >= 128) {
            unsigned continuation = ch >= 0xC2 && ch <= 0xDF   ? 1
                                    : ch >= 0xE0 && ch <= 0xEF ? 2
                                    : ch >= 0xF0 && ch <= 0xF4 ? 3
                                                               : 0;
            if (!continuation || (size_t)(c->end - c->p) < continuation)
                return 0;
            unsigned first = (unsigned char)*c->p;
            if ((ch == 0xE0 && first < 0xA0) || (ch == 0xED && first > 0x9F) ||
                (ch == 0xF0 && first < 0x90) || (ch == 0xF4 && first > 0x8F))
                return 0;
            for (unsigned i = 0; i < continuation; ++i) {
                unsigned next = (unsigned char)*c->p++;
                if (next < 0x80 || next > 0xBF)
                    return 0;
            }
        }
        if (ch == '\\') {
            if (c->p == c->end)
                return 0;
            char escape = *c->p++;
            if (!escape)
                return 0;
            if (escape == 'u') {
                for (int i = 0; i < 4; ++i) {
                    if (c->p == c->end)
                        return 0;
                    ch = (unsigned char)*c->p++;
                    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                          (ch >= 'A' && ch <= 'F')))
                        return 0;
                }
            } else if (!strchr("\"\\/bfnrt", escape))
                return 0;
        }
    }
    return 0;
}
static int hm_value(hm_json_cursor *c, unsigned depth) {
    hm_space(c);
    if (c->p == c->end || depth > 32)
        return 0;
    char ch = *c->p;
    if (ch == '"')
        return hm_string(c);
    if (ch == '{' || ch == '[') {
        char close = ch == '{' ? '}' : ']';
        ++c->p;
        hm_space(c);
        if (c->p < c->end && *c->p == close) {
            ++c->p;
            return 1;
        }
        for (;;) {
            if (ch == '{') {
                if (!hm_string(c))
                    return 0;
                hm_space(c);
                if (c->p == c->end || *c->p++ != ':')
                    return 0;
            }
            if (!hm_value(c, depth + 1))
                return 0;
            hm_space(c);
            if (c->p == c->end)
                return 0;
            if (*c->p == close) {
                ++c->p;
                return 1;
            }
            if (*c->p++ != ',')
                return 0;
            hm_space(c);
        }
    }
    const char *words[] = {"true", "false", "null"};
    for (size_t i = 0; i < 3; ++i) {
        size_t n = strlen(words[i]);
        if ((size_t)(c->end - c->p) >= n && !memcmp(c->p, words[i], n)) {
            c->p += n;
            return 1;
        }
    }
    if (*c->p == '-')
        ++c->p;
    if (c->p == c->end)
        return 0;
    if (*c->p == '0')
        ++c->p;
    else {
        if (*c->p < '1' || *c->p > '9')
            return 0;
        while (c->p < c->end && *c->p >= '0' && *c->p <= '9')
            ++c->p;
    }
    if (c->p < c->end && *c->p == '.') {
        ++c->p;
        if (c->p == c->end || *c->p < '0' || *c->p > '9')
            return 0;
        while (c->p < c->end && *c->p >= '0' && *c->p <= '9')
            ++c->p;
    }
    if (c->p < c->end && (*c->p == 'e' || *c->p == 'E')) {
        ++c->p;
        if (c->p < c->end && (*c->p == '+' || *c->p == '-'))
            ++c->p;
        if (c->p == c->end || *c->p < '0' || *c->p > '9')
            return 0;
        while (c->p < c->end && *c->p >= '0' && *c->p <= '9')
            ++c->p;
    }
    return 1;
}
int hm_json_property(const char *text, size_t bytes, const char *key, const char **value,
                     size_t *length) {
    if (!text || !key || !value || !length)
        return -1;
    hm_json_cursor c = {text, text + bytes};
    hm_space(&c);
    if (c.p == c.end || *c.p++ != '{')
        return -1;
    hm_space(&c);
    int found = 0;
    if (c.p < c.end && *c.p == '}')
        ++c.p;
    else
        for (;;) {
            const char *name = c.p;
            if (!hm_string(&c))
                return -1;
            int matches =
                (size_t)(c.p - name) == strlen(key) + 2 && !memcmp(name + 1, key, strlen(key));
            hm_space(&c);
            if (c.p == c.end || *c.p++ != ':')
                return -1;
            hm_space(&c);
            const char *start = c.p;
            if (!hm_value(&c, 0))
                return -1;
            if (matches) {
                if (found)
                    return -1;
                found = 1;
                *value = start;
                *length = (size_t)(c.p - start);
            }
            hm_space(&c);
            if (c.p == c.end)
                return -1;
            if (*c.p == '}') {
                ++c.p;
                break;
            }
            if (*c.p++ != ',')
                return -1;
            hm_space(&c);
        }
    hm_space(&c);
    return c.p == c.end ? found : -1;
}
int hm_json_integer(const char *text, size_t bytes, const char *key, int *value) {
    const char *p;
    size_t length;
    if (hm_json_property(text, bytes, key, &p, &length) != 1 || !length || length >= 32)
        return 0;
    char copy[32], *end;
    memcpy(copy, p, length);
    copy[length] = 0;
    errno = 0;
    long number = strtol(copy, &end, 10);
    if (errno == ERANGE || end != copy + length || number < INT_MIN || number > INT_MAX)
        return 0;
    *value = (int)number;
    return 1;
}
int hm_json_replace(const char *text, size_t bytes, const char *key, const char *replacement,
                    char *output, size_t capacity) {
    const char *value = NULL;
    size_t length = 0;
    int found = hm_json_property(text, bytes, key, &value, &length);
    if (found < 0 || !output || !replacement || strchr(key, '"') || strchr(key, '\\'))
        return 0;
    hm_json_cursor c = {replacement, replacement + strlen(replacement)};
    if (!hm_value(&c, 0))
        return 0;
    hm_space(&c);
    if (c.p != c.end)
        return 0;
    size_t prefix, suffix, n = strlen(replacement), k = strlen(key);
    if (found) {
        prefix = (size_t)(value - text);
        suffix = bytes - prefix - length;
        if (prefix > capacity || n >= capacity - prefix || suffix >= capacity - prefix - n)
            return 0;
        memcpy(output, text, prefix);
        memcpy(output + prefix, replacement, n);
        memcpy(output + prefix + n, value + length, suffix);
        output[prefix + n + suffix] = 0;
    } else {
        size_t close = bytes;
        while (close && text[close - 1] != '}')
            --close;
        if (!close)
            return 0;
        --close;
        hm_json_cursor inside = {text, text + bytes};
        hm_space(&inside);
        ++inside.p;
        hm_space(&inside);
        int comma = inside.p < text + close;
        if (bytes > capacity || n > capacity - bytes || k + 8 >= capacity - bytes - n)
            return 0;
        memcpy(output, text, close);
        prefix = close;
        if (comma)
            output[prefix++] = ',';
        output[prefix++] = '\n';
        output[prefix++] = ' ';
        output[prefix++] = '"';
        memcpy(output + prefix, key, k);
        prefix += k;
        output[prefix++] = '"';
        output[prefix++] = ':';
        memcpy(output + prefix, replacement, n);
        prefix += n;
        memcpy(output + prefix, text + close, bytes - close);
        prefix += bytes - close;
        output[prefix] = 0;
    }
    return 1;
}
