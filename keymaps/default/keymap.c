// piBrick PocketCM5 Keyboard Firmware
// Copyright (C) 2026 amarullz.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include QMK_KEYBOARD_H

#include "eeprom.h"
#include "hal.h"
#include "i2c_master.h"
#include "quantum.h"
#include "raw_hid.h"
#include "timer.h"


// ============================================================================
// Configuration
// ============================================================================

#define TRACKPAD_ADDRESS        0x3B
#define TRACKPAD_WRITE          (TRACKPAD_ADDRESS << 1)
#define TRACKPAD_READ           (TRACKPAD_ADDRESS << 1)

#define TRACKPAD_TIMEOUT        100
#define SCROLL_SPEED_DIVIDER    6

#define PIN_TRACKPAD_MOTION     GP22
#define PIN_TRACKPAD_RESET      GP16

#define DIRECT_PINS_COUNT       6

#ifndef MATRIX_INPUT_PRESSED_STATE
#    define MATRIX_INPUT_PRESSED_STATE 0
#endif

#define BACKLIGHT_LEVEL_COUNT   9
#define BACKLIGHT_MAX_LEVEL     8
#define BACKLIGHT_TIMEOUT_DEFAULT 5

// RAW HID commands
#define PIBRICK_CMD             0xFF

#define PIBRICK_CMD_TIMEOUT     0x01
#define PIBRICK_CMD_BACKLIGHT   0x02
#define PIBRICK_CMD_RGB         0x03

#define PIBRICK_GET             0x00
#define PIBRICK_SET             0x01

#define PIBRICK_STATUS_OK       0x00
#define PIBRICK_STATUS_ERROR    0x01


// ============================================================================
// Trackpad registers
// ============================================================================

#define REG_MOTION              0x02
#define REG_DELTA_X             0x03
#define REG_DELTA_Y             0x04

#define BIT_MOTION_MOT          (1 << 7)


// ============================================================================
// Persistent user configuration
// ============================================================================
//
// Keep the structure layout compatible with the existing EEPROM data.

typedef union {
    uint32_t value;

    struct {
        uint8_t backlight;
        uint8_t bl_timeout;
        uint8_t b2;
        uint8_t initialized;
    };
} user_config_t;

static user_config_t user_config;


// ============================================================================
// Matrix configuration
// ============================================================================

const uint8_t direct_pin_keys[DIRECT_PINS_COUNT] = {
    GP24, // Key 0
    GP17, // Key 1
    GP0,  // Key 2
    GP15, // Key 3
    GP20, // Rotary
    GP14  // End button
};

const pin_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;
const pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;


// ============================================================================
// Keymap
// ============================================================================

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        QK_MOUSE_BUTTON_1, KC_LEFT_GUI, QK_MOUSE_BUTTON_1, TD(0), QK_MOUSE_BUTTON_2,
        KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P,
        KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_BSPC,
        OSM(MOD_LALT), KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_TAB, KC_ENT,
        OSM(MOD_LSFT), OSM(MOD_LCTL), KC_SPC, OSL(1), OSM(MOD_RSFT),
        KC_BRIU, KC_BRID, KC_KB_VOLUME_UP, KC_KB_VOLUME_DOWN,
        QK_MOUSE_BUTTON_3, KC_NO
    ),

    [1] = LAYOUT(
        KC_TAB, KC_LEFT_GUI, QK_MOUSE_BUTTON_1, KC_ESC, KC_DEL,
        KC_HASH, KC_1, KC_2, KC_3, KC_LPRN, KC_RPRN, KC_UNDS, KC_MINS, KC_PLUS, KC_AT,
        KC_ASTR, KC_4, KC_5, KC_6, KC_SLSH, KC_COLN, KC_SCLN, KC_QUOT, KC_DQT, KC_BSPC,
        OSM(MOD_LALT), KC_7, KC_8, KC_9, KC_QUES, KC_EXLM, KC_COMM, KC_DOT, KC_DLR, KC_ENT,
        OSM(MOD_LSFT), KC_0, KC_SPC, OSL(2), TO(3),
        BL_UP, BL_DOWN, KC_MPRV, KC_MNXT, KC_KB_MUTE, KC_NO
    ),

    [2] = LAYOUT(
        KC_TAB, KC_LEFT_GUI, QK_MOUSE_BUTTON_1, KC_ESC, KC_DEL,
        KC_GRV, KC_EXLM, KC_AT, KC_HASH, KC_LCBR, KC_RCBR, KC_UNDS, KC_MINS, KC_EQL, KC_PIPE,
        KC_TILD, KC_DLR, KC_PERC, KC_CIRC, KC_BSLS, KC_EQL, KC_SCLN, KC_GRV, KC_CIRC, KC_BSPC,
        OSM(MOD_LALT), KC_AMPR, KC_ASTR, KC_9, KC_LBRC, KC_RBRC, KC_LT, KC_GT, KC_AMPR, KC_ENT,
        OSM(MOD_LSFT), OSM(MOD_LCTL), KC_SPC, TO(0), TO(3),
        KC_PASTE /*QK_BOOTLOADER*/, KC_PASTE, KC_PRINT_SCREEN, KC_INS,
        QK_MOUSE_BUTTON_3, KC_NO
    ),

    [3] = LAYOUT(
        QK_MOUSE_BUTTON_1, KC_LEFT_GUI, QK_MOUSE_BUTTON_1, KC_ESC, QK_MOUSE_BUTTON_2,
        KC_TAB, KC_F1, KC_F2, KC_F3, KC_F10, KC_NO, KC_HOME, KC_INS, KC_PGUP, KC_DEL,
        LSFT(KC_TAB), KC_F4, KC_F5, KC_F6, KC_F11, KC_NO, KC_END, KC_UP, KC_PGDN, KC_BSPC,
        OSM(MOD_LALT), KC_F7, KC_F8, KC_F9, KC_F12, KC_NO, KC_LEFT, KC_DOWN, KC_RGHT, KC_ENT,
        OSM(MOD_LSFT), OSM(MOD_LCTL), KC_SPC, TO(0), TO(0),
        KC_BRIU, KC_BRID, KC_KB_VOLUME_UP, KC_KB_VOLUME_DOWN,
        QK_MOUSE_BUTTON_3, KC_NO
    )
};


