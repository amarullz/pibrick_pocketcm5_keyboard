# piBrick PocketCM5 Keyboard Firmware
[piBrick PocketCM5](https://github.com/amarullz/piBrick/blob/main/Pocket-CM5/) Keyboard Firmware

## Make VIAL
```
make pibrick_pocketcm5_keyboard:default
```

## How to Flash the Keyboard Firmware
- Turn off the device
- Press and hold **User Button 1** (middle-left, below the USB2 connector)
- Plug the bottom USB-C port into your PC
- The device will appear on your PC as a mass storage device labeled `RPI-RP2`
- Copy the `uf2` firmware file into `RPI-RP2`
- The keyboard will automatically restart and appear as an HID device on your PC
- You can now turn on your device

## Customize your Keys
You can customize your keys using [https://vial.rocks/](https://vial.rocks/)
- If you mess up the `VIAL` configuration and want to reset the keymaps, you can download [pibrick_pocketcm5-default-keymaps.vil](docs/pibrick_pocketcm5-default-keymaps.vil)

---
## Default Keymaps
### Layer 0
![piBrick Pocket-CM5 3D Render](docs/Layer0.png)
Layer 0 is the default layer, mainly for letter characters.
- **LGui / Super Key** is on the top panel
- Mouse drag using the **Green / Call** button, right-click using the **Red / Hangup** button
- The **Back** button is configured as a TAP-DANCE key with the following behavior:
  - Default tap: `ESC`
  - Tap and hold: Toggle trackpad mode  
    - **Blinking:** Arrow mode  
    - **Static / Default:** Mouse mode
- `TAB` is on the dollar `$` key, close to `Enter`
- Modifier keys (`ALT`, `CTRL`, `SHIFT`) use `OSM` mode. You can tap a modifier once and then press another key to apply the combination, or tap and hold the modifier simultaneously with other keys.
- Press `SYM` to move to **Layer 1**. This is a single-shot mode: it processes one character and immediately returns to Layer 0. You can also tap and hold `SYM` to stay in Layer 1.

### Layer 1
![piBrick Pocket-CM5 3D Render](docs/Layer1.png)
Layer 1 contains the main symbols and numeric characters printed on the physical keyboard.
- Press `SYM` again to go to **Layer 2**
- Press `Right Shift` to go to **Layer 3**

### Layer 2
![piBrick Pocket-CM5 3D Render](docs/Layer2.png)
Layer 2 is used for extended characters.
- Press `SYM` again to return to **Layer 0**
- Press `Right Shift` to go to **Layer 3**

### Layer 3
![piBrick Pocket-CM5 3D Render](docs/Layer3.png)
Layer 3 contains function and navigation keys.

### Layers Tips
- You can go to **Layer 2** from the default layer by double-tapping the `SYM` key.
- You can go to **Layer 3** from the default layer by tapping `SYM` and then `Right Shift`.
- Tap and hold the `BACK` button to toggle trackpad mode and use it as arrow keys. The trackpad click becomes `ENTER` when in Arrow mode.
- Double-tap and hold the `SYM` key, then rotate the rotary wheel to change the keyboard backlight.
- Double-tap `SYM`, then tap `$` to input an ampersand `&`.

---

## Hardware Pinout
**Column GPIO**
- `GPIO 8`
- `GPIO 9`
- `GPIO 10`
- `GPIO 11`
- `GPIO 12`
- `GPIO 13`

**Rows GPIO**
- `GPIO 1`
- `GPIO 2`
- `GPIO 3`
- `GPIO 4`
- `GPIO 5`
- `GPIO 6`
- `GPIO 7`

**Direct & Rotary Pins GPIO**
- `GPIO 24` - User Button 1 (Left Top)
- `GPIO 17` - User Button 2 (Left Bottom)
- `GPIO 0`  - User Button 3 (Right Top)
- `GPIO 15` - User Button 4 (Right Bottom)
- `GPIO 20` - User Button 5 (Rotary Switch)
- `GPIO 14` - BBQ20 End/Hangup Button
- `GPIO 19` - Rotary Encoder A
- `GPIO 21` - Rotary Encoder B

**Backlight & Indicators**
- `GPIO 25` - Keyboard backlight
- `GPIO 29` - Panel Backlight (Arrow mode indicator)
- `GPIO 26` - Red Indicator
- `GPIO 27` - Green Indicator
- `GPIO 28` - Blue Indicator

**Trackpad**
- `GPIO 16` - Trackpad Reset
- `GPIO 22` - Trackpad Motion
- `GPIO 23` - Trackpad I2C SCL
- `GPIO 18` - Trackpad I2C SDA
