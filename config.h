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

#pragma once

#define DEBOUNCE 15

#define I2C_DRIVER I2CD1
#define I2C1_SCL_PIN GP23
#define I2C1_SDA_PIN GP18
#define I2C1_CLOCK_SPEED 100000

#define BACKLIGHT_PWM_DRIVER PWMD4
#define BACKLIGHT_PWM_CHANNEL RP2040_PWM_CHANNEL_B
#define BACKLIGHT_ON_STATE 0