// ============================================================================
// Encoder
// ============================================================================

const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [0] = { ENCODER_CCW_CW(KC_MS_WH_DOWN, KC_MS_WH_UP) },
    [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [2] = { ENCODER_CCW_CW(BL_DOWN, BL_UP) },
    [3] = { ENCODER_CCW_CW(KC_MS_WH_DOWN, KC_MS_WH_UP) },
};


// ============================================================================
// Runtime state
// ============================================================================

static bool is_initialized = false;
static bool arrow_mode = false;
static bool arrow_enter_pressed = false;
static bool keyboard_led_on = false;
static bool blink_state = false;

static int16_t arrow_rel_x = 0;
static int16_t arrow_rel_y = 0;

static uint8_t saved_backlight_level = 1;

static uint16_t blink_timer = 0;
static uint16_t layer_change_timer = 0;
static uint16_t idle_timer = 0;
static uint16_t idle_half_seconds = 0;


// ============================================================================
// RGB state
// ============================================================================

static uint8_t rgb_r = 0;
static uint8_t rgb_g = 0;
static uint8_t rgb_b = 0;

static bool rgb_timeout_active = false;
static uint32_t rgb_timeout_start = 0;
static uint32_t rgb_timeout_duration = 0;


// ============================================================================
// Panel PWM
// ============================================================================

static const PWMConfig pwm_config = {
    .frequency = 62500000,
    .period = 62500,
    .callback = NULL,
    .channels = {
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_ACTIVE_LOW, NULL}
    }
};

/*
 * Non-linear brightness curve.
 *
 * The panel LED is not visually linear, so low levels are intentionally
 * compressed while the upper levels are spread out.
 */
static const uint16_t panel_light_levels[BACKLIGHT_LEVEL_COUNT] = {
    0,       // 0  = OFF
    625,     // 1  = 1%
    1875,    // 2  = 3%
    3750,    // 3  = 6%
    7500,    // 4  = 12%
    15625,   // 5  = 25%
    28125,   // 6  = 45%
    43750,   // 7  = 70%
    62500    // 8  = 100%
};


// ============================================================================
// GPIO helpers
// ============================================================================

static inline void gpio_output_low(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_output(pin);
        gpio_write_pin_low(pin);
    }
}

static inline void gpio_output_high(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_output(pin);
        gpio_write_pin_high(pin);
    }
}

static inline void gpio_input_high(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_input_high(pin);
    }
}

static inline uint8_t read_matrix_pin(pin_t pin) {
    if (pin == NO_PIN) {
        return 1;
    }

    return gpio_read_pin(pin) == MATRIX_INPUT_PRESSED_STATE ? 0 : 1;
}


