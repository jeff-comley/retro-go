# XIAO ESP32S3

- Status: `ILI9341` SPI display target, confirmed to match Adafruit product `2090`
- Ref:
  - https://www.adafruit.com/product/2090
  - https://learn.adafruit.com/adafruit-2-8-and-3-2-color-tft-touchscreen-breakout-v2/pinouts

# Adafruit 2090 notes

- This target uses the display in landscape mode at `320x240`, which is a good fit for Retro-Go.
- The board must be in `SPI` mode. Per Adafruit's guide, the SPI-mode jumper on the breakout must be closed.
- Capacitive touch is not used by Retro-Go in this target.
- `RST` is optional on this breakout and is not assigned in this target.
- `Lite` is optional. If you leave it unconnected, the backlight stays on by default.
- `GPIO43` and `GPIO44` are used for TFT control in this target, so the target `sdkconfig` is set to use the ESP32-S3 USB Serial/JTAG console instead of UART on those pins.

# Wiring used by this target

| XIAO ESP32S3 GPIO | Retro-Go define | Adafruit 2090 pin |
|---|---|---|
| `GPIO7` | `RG_GPIO_LCD_CLK` / `RG_GPIO_SDSPI_CLK` | `CLK` |
| `GPIO9` | `RG_GPIO_LCD_MOSI` / `RG_GPIO_SDSPI_MOSI` | `MOSI` |
| `GPIO8` | `RG_GPIO_SDSPI_MISO` | `MISO` |
| `GPIO43` | `RG_GPIO_LCD_CS` | `CS` |
| `GPIO44` | `RG_GPIO_LCD_DC` | `D/C` |
| `GPIO4` | `RG_GPIO_SDSPI_CS` | `Card CS / CCS` |
| `3V3` or `5V` | power | `Vin` |
| `GND` | ground | `GND` |

# What is not mapped here

- `SDA`, `SCL`, `IRQ`: touchscreen interface, not used by Retro-Go
- `RST`: optional display reset
- `Lite`: optional backlight PWM
- `CD`: optional SD card detect

# Build

```sh
./rg_tool.py --target=xiao-esp32s3 build launcher
```

If you use the display breakout's microSD slot for ROM storage, wire `MISO` and `Card CS` exactly as shown above.
