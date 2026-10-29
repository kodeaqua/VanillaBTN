#include "platform/controls_options.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hm_controls_defaults(hm_controls_options *opt) {
    if (!opt)
        return;
    opt->controller_enabled = 1;
    opt->keyboard_layout = HM_KEYBOARD_LAYOUT_ARROWS;
    opt->mouse_mode = HM_MOUSE_MODE_OFF;
}

int hm_controls_change(hm_controls_options *opt, int item, int step) {
    if (!opt)
        return 0;
    switch (item) {
    case 0:
        opt->controller_enabled = !opt->controller_enabled;
        return 1;
    case 1:
        if (step > 0)
            opt->keyboard_layout = (opt->keyboard_layout + 1) % 3;
        else
            opt->keyboard_layout = (opt->keyboard_layout + 2) % 3;
        return 1;
    case 2:
        if (step > 0)
            opt->mouse_mode = (opt->mouse_mode + 1) % 3;
        else
            opt->mouse_mode = (opt->mouse_mode + 2) % 3;
        return 1;
    default:
        return 0;
    }
}

void hm_controls_row_value(const hm_controls_options *opt, int item, char *out, size_t size) {
    if (!opt || !out || size == 0)
        return;
    switch (item) {
    case 0:
        snprintf(out, size, "%s", opt->controller_enabled ? "Enabled" : "Off");
        break;
    case 1:
        switch (opt->keyboard_layout) {
        case HM_KEYBOARD_LAYOUT_ARROWS:
            snprintf(out, size, "Arrows ZX");
            break;
        case HM_KEYBOARD_LAYOUT_WASD:
            snprintf(out, size, "WASD JK");
            break;
        default:
            snprintf(out, size, "Off");
            break;
        }
        break;
    case 2:
        switch (opt->mouse_mode) {
        case HM_MOUSE_MODE_ACTION:
            snprintf(out, size, "Action");
            break;
        case HM_MOUSE_MODE_MOVE:
            snprintf(out, size, "Move-Act");
            break;
        default:
            snprintf(out, size, "Off");
            break;
        }
        break;
    default:
        snprintf(out, size, "-");
        break;
    }
}

void hm_controls_value(const hm_controls_options *opt, int item, char *out, size_t size) {
    if (!opt || !out || size == 0)
        return;
    switch (item) {
    case 0:
        if (opt->controller_enabled)
            snprintf(out, size, "Standard gamepad controller");
        else
            snprintf(out, size, "Controller/Gamepad input disabled");
        break;
    case 1:
        switch (opt->keyboard_layout) {
        case HM_KEYBOARD_LAYOUT_ARROWS:
            snprintf(out, size, "Preset A - Arrows and ZX");
            break;
        case HM_KEYBOARD_LAYOUT_WASD:
            snprintf(out, size, "Preset B - WASD and JK");
            break;
        default:
            snprintf(out, size, "Keyboard input disabled");
            break;
        }
        break;
    case 2:
        switch (opt->mouse_mode) {
        case HM_MOUSE_MODE_ACTION:
            snprintf(out, size, "Mouse Action and Tool mode");
            break;
        case HM_MOUSE_MODE_MOVE:
            snprintf(out, size, "Mouse Move and Action mode");
            break;
        default:
            snprintf(out, size, "Mouse input disabled");
            break;
        }
        break;
    default:
        out[0] = '\0';
        break;
    }
}

int hm_controls_load(hm_controls_options *opt) {
    if (!opt)
        return 0;
    hm_controls_defaults(opt);
    char *text = malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "controls", &object, &length) == 1) {
        const char *val;
        size_t val_len;
        if (hm_json_property(object, length, "controller", &val, &val_len) == 1) {
            if (val_len == 5 && !memcmp(val, "false", 5))
                opt->controller_enabled = 0;
            else if (val_len == 4 && !memcmp(val, "true", 4))
                opt->controller_enabled = 1;
        }
        if (hm_json_property(object, length, "keyboard", &val, &val_len) == 1) {
            if (val_len == 6 && !memcmp(val, "\"wasd\"", 6))
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_WASD;
            else if (val_len == 5 && !memcmp(val, "\"off\"", 5))
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_OFF;
            else
                opt->keyboard_layout = HM_KEYBOARD_LAYOUT_ARROWS;
        }
        if (hm_json_property(object, length, "mouse", &val, &val_len) == 1) {
            if (val_len == 8 && !memcmp(val, "\"action\"", 8))
                opt->mouse_mode = HM_MOUSE_MODE_ACTION;
            else if (val_len == 6 && !memcmp(val, "\"move\"", 6))
                opt->mouse_mode = HM_MOUSE_MODE_MOVE;
            else
                opt->mouse_mode = HM_MOUSE_MODE_OFF;
        }
    }
    free(text);
    return 1;
}

int hm_controls_save(const hm_controls_options *opt) {
    if (!opt)
        return 0;
    int ok = 1;
    ok &= hm_user_data_set_nested_setting("controls", "controller",
                                          opt->controller_enabled ? "true" : "false");
    const char *kb_val = (opt->keyboard_layout == HM_KEYBOARD_LAYOUT_WASD) ? "\"wasd\"" :
                         ((opt->keyboard_layout == HM_KEYBOARD_LAYOUT_OFF) ? "\"off\"" : "\"arrows\"");
    ok &= hm_user_data_set_nested_setting("controls", "keyboard", kb_val);
    const char *m_val = (opt->mouse_mode == HM_MOUSE_MODE_ACTION) ? "\"action\"" :
                        ((opt->mouse_mode == HM_MOUSE_MODE_MOVE) ? "\"move\"" : "\"off\"");
    ok &= hm_user_data_set_nested_setting("controls", "mouse", m_val);
    return ok;
}