// ============================================================================
// Panel / RGB control
// ============================================================================

static uint8_t clamp_backlight_level(uint8_t level) {
    return level > BACKLIGHT_MAX_LEVEL ? BACKLIGHT_MAX_LEVEL : level;
}

static void panel_set_level(uint8_t level) {
    level = clamp_backlight_level(level);
    pwmEnableChannel(&PWMD6, 1, panel_light_levels[level]);
}

static void rgb_set_pwm(uint16_t r, uint16_t g, uint16_t b) {
    const uint16_t PWM_MAX = 62500;

    pwmEnableChannel(&PWMD5, 0, PWM_MAX - r);
    pwmEnableChannel(&PWMD5, 1, g);
    pwmEnableChannel(&PWMD6, 0, PWM_MAX - b);
}

static void rgb_apply(uint8_t r, uint8_t g, uint8_t b) {
    const uint16_t PWM_MAX = 62500;

    uint16_t rpwm = ((uint32_t)r * PWM_MAX) / 255;
    uint16_t gpwm = (((uint32_t)g * PWM_MAX) / 255 * 40) / 100;
    uint16_t bpwm = (((uint32_t)b * PWM_MAX) / 255 * 70) / 100;

    rgb_set_pwm(rpwm, gpwm, bpwm);
}

static void rgb_apply_brightness(uint16_t brightness) {
    if (brightness == 0) {
        brightness = 400;
    }

    rgb_set_pwm(
        rgb_r ? brightness : 0,
        rgb_g ? (brightness * 40) / 100 : 0,
        rgb_b ? (brightness * 70) / 100 : 0
    );
}

static void rgb_restore(void) {
    uint8_t level = get_backlight_level();

    if (level == 0) {
        level = saved_backlight_level;
    }

    level = clamp_backlight_level(level);

    rgb_apply_brightness(panel_light_levels[level]);
}

static void rgb_set(uint8_t r, uint8_t g, uint8_t b) {
    rgb_r = r;
    rgb_g = g;
    rgb_b = b;

    rgb_apply_brightness(panel_light_levels[
        clamp_backlight_level(get_backlight_level())
    ]);
}

static void rgb_reset(void) {
    rgb_timeout_active = false;
    rgb_apply_brightness(panel_light_levels[
        clamp_backlight_level(
            get_backlight_level() ? get_backlight_level() : saved_backlight_level
        )
    ]);
}

static void panel_set_enabled(bool enabled) {
    if (!enabled) {
        panel_set_level(0);
        return;
    }

    uint8_t level = get_backlight_level();

    if (level == 0) {
        level = saved_backlight_level;
    }

    panel_set_level(level);
}


// ============================================================================
// Light PWM initialization
// ============================================================================

static void light_pwm_init(void) {
    // Panel backlight - GP29
    palSetPadMode(
        0U,
        29,
        PAL_MODE_ALTERNATE_PWM |
        PAL_RP_PAD_DRIVE12 |
        PAL_RP_GPIO_OE
    );

    // RGB red - GP26
    palSetPadMode(
        0U,
        26,
        PAL_MODE_ALTERNATE_PWM |
        PAL_RP_PAD_DRIVE12 |
        PAL_RP_GPIO_OE
    );

    // RGB green - GP27
    palSetPadMode(
        0U,
        27,
        PAL_MODE_ALTERNATE_PWM |
        PAL_RP_PAD_DRIVE12 |
        PAL_RP_GPIO_OE
    );

    // RGB blue - GP28
    palSetPadMode(
        0U,
        28,
        PAL_MODE_ALTERNATE_PWM |
        PAL_RP_PAD_DRIVE12 |
        PAL_RP_GPIO_OE
    );

    pwmStart(&PWMD6, &pwm_config);
    pwmStart(&PWMD5, &pwm_config);

    // RGB initially OFF
    rgb_set_pwm(0, 0, 0);
}


// ============================================================================
// Backlight configuration
// ============================================================================

static void backlight_config_init(void) {
    user_config.value = eeconfig_read_user();

    uint8_t current_level = get_backlight_level();

    if (user_config.initialized != 0xF3) {
        user_config.backlight = current_level;
        user_config.bl_timeout = BACKLIGHT_TIMEOUT_DEFAULT;
        user_config.b2 = 0;
        user_config.initialized = 0xF3;

        eeconfig_update_user(user_config.value);
    }

    if (user_config.bl_timeout == 0) {
        user_config.bl_timeout = BACKLIGHT_TIMEOUT_DEFAULT;
        eeconfig_update_user(user_config.value);
    }

    saved_backlight_level = user_config.backlight;

    if (saved_backlight_level == 0) {
        saved_backlight_level = current_level;
    }

    saved_backlight_level = clamp_backlight_level(saved_backlight_level);

    if (current_level != saved_backlight_level) {
        backlight_set(saved_backlight_level);
    }
}


