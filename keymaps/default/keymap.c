// piBrick PocketCM5 Keyboard Firmware
// Copyright (C) 2026 amarullz.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include QMK_KEYBOARD_H
#include "i2c_master.h"

// Trackpad Constants
#ifndef TRACKPAD_ADDRESS
#    define TRACKPAD_ADDRESS 0x3B
#endif
#define TRACKPAD_WRITE ((TRACKPAD_ADDRESS << 1) )
#define TRACKPAD_READ  ((TRACKPAD_ADDRESS << 1) )
#define Trackpad_ADDR 0x3B
#define REG_PID        0x00
#define REG_REV        0x01
#define REG_MOTION     0x02
#define REG_DELTA_X    0x03
#define REG_DELTA_Y    0x04
#define REG_DELTA_XY_H 0x05
#define REG_CONFIG     0x11
#define REG_OBSERV     0x2E
#define REG_MBURST     0x42
#define BIT_MOTION_MOT (1 << 7)
#define BIT_MOTION_OVF (1 << 4)
#define trackpad_timeout 100
#define SCROLL_SPEED_DIVIDER   6

// Trackpad pins
#define pin_TP_MOTION GP22
#define pin_TP_RESET GP16

// Indicators pins
#define pin_PANEL_BLK_KBD GP29
#define pin_RGB_R GP26
#define pin_RGB_G GP27
#define pin_RGB_B GP28

// Backlight timeout in seconds
#define BACKLIGHT_TIMEOUT 5

// State variables
static int layerPos = 0;
static bool isArrowMode = false;
static bool arromEnterState = false;
static int arrowRelX = 0;
static int arrowRelY = 0;

// Backlight state variables
static uint16_t idle_timer = 0;
static uint8_t halfmin_counter = 0;
static uint8_t old_rgb_value = -1; 
static bool led_on = true;
static bool blink_state = false;
static uint16_t blink_timer = 0;
static uint16_t layer_change_timer = 0;

// Function prototypes
void userInteracted(void);
uint8_t  read_register8(uint8_t reg);

//------------------------------------- Matrix Configuration -------------------------------------//
#define DIRECT_PINS_COUNT 6
#ifndef MATRIX_INPUT_PRESSED_STATE
#    define MATRIX_INPUT_PRESSED_STATE 0
#endif

// Direct pin keys configuration
const uint8_t direct_pin_keys[DIRECT_PINS_COUNT] = {
  GP24, // Key 0
  GP17, // Key 1
  GP0,  // Key 2
  GP15, // Key 3
  GP20, // ROTARY
  GP14 // END-BUTTON
};

