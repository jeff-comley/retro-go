# LILYGO T7-S3

- Status: working Retro-Go target for ESP32-S3 `T7-S3`
- Ref:
  - https://github.com/Xinyuan-LilyGO/T7-S3
  - https://www.adafruit.com/product/2090
  - https://learn.adafruit.com/adafruit-2-8-and-3-2-color-tft-touchscreen-breakout-v2/pinouts

# Hardware

- Board: `T7-S3` with PSRAM
- Display: Adafruit `2090` in `SPI` mode
- Input: same `AW9523`-style I2C gamepad expander used on the XIAO target
- Audio: external I2S DAC on exposed GPIO pins
- Basis: T7-S3 V1.1 pinmap image plus working bring-up on this target

# Bus layout

- LCD uses `SPI2_HOST`
- SD uses the native `SDMMC_HOST_SLOT_1` peripheral

This keeps display updates on SPI while the card uses the dedicated SDMMC/SDIO path.

# Wiring for this target

## LCD / Adafruit 2090 on SPI2

| T7-S3 GPIO | Retro-Go define | Adafruit 2090 pin |
|---|---|---|
| `GPIO12` | `RG_GPIO_LCD_CLK` | `CLK` |
| `GPIO11` | `RG_GPIO_LCD_MOSI` | `MOSI` |
| `GPIO10` | `RG_GPIO_LCD_CS` | `CS` |
| `GPIO38` | `RG_GPIO_LCD_DC` | `D/C` |
| `GPIO21` | `RG_GPIO_LCD_RST` | `RST` |
| `GPIO7` | `RG_GPIO_LCD_BCKL` | `Lite` (optional PWM brightness) |
| `3V3` or `5V` | power | `Vin` |
| `GND` | ground | `GND` |

## SD card on SDIO / SDMMC

| T7-S3 GPIO | Retro-Go define | SD signal |
|---|---|---|
| `GPIO45` | `RG_GPIO_SDSPI_CLK` | `CLK` |
| `GPIO47` | `RG_GPIO_SDSPI_CMD` | `CMD` |
| `GPIO48` | `RG_GPIO_SDSPI_D0` | `D0` |
| `GPIO15` | `RG_GPIO_SDSPI_D1` | `D1` |
| `GPIO5` | `RG_GPIO_SDSPI_D2` | `D2` |
| `GPIO8` | `RG_GPIO_SDSPI_D3` | `D3` |

This is the current working 4-bit SDIO map and the recommended storage setup for this target.

Optional card-detect:

| T7-S3 GPIO | Optional define | SD signal |
|---|---|---|
| `GPIO16` | `RG_GPIO_SDSPI_CD` | `CD` |

When `RG_GPIO_SDSPI_CD` is enabled, Retro-Go debounces the signal, pauses on card removal, and remounts after reinsertion. `CD` is treated as active-low by default, and the firmware enables an internal pull-up on that pin. `GPIO16` is just the current suggested spare pin; use whatever GPIO your card-detect switch is actually wired to.

## I2C gamepad expander

| T7-S3 GPIO | Retro-Go define |
|---|---|
| `GPIO13` | `RG_GPIO_I2C_SCL` |
| `GPIO14` | `RG_GPIO_I2C_SDA` |

## External I2S DAC

| T7-S3 GPIO | Retro-Go define |
|---|---|
| `GPIO4` | `RG_GPIO_SND_I2S_BCK` |
| `GPIO3` | `RG_GPIO_SND_I2S_WS` |
| `GPIO1` | `RG_GPIO_SND_I2S_DATA` |

## Battery monitor

| T7-S3 GPIO | Retro-Go define |
|---|---|
| `GPIO2` | `RG_BATTERY_ADC_CHANNEL` (`ADC1_CH1`) |

# Notes

- If your wiring differs, edit [config.h](/Users/jeffc/Development/Xiao/retro-go/components/retro-go/targets/lilygo-t7-s3/config.h).
- The Adafruit breakout's touchscreen interface is not used here.
- The Adafruit breakout must be switched to `SPI` mode.
- The T7-S3 V1.1 pinmap image was used as the basis for these pin choices.
- `GPIO2` is reserved for battery detection and is now used only for ADC battery sensing.
- Brightness control assumes the display `Lite` pin is wired to `GPIO7`.
- Battery voltage scaling is currently assumed to be a 1:1 divider. If the displayed voltage is off, calibrate the formulas in [config.h](/Users/jeffc/Development/Xiao/retro-go/components/retro-go/targets/lilygo-t7-s3/config.h) against a multimeter.
- Internal SDMMC pull-ups are enabled in software, but the SDIO bus should still have proper external pull-ups for reliable operation.
- SD runs at `10 MHz` for a better stability/performance balance on this wiring, and it will fall back to probing speed automatically if mount-time CRC/timeout errors occur.
- If you enable `CD`, low means card inserted and high means no card unless you override `RG_GPIO_SDSPI_CD_PRESENT_LEVEL`.
- Native SDIO is the supported storage path here. The earlier SPI experiment was abandoned because SDIO proved more reliable on this hardware.

# Build

```sh
./rg_tool.py --target=lilygo-t7-s3 build launcher
./rg_tool.py --target=lilygo-t7-s3 build retro-core
```