// ============================================================================
// Backlight state
// ============================================================================

static void backlight_set_state(bool enabled) {
    if (enabled) {
        if (keyboard_led_on) {
            return;
        }

        backlight_set(saved_backlight_level);
        panel_set_enabled(true);

        keyboard_led_on = true;
        return;
    }

    if (!keyboard_led_on) {
        return;
    }

    keyboard_led_on = false;

    uint8_t level = clamp_backlight_level(get_backlight_level());

    if (level != 0) {
        saved_backlight_level = level;

        if (user_config.backlight != saved_backlight_level) {
            user_config.backlight = saved_backlight_level;
            eeconfig_update_user(user_config.value);
        }
    }

    backlight_set(0);

    idle_half_seconds = 0;

    if (!arrow_mode) {
        panel_set_level(0);
    }
}


// ============================================================================
// User interaction
// ============================================================================

static void user_interacted(void) {
    if (!is_initialized) {
        return;
    }

    if (!keyboard_led_on) {
        backlight_set_state(true);
    }

    idle_timer = timer_read();
    idle_half_seconds = 0;
}


// ============================================================================
// Arrow mode
// ============================================================================

static void arrow_set_mode(bool enabled) {
    arrow_rel_x = 0;
    arrow_rel_y = 0;
    arrow_mode = enabled;

    if (arrow_enter_pressed) {
        unregister_code(KC_ENT);
        arrow_enter_pressed = false;
    }
}


// ============================================================================
// Matrix
// ============================================================================

static bool matrix_select_row(uint8_t row) {
    pin_t pin = row_pins[row];

    if (pin == NO_PIN) {
        return false;
    }

    gpio_output_low(pin);
    return true;
}

static void matrix_unselect_row(uint8_t row) {
    pin_t pin = row_pins[row];

    if (pin == NO_PIN) {
        return;
    }

#ifdef MATRIX_UNSELECT_DRIVE_HIGH
    gpio_output_high(pin);
#else
    gpio_input_high(pin);
#endif
}

void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {
    matrix_row_t row_value = 0;
    matrix_row_t row_shifter = MATRIX_ROW_SHIFTER;

    if (!matrix_select_row(current_row)) {
        matrix_output_select_delay();

        for (uint8_t i = 0; i < DIRECT_PINS_COUNT; i++, row_shifter <<= 1) {
            if (!readPin(direct_pin_keys[i])) {
                row_value |= row_shifter;
            }
        }
    } else {
        matrix_output_select_delay();

        for (uint8_t i = 0; i < MATRIX_COLS; i++, row_shifter <<= 1) {
            uint8_t pressed = read_matrix_pin(col_pins[i]);

            if (i == 0 && current_row == 0 && arrow_mode) {
                if (!pressed && !arrow_enter_pressed) {
                    register_code(KC_ENT);
                    arrow_enter_pressed = true;
                } else if (pressed && arrow_enter_pressed) {
                    unregister_code(KC_ENT);
                    arrow_enter_pressed = false;
                }
            } else if (!pressed) {
                row_value |= row_shifter;
            }
        }
    }

    matrix_unselect_row(current_row);

    matrix_output_unselect_delay(
        current_row,
        row_value != 0
    );

    current_matrix[current_row] = row_value;
}


// ============================================================================
// Trackpad
// ============================================================================

static uint8_t trackpad_read_register(uint8_t reg) {
    uint8_t value;

    i2c_write_register(
        TRACKPAD_WRITE,
        reg,
        NULL,
        0,
        TRACKPAD_TIMEOUT
    );

    i2c_read_register(
        TRACKPAD_READ,
        reg,
        &value,
        1,
        TRACKPAD_TIMEOUT
    );

    return value;
}

