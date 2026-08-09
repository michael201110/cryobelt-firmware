# CryoBelt Rev-A Arduino Firmware

Initial firmware skeleton for the custom CryoBelt ESP32-S3-MINI-1-N8 PCB.

## What works in this skeleton

- Native USB serial logging
- I2C on GPIO9/GPIO8
- BQ25895 identity checking and verified fail-closed boot configuration
- Dual-interlock ESP-authorised charging (disabled by default)
- SHT40 temperature/humidity readings
- Fan PWM on GPIO13
- INA180A3 fan-current reading on GPIO14
- Three front buttons
- Three SK6805 addressable LEDs
- FUSB303B GPIO-mode role/current-state reading
- 12V boost enable control
- OTG gate defaults safely OFF
- Bluetooth LE control and telemetry for the companion Flutter app
- MAX98357A I2S audio on reworked Rev-A boards

## Companion app and BLE protocol

The companion Flutter project is in the sibling `cryobelt_app` repository. The
firmware advertises as `CryoBelt` and exposes service
`7d4b1000-6c4a-4f65-9f09-8a2c7d3e1000`.

- Command characteristic `...1001` accepts exactly three bytes: protocol
  version (`3`), opcode, and value. Opcodes are power (`1`, value 0/1), fan
  percent (`2`, value 0-100), mode (`3`, value 0-3), and Find My Belt
  (`4`, value 1). Pod safety recheck uses opcode `5`, value `1`.
- Telemetry characteristic `...1002` is readable and notifiable. Its fixed
  16-byte packet carries state flags, requested/actual fan level, mode, USB
  role, estimated pod count, temperature, humidity, fan current, battery
  voltage, and BQ25895 status/fault bytes.

Cooling is switched off when the BLE client disconnects. Remote commands never
enable charging or OTG; those remain governed by the firmware safety
interlocks.

Find My Belt flashes the three status LEDs blue and emits a short chirp once per
second for eight seconds. It stops automatically or immediately on BLE
disconnect and does not change the cooling, charging, or OTG state.

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
5. disables autonomous DPDM/HVDCP/MaxCharge/ICO behavior because the BQ D+/D- pins are unconnected on Rev-A;
6. programs and reads back the complete provisional charge profile;
7. disables the BQ I2C watchdog so it cannot restore unsafe register defaults after 40 seconds;
8. leaves the hardware ILIM clamp enabled unless both charge-authorisation interlocks are true;
9. only then disables ILIM-pin limiting and enables charging.

The input limit, validated battery envelope, conservative bring-up profile, and both authorisation interlocks are in `config.h`. The current battery specification is a 4000 mAh, single-cell 3.7 V nominal Li-ion pack rated for charging at up to 1C. Firmware initially limits fast-charge current to 960 mA (0.24C), while the 500 mA USB input limit remains the tighter practical limit.

For first bench bring-up, it is sensible to set:

```cpp
constexpr bool CHARGER_PROFILE_VALIDATED = true;
constexpr bool ALLOW_CHARGING_AFTER_BOOT = false;
```

`ALLOW_CHARGING_AFTER_BOOT` must remain false until the charger section has been bench tested with a current-limited supply. Raising either the input or charge-current limit also requires PCB thermal validation.

The fan and 12 V boost also remain off after boot. Press USER to start the fan at the configured default level.

## Rev-A audio rework

The supplied PCB/netlist connects:

- ESP32 GPIO18
- MAX98357A `DIN`
- **D3 (final SK6805) `DOUT`**

to the same net `/ESP32-S3/DOUT`.

The affected board has been reworked by cutting the final SK6805 DOUT trace.
Continuity testing confirmed that GPIO18 remains connected to MAX98357A `DIN`
and is isolated from D3 `DOUT`. The firmware therefore enables I2S and plays a
short, low-volume two-tone confirmation during startup.

Do not flash this audio-enabled configuration onto an unreworked Rev-A board.
Set `AUDIO_HARDWARE_REWORKED` to `false` in `config.h` for any PCB that has not
had the trace cut and verified.

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
11. Test audio only after verifying the GPIO18/D3-DOUT rework.
12. Leave OTG disabled until the rest is proven.

## Current-sense conversion

Rev-A uses:

- Rshunt = 0.05 ohm
- INA180A3 gain = 100 V/V

So:

`I_fan = Vout / (100 * 0.05) = Vout / 5`

Example: 3.0 V at the INA180 output corresponds to about 0.6 A.

## Pod-count estimate

Each pod contains one CFM-5010V-155-310 fan, rated for 12 V, 81 mA maximum,
and 0.98 W. Once fan output has been stable for 0.75 seconds at 40% or more, the
firmware calculates:

`pods = round((12 V * measured current / PWM duty) / 0.98 W)`

The result is clamped to eight pods because the Rev-A INA180A3 and ESP32 ADC
range can measure only about 0.66 A. This is an estimate: fan unit variation,
air restriction, PWM behaviour, and the unmeasured +12 V rail affect accuracy.
Calibrate `POD_FAN_RATED_POWER_W` using one real pod before relying on the count.

Separately from the rounded count, firmware trips when PWM-normalized fan
current exceeds 496 mA: six fans at the 81 mA nameplate maximum plus a 10 mA
measurement margin. During each safety pulse this limit is sampled every 50 ms
after the 750 ms settling period.

Cooling startup first runs a one-second, full-duty pod check. An estimate above
six latches the 12 V fan rail off and reports a warning to the app. After pods
are removed, the user must explicitly confirm a one-second recheck in the app;
the rail remains off after a successful recheck until cooling is requested
again. A failed or invalid recheck remains latched off.

While cooling remains active, firmware repeats the full-duty safety pulse every
30 seconds. This bounds the low-duty hot-plug detection delay, but it produces a
brief full-speed fan event. Current-only detection is not safety-rated until
the thresholds are calibrated with physical pods across voltage, temperature,
and airflow conditions.

## Status

This is a bring-up firmware skeleton, not production firmware. Hardware-dependent values such as charge current, thermal policy, battery limits, fan curve, USB source behaviour, watchdog policy, and fault recovery should be validated on the physical prototype.