// Matrix definition
const pin_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;
const pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        QK_MOUSE_BUTTON_1, KC_LEFT_GUI,  QK_MOUSE_BUTTON_1, TD(0), QK_MOUSE_BUTTON_2, 
        KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,
        KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_BSPC,
        OSM(MOD_LALT), KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_TAB,  KC_ENT,
                 OSM(MOD_LSFT),          OSM(MOD_LCTL),    KC_SPC,        OSL(1),            OSM(MOD_RSFT),
        KC_BRIU,  KC_BRID,    KC_KB_VOLUME_UP,    KC_KB_VOLUME_DOWN,    QK_MOUSE_BUTTON_3, KC_NO
    ),
    [1] = LAYOUT(
        KC_TAB,          KC_LEFT_GUI,             QK_MOUSE_BUTTON_1,                KC_ESC,           KC_DEL, 
        KC_HASH, KC_1,    KC_2,    KC_3,    KC_LPRN, KC_RPRN, KC_UNDS, KC_MINS, KC_PLUS, KC_AT,
        KC_ASTR, KC_4,    KC_5,    KC_6,    KC_SLSH, KC_COLN, KC_SCLN, KC_QUOT, KC_DQT,  KC_BSPC,
        OSM(MOD_LALT), KC_7,    KC_8,    KC_9,    KC_QUES, KC_EXLM, KC_COMM, KC_DOT,  KC_DLR, KC_ENT,
                 OSM(MOD_LSFT),          KC_0,    KC_SPC,        OSL(2),            TO(3),
        KC_WHOM,  KC_MYCM,    KC_MPRV,    KC_MNXT,    KC_KB_MUTE, KC_NO
    ),
    [2] = LAYOUT(
        KC_TAB,          KC_LEFT_GUI,             QK_MOUSE_BUTTON_1,                KC_ESC,           KC_DEL, 
        KC_GRV,  KC_EXLM,     KC_AT,     KC_HASH,     KC_LCBR,  KC_RCBR, KC_UNDS, KC_MINS,  KC_EQL,  KC_PIPE,
        KC_TILD, KC_DLR,      KC_PERC,   KC_CIRC,     KC_BSLS,  KC_EQL,  KC_SCLN,  KC_GRV,   KC_CIRC,  KC_BSPC,
        OSM(MOD_LALT), KC_AMPR,     KC_ASTR,   KC_9,        KC_LBRC,  KC_RBRC, KC_LT,   KC_GT,    KC_AMPR,  KC_ENT,
                 OSM(MOD_LSFT),          OSM(MOD_LCTL),   KC_SPC,        TO(0),            TO(3),
        KC_COPY,  KC_PASTE,    KC_PRINT_SCREEN,    KC_INS,    QK_MOUSE_BUTTON_3, KC_NO
    ),
    [3] = LAYOUT(
        QK_MOUSE_BUTTON_1,          KC_LEFT_GUI,             QK_MOUSE_BUTTON_1,                KC_ESC,           QK_MOUSE_BUTTON_2, 
        KC_TAB,           KC_F1,    KC_F2,    KC_F3,    KC_F10,         KC_NO, KC_HOME, KC_INS,   KC_PGUP,  KC_DEL,
        LSFT(KC_TAB),     KC_F4,    KC_F5,    KC_F6,    KC_F11,         KC_NO, KC_END,  KC_UP,    KC_PGDN,  KC_BSPC,
        OSM(MOD_LALT),          KC_F7,    KC_F8,    KC_F9,    KC_F12,         KC_NO, KC_LEFT, KC_DOWN,  KC_RGHT,  KC_ENT,
                 OSM(MOD_LSFT),          OSM(MOD_LCTL),    KC_SPC,        TO(0),            TO(0),
        KC_BRIU,  KC_BRID,    KC_KB_VOLUME_UP,    KC_KB_VOLUME_DOWN,    QK_MOUSE_BUTTON_3, KC_NO
    )
};

// Encoder configuration
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [0] =   { ENCODER_CCW_CW(KC_MS_WH_DOWN, KC_MS_WH_UP)  },
    [1] =   { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [2] =   { ENCODER_CCW_CW(BL_DOWN, BL_UP) },
    [3] =   { ENCODER_CCW_CW(KC_MS_WH_DOWN, KC_MS_WH_UP) },
};

