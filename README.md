# ESP32-S3 Round LCD LVGL Test

Minimal ESP-IDF project for a 360x360 ST77916/JD9855 QSPI round display and an FT5x06-compatible I2C touch controller.

## Hardware

- Target: ESP32-S3
- LCD: 360x360, JD9855/ST77916-compatible QSPI
- LCD pins: CS=GPIO1, SCLK=GPIO2, D0=GPIO4, D1=GPIO3, D2=GPIO5, D3=GPIO6, RST=NC, BL=GPIO0 active high
- Touch: FT5x06-compatible controller, I2C0, SDA=GPIO8, SCL=GPIO7, address=0x15

The touch controller IC was not specified, so the project uses the FT5x06 protocol from the reference project. If the hardware uses CST8xx/HYN or another controller, replace the touch component and constructor while keeping the new I2C master bus setup.

## Build

Use an ESP-IDF 6.1 terminal:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

The first screen is intentionally small: a dark background, a center label, and a touch counter. Touches update the counter and log coordinates, which gives a quick display and input sanity check.
