# CryoBelt Rev-A Arduino Firmware

Initial firmware skeleton for the custom CryoBelt ESP32-S3-MINI-1-N8 PCB.

## What works in this skeleton

- Native USB serial logging
- I2C on GPIO9/GPIO8
- BQ25895 detection and safe boot configuration
- ESP-authorised charging
- SHT40 temperature/humidity readings
- Fan PWM on GPIO13
- INA180A3 fan-current reading on GPIO14
- Three front buttons
- Three SK6805 addressable LEDs
- FUSB303B GPIO-mode role/current-state reading
- 12V boost enable control
- OTG gate defaults safely OFF

## Library required

Install through Arduino IDE Library Manager:

- **Adafruit NeoPixel**

Everything else uses the Arduino-ESP32 core.

## Arduino IDE setup

1. Install the Espressif **esp32** board package.
2. Select an ESP32-S3 board profile suitable for an ESP32-S3-MINI-1-N8.
   `ESP32S3 Dev Module` is fine for initial bring-up.
3. Enable USB CDC on boot if that option is shown.
4. Flash size: 8 MB.
5. Connect the CryoBelt USB-C port.
6. For the first flash, if necessary:
   - hold BOOT
   - tap RESET
   - release BOOT
   - choose the newly appearing serial port
7. Open `CryoBelt_Firmware.ino` and upload.

## Pin map from the supplied CryoBelt KiCad netlist

| Function | GPIO |
|---|---:|
| I2C SCL | 8 |
| I2C SDA | 9 |
| BQ25895 INT_N | 11 |
| 12V boost enable | 12 |
| Fan PWM / gate | 13 |
| Fan current ADC | 14 |
| Audio BCLK | 16 |
| Audio LRCLK | 17 |
| Audio DIN | 18 |
| RGB DIN | 26 |
| FUSB303 OUT1 | 36 |
| FUSB303 OUT2 | 37 |
| Up | 38 |
| User | 39 |
| Down | 40 |
| FUSB303 ID | 41 |
| External OTG gate | 42 |
| USB D- | 19 |
| USB D+ | 20 |

## Charging behaviour

The hardware intentionally leaves the BQ25895 ILIM pin open.

At power-up, EN_ILIM defaults enabled, so an open ILIM hardware pin prevents normal input current. Firmware then:

1. talks to the BQ25895;
2. explicitly disables charging;
3. explicitly disables BQ OTG;
4. sets the desired I2C input-current limit;
5. disables ILIM-pin limiting (`EN_ILIM = 0`);
6. only then enables charging.

`CHARGE_INPUT_LIMIT_MA` and `ALLOW_CHARGING_AFTER_BOOT` are in `config.h`.

For first bench bring-up, it is sensible to set:

```cpp
constexpr bool ALLOW_CHARGING_AFTER_BOOT = false;
```

until the charger section has been inspected and tested.

## IMPORTANT Rev-A net conflict found in supplied netlist

The supplied PCB/netlist connects:

- ESP32 GPIO18
- MAX98357A `DIN`
- **D3 (final SK6805) `DOUT`**

to the same net `/ESP32-S3/DOUT`.

That means GPIO18 cannot safely be driven as I2S audio data while the final RGB LED output is also driving the same copper net.

For that reason, this firmware does **not** initialise the MAX98357A.

Review the hardware before enabling audio. Possible Rev-A rework would depend on the physical routing and intended design.

## First hardware bring-up order

Recommended sequence once components arrive:

1. Assemble only 3.3V supply + ESP32 + USB support parts.
2. Verify no rail shorts.
3. Flash a minimal serial test.
4. Verify 3.3V.
5. Populate/test I2C devices.
6. Populate BQ25895 and test with a current-limited bench supply.
7. Test 12V boost unloaded.
8. Test fan stage at low duty.
9. Test LEDs/buttons.
10. Test charging at a conservative current.
11. Leave OTG and audio until the rest is proven.

## Current-sense conversion

Rev-A uses:

- Rshunt = 0.05 ohm
- INA180A3 gain = 100 V/V

So:

`I_fan = Vout / (100 * 0.05) = Vout / 5`

Example: 3.0 V at the INA180 output corresponds to about 0.6 A.

## Status

This is a bring-up firmware skeleton, not production firmware. Hardware-dependent values such as charge current, thermal policy, battery limits, fan curve, USB source behaviour, watchdog policy, and fault recovery should be validated on the physical prototype.