//----------------------------------- Custom matrix functions ------------------------------------------//
static inline void gpio_atomic_set_pin_output_low(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_output(pin);
        gpio_write_pin_low(pin);
    }
}
static inline void gpio_atomic_set_pin_output_high(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_output(pin);
        gpio_write_pin_high(pin);
    }
}
static inline void gpio_atomic_set_pin_input_high(pin_t pin) {
    ATOMIC_BLOCK_FORCEON {
        gpio_set_pin_input_high(pin);
    }
}
static inline uint8_t readMatrixPin(pin_t pin) {
    if (pin != NO_PIN) {
        return (gpio_read_pin(pin) == MATRIX_INPUT_PRESSED_STATE) ? 0 : 1;
    } else {
        return 1;
    }
}
static bool select_row(uint8_t row) {
    pin_t pin = row_pins[row];
    if (pin != NO_PIN) {
        gpio_atomic_set_pin_output_low(pin);
        return true;
    }
    return false;
}
static void unselect_row(uint8_t row) {
    pin_t pin = row_pins[row];
    if (pin != NO_PIN) {
#            ifdef MATRIX_UNSELECT_DRIVE_HIGH
        gpio_atomic_set_pin_output_high(pin);
#            else
        gpio_atomic_set_pin_input_high(pin);
#            endif
    }
}
void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {
    // Start with a clear matrix row
    matrix_row_t current_row_value = 0;
    matrix_row_t row_shifter = MATRIX_ROW_SHIFTER;
    if (!select_row(current_row)) {
      matrix_output_select_delay();
      for (uint8_t col_index = 0; col_index < DIRECT_PINS_COUNT; col_index++, row_shifter <<= 1) {
          uint8_t pin_state = readPin(direct_pin_keys[col_index]);
          current_row_value |= pin_state ? 0 : row_shifter;
      }
    }
    else{
      matrix_output_select_delay();
      for (uint8_t col_index = 0; col_index < MATRIX_COLS; col_index++, row_shifter <<= 1) {
        uint8_t pin_state = readMatrixPin(col_pins[col_index]);
        if (col_index==0 && current_row==0 && isArrowMode){
          if (!pin_state && !arromEnterState){
            register_code(KC_ENT);
            arromEnterState=true;
          }
          else if (pin_state){
            if (arromEnterState){
              unregister_code(KC_ENT);
              arromEnterState=false;
            }
          }
        }
        else{
          current_row_value |= pin_state ? 0 : row_shifter;
        }
      }
    }
    // Unselect row
    unselect_row(current_row);
    matrix_output_unselect_delay(current_row, current_row_value != 0); // wait for all Col signals to go HIGH
    // Update the matrix
    current_matrix[current_row] = current_row_value;
}

//----------------------------------- Custom user functions ------------------------------------------//

// Arrow mode setter
void arrowSetMode(bool arrowMode){
  arrowRelX=arrowRelY=0;
  isArrowMode=arrowMode;
  if (arromEnterState){
    unregister_code(KC_ENT);
    arromEnterState=false;
  }
  arromEnterState=false;
}

// RGB Indicator setter
void setRgbVal(uint8_t r, uint8_t g, uint8_t b){
  if (r)
    writePinLow(pin_RGB_R);
  else
    writePinHigh(pin_RGB_R);
  
  if (g)
    writePinLow(pin_RGB_G);
  else
    writePinHigh(pin_RGB_G);
  
  if (b)
    writePinLow(pin_RGB_B);
  else
    writePinHigh(pin_RGB_B);
}

// Backlight setter
void backlightSetState(bool on){
  if (on){
    if (!led_on){
        if (old_rgb_value == -1) old_rgb_value = 1;
        writePinLow(pin_PANEL_BLK_KBD);
        old_rgb_value = 1;
        backlight_enable();
        led_on = true;
    }
  }
  else{
    if (led_on){
      if (!isArrowMode){
        writePinHigh(pin_PANEL_BLK_KBD);
      }
      backlight_disable();
      led_on = false; 
      halfmin_counter = 0;
    }
  }
}

// Matrix scan user function
void matrix_scan_user(void) {
  if (isArrowMode){
    if (timer_elapsed(blink_timer) > 200) { 
      if (blink_state)
        writePinHigh(pin_PANEL_BLK_KBD);
      else
        writePinLow(pin_PANEL_BLK_KBD);
      blink_timer = timer_read();
      blink_state=!blink_state;
    }
  }
  if (idle_timer == 0) idle_timer = timer_read();
  if (led_on && timer_elapsed(idle_timer) > 500) {
      halfmin_counter++;
      idle_timer = timer_read();
  }
  if (led_on && halfmin_counter >= BACKLIGHT_TIMEOUT * 2){
      backlightSetState(false);
  }
};

// User interaction handler
void userInteracted(void){
  if (led_on == false){
    backlightSetState(true);
  }
  idle_timer = timer_read();
  halfmin_counter = 0;
}