bool pointing_device_task(void) {
    uint8_t motion = trackpad_read_register(REG_MOTION);

    if (!(motion & BIT_MOTION_MOT)) {
        return pointing_device_send();
    }

    int8_t x = trackpad_read_register(REG_DELTA_X);
    int8_t y = trackpad_read_register(REG_DELTA_Y);

    if (arrow_mode) {
        arrow_rel_x -= x;
        arrow_rel_y += y;

        bool moved = false;

        if (arrow_rel_x < -40) {
            tap_code(KC_LEFT);
            moved = true;
        } else if (arrow_rel_x > 40) {
            tap_code(KC_RGHT);
            moved = true;
        }

        if (arrow_rel_y < -40) {
            tap_code(KC_UP);
            moved = true;
        } else if (arrow_rel_y > 40) {
            tap_code(KC_DOWN);
            moved = true;
        }

        if (moved) {
            arrow_rel_x = 0;
            arrow_rel_y = 0;
        }

        return false;
    }

    report_mouse_t report = pointing_device_get_report();

    report.x = -x;
    report.y = y;

    pointing_device_set_report(report);

    return pointing_device_send();
}


// ============================================================================
// Periodic light handling
// ============================================================================

void matrix_scan_user(void) {
    static uint8_t last_level = 255;

    if (!is_initialized) {
        return;
    }

    // Arrow mode panel blinking
    if (arrow_mode) {
        if (timer_elapsed(blink_timer) > 200) {
            panel_set_enabled(!blink_state);

            blink_state = !blink_state;
            blink_timer = timer_read();
        }
    }

    // Update brightness when QMK backlight changes
    if (keyboard_led_on) {
        uint8_t level = clamp_backlight_level(get_backlight_level());

        if (blink_state || level != last_level) {
            blink_state = false;
            last_level = level;

            if (level != user_config.backlight) {
                saved_backlight_level = level;
                user_config.backlight = level;
                eeconfig_update_user(user_config.value);
            }

            panel_set_level(level);
            rgb_apply_brightness(panel_light_levels[level]);
        }
    }

    // Idle timer
    if (idle_timer == 0) {
        idle_timer = timer_read();
    }

    if (keyboard_led_on && timer_elapsed(idle_timer) > 500) {
        idle_half_seconds++;
        idle_timer = timer_read();
    }

    if (
        keyboard_led_on &&
        idle_half_seconds >= (uint16_t)user_config.bl_timeout * 2
    ) {
        backlight_set_state(false);
    }
}


// ============================================================================
// RGB timeout
// ============================================================================

void housekeeping_task_user(void) {
    if (
        rgb_timeout_active &&
        timer_elapsed32(rgb_timeout_start) >= rgb_timeout_duration
    ) {
        rgb_timeout_active = false;
        rgb_restore();
    }
}


// ============================================================================
// RAW HID
// ============================================================================

static void raw_hid_reply(
    uint8_t *data,
    uint8_t status
) {
    data[0] = PIBRICK_CMD;
    data[2] = status;
}

static void raw_hid_handle_timeout(uint8_t *data, uint8_t length) {
    if (length < 3) {
        return;
    }

    if (data[2] == PIBRICK_GET) {
        raw_hid_reply(data, PIBRICK_STATUS_OK);
        data[3] = user_config.bl_timeout;
        return;
    }

    if (data[2] == PIBRICK_SET && length >= 4) {
        user_config.bl_timeout = data[3];

        if (user_config.bl_timeout == 0) {
            user_config.bl_timeout = BACKLIGHT_TIMEOUT_DEFAULT;
        }

        eeconfig_update_user(user_config.value);

        raw_hid_reply(data, PIBRICK_STATUS_OK);
        data[3] = user_config.bl_timeout;
        return;
    }

    raw_hid_reply(data, PIBRICK_STATUS_ERROR);
}

static void raw_hid_handle_backlight(uint8_t *data, uint8_t length) {
    if (length < 3) {
        return;
    }

    if (data[2] == PIBRICK_GET) {
        raw_hid_reply(data, PIBRICK_STATUS_OK);
        data[3] = user_config.backlight;
        return;
    }

    if (data[2] == PIBRICK_SET && length >= 4) {
        uint8_t level = data[3];

        if (level > BACKLIGHT_MAX_LEVEL) {
            raw_hid_reply(data, PIBRICK_STATUS_ERROR);
            return;
        }

        saved_backlight_level = level;
        user_config.backlight = level;

        eeconfig_update_user(user_config.value);

        backlight_set(level);
        panel_set_level(level);
        keyboard_led_on = true;

        raw_hid_reply(data, PIBRICK_STATUS_OK);
        data[3] = user_config.backlight;
        return;
    }

    raw_hid_reply(data, PIBRICK_STATUS_ERROR);
}

