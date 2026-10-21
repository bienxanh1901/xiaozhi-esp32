# LCD Wiki ES3C28P / ES3N28P

2.8-inch ESP32-S3 display module.

Documentation: [https://www.lcdwiki.com/2.8inch_ESP32-S3_Display](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)

One firmware covers both SKUs: ES3C28P has a capacitive touch panel, ES3N28P
does not. Touch is detected at startup and skipped when absent.

## Specifications

| Item | Value |
|------|-------|
| SoC | ESP32-S3, 16 MB flash, 8 MB OPI PSRAM |
| Display | 2.8" IPS, 240x320, ILI9341V over SPI |
| Touch | FT6336G capacitive, I2C 0x38 (ES3C28P only) |
| Audio | ES8311 codec, MEMS microphone, speaker amplifier |
| Other | RGB LED, MicroSD, battery charging with voltage ADC |

## Pins

| Function | GPIO |
|----------|------|
| LCD CS / DC / SCK / MOSI / MISO | 10 / 46 / 12 / 11 / 13 |
| LCD backlight | 45 (high = on) |
| LCD reset | shared with the ESP32-S3 EN reset |
| I2C SDA / SCL (codec, touch) | 16 / 15 |
| Touch reset / interrupt | 18 / 17 |
| I2S MCLK / BCLK / WS / DOUT / DIN | 4 / 5 / 7 / 8 / 6 |
| Amplifier enable | 1 (low = on) |
| RGB LED | 42 |
| Battery ADC | 9 |
| BOOT button | 0 |

## Display

The screen shows animated robot eyes (`main/display/eye_display.h`, shared
with the bread-compact boards) that react to the emotion of each reply, with
the usual status bar and chat text underneath.

## Controls

- BOOT button: start or stop a conversation. Press during startup to enter
  WiFi configuration.
- Touch (ES3C28P): tap to toggle a conversation, double-tap to start
  listening, hold for 3 seconds to enter WiFi configuration.

## Build

```bash
python3 scripts/build.py lcdwiki-es3c28p
```

The pinout matches `freenove-esp32s3-display-2.8-lcd`, and this board reuses
its driver code. It has its own board type so OTA updates for one product are
never offered to the other.