//----------------------------------- Trackpad functions ------------------------------------------//
uint8_t read_register8(uint8_t reg) {
    uint8_t val;
    i2c_write_register(TRACKPAD_WRITE, reg, NULL, 0, trackpad_timeout);
    i2c_read_register(TRACKPAD_READ, reg, &val, 1, trackpad_timeout);
    return val;
}

bool pointing_device_task(void) {
    const uint8_t ifmotion = read_register8(REG_MOTION);
    
    if ( ifmotion & BIT_MOTION_MOT) {
        int8_t x =  read_register8(REG_DELTA_X);
        int8_t y =  read_register8(REG_DELTA_Y);
        report_mouse_t currentReport = pointing_device_get_report();

        if (isArrowMode){
          arrowRelX -= x;
          arrowRelY += y;
          
          bool ismoving = false;
          if (arrowRelX<-40){
            tap_code(KC_LEFT);
            ismoving = true;
          }
          else if (arrowRelX>40){
            tap_code(KC_RGHT);
            ismoving = true;
          }
          if (arrowRelY<-40){
            tap_code(KC_UP);
            ismoving = true;
          }
          else if (arrowRelY>40){
            tap_code(KC_DOWN);
            ismoving = true;
          }
          if (ismoving){
            arrowRelX=0;
            arrowRelY=0;
          }
          return false;
        }
        else
        {
           x = ((x < 127) ? x : (x - 256));
           y = ((y < 127) ? y : (y - 256));
           currentReport.x= 0-x;
           currentReport.y= y;
        }
        pointing_device_set_report(currentReport);
    }
    return pointing_device_send();
}

//----------------------------------- QMK required functions ------------------------------------------//

// Key processing function
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    userInteracted();
  }
  return true;
}

// Board initialization function
void board_init(void){
  // Init indicator pins
  setPinOutput(pin_PANEL_BLK_KBD);
  setPinOutput(pin_RGB_R);
  setPinOutput(pin_RGB_G);
  setPinOutput(pin_RGB_B);
  writePinLow(pin_PANEL_BLK_KBD);
  
  // Turn off RGB indicators
  setRgbVal(0,0,0);

  // Init direct pins
  for (uint8_t i=0;i<DIRECT_PINS_COUNT;i++){
    setPinInputHigh(direct_pin_keys[i]);
  }

  // Turn on backlight
  backlightSetState(true);
}

// Pointing device initialization function
void pointing_device_init(void) {
    setPinOutput(pin_TP_RESET);
    writePinLow(pin_TP_RESET);
    wait_ms(200);
    writePinHigh(pin_TP_RESET);
    wait_ms(200);

    i2c_init();
    wait_ms(100);

    // setPinOutput(pin_TP_SHUTDOWN);
    // writePinLow(pin_TP_SHUTDOWN);
    setPinInputHigh(pin_TP_MOTION);
}

// Keyboard post-initialization function
void keyboard_post_init_user(void) {
  vial_tap_dance_entry_t tapdance_alt = { KC_ESC,
                                   TO(0),
                                   KC_NO,
                                   KC_NO,
                                   400 };
  dynamic_keymap_set_tap_dance(0, &tapdance_alt);
  layer_change_timer = timer_read();
}

// Layer state change handler
layer_state_t layer_state_set_user(layer_state_t state) {
  int oldLayer=layerPos;
  layerPos=get_highest_layer(state);
  switch (layerPos) {
    case 0:
        setRgbVal(0,0,0);
        if (oldLayer==0){
          if (timer_elapsed(layer_change_timer) > 300) { 
            if (!isArrowMode){
              arrowSetMode(true);
            }
            else{
              arrowSetMode(false);
              writePinLow(pin_PANEL_BLK_KBD);
              blink_state=true;
            }
          }
        }
        break;
    case 1:
        setRgbVal(0,1,0);
        break;
    case 2:
        setRgbVal(1,0,0);
        break;
    case 3:
        setRgbVal(0,0,1);
        break;
  }
  userInteracted();
  layer_change_timer = timer_read();
  return state;
}