static void raw_hid_handle_rgb(uint8_t *data, uint8_t length) {
    /*
     * data[0] = command
     * data[1] = RGB command
     * data[2] = R
     * data[3] = G
     * data[4] = B
     * data[5] = duration low byte
     * data[6] = duration high byte
     */
    if (length < 5) {
        return;
    }

    uint8_t r = data[2];
    uint8_t g = data[3];
    uint8_t b = data[4];

    uint16_t duration = 0;

    if (length >= 7) {
        duration =
            ((uint16_t)data[6] << 8) |
            data[5];
    }

    if (r == 0 && g == 0 && b == 0) {
        rgb_timeout_active = false;
        rgb_reset();
    } else {
        rgb_r = r;
        rgb_g = g;
        rgb_b = b;

        rgb_apply(r, g, b);

        if (duration > 0) {
            rgb_timeout_duration = duration;
            rgb_timeout_start = timer_read32();
            rgb_timeout_active = true;
        } else {
            rgb_timeout_active = false;
        }
    }

    raw_hid_reply(data, PIBRICK_STATUS_OK);

    data[3] = r;
    data[4] = g;
    data[5] = b;
}

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (length < 2 || data[0] != PIBRICK_CMD) {
        return;
    }

    switch (data[1]) {
        case PIBRICK_CMD_TIMEOUT:
            raw_hid_handle_timeout(data, length);
            break;

        case PIBRICK_CMD_BACKLIGHT:
            raw_hid_handle_backlight(data, length);
            break;

        case PIBRICK_CMD_RGB:
            raw_hid_handle_rgb(data, length);
            break;

        default:
            raw_hid_reply(data, PIBRICK_STATUS_ERROR);
            break;
    }
}


// ============================================================================
// QMK callbacks
// ============================================================================

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        user_interacted();
    }

    return true;
}

void board_init(void) {
    for (uint8_t i = 0; i < DIRECT_PINS_COUNT; i++) {
        setPinInputHigh(direct_pin_keys[i]);
    }

    is_initialized = false;
}

void pointing_device_init(void) {
    setPinOutput(PIN_TRACKPAD_RESET);

    writePinLow(PIN_TRACKPAD_RESET);
    wait_ms(200);

    writePinHigh(PIN_TRACKPAD_RESET);
    wait_ms(200);

    i2c_init();
    wait_ms(100);

    setPinInputHigh(PIN_TRACKPAD_MOTION);
}

void keyboard_post_init_user(void) {
    vial_tap_dance_entry_t tapdance_alt = {
        KC_ESC,
        TO(0),
        KC_NO,
        KC_NO,
        400
    };

    dynamic_keymap_set_tap_dance(0, &tapdance_alt);

    layer_change_timer = timer_read();

    keyboard_led_on = false;

    backlight_config_init();
    light_pwm_init();

    is_initialized = true;

    rgb_set(0, 0, 0);
    backlight_set_state(true);

    idle_timer = 0;
    idle_half_seconds = 0;

    user_interacted();
}

void suspend_wakeup_init_user(void) {
    keyboard_led_on = false;

    backlight_set_state(true);
    panel_set_enabled(true);

    idle_timer = 0;
    idle_half_seconds = 0;

    user_interacted();
}

void suspend_power_down_user(void) {
    arrow_set_mode(false);

    rgb_set(0, 0, 0);

    backlight_set_state(false);
    panel_set_level(0);

    idle_timer = 0;
    idle_half_seconds = 0;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    static uint8_t previous_layer = 0;

    uint8_t layer = get_highest_layer(state);

    switch (layer) {
        case 0:
            rgb_set(0, 0, 0);

            if (previous_layer == 0) {
                if (timer_elapsed(layer_change_timer) > 300) {
                    if (!arrow_mode) {
                        arrow_set_mode(true);
                    } else {
                        arrow_set_mode(false);
                        blink_state = true;
                        panel_set_enabled(true);
                    }
                }
            }
            break;

        case 1:
            rgb_set(0, 1, 0);
            break;

        case 2:
            rgb_set(1, 0, 0);
            break;

        case 3:
            rgb_set(0, 0, 1);
            break;

        case 4:
            rgb_set(0, 1, 1);
            break;

        case 5:
            rgb_set(1, 1, 0);
            break;

        case 6:
            rgb_set(1, 0, 1);
            break;

        case 7:
            rgb_set(1, 1, 1);
            break;
    }

    previous_layer = layer;

    user_interacted();
    layer_change_timer = timer_read();

    return state;
}
