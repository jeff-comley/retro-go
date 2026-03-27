// Target definition
#define RG_TARGET_NAME "LILYGO-T7-S3"
#define RG_OVERCLOCK_MAX_LEVEL 12
#define RG_A26_OVERCLOCK_LEVEL 0

// ---------- Storage ----------
#define RG_STORAGE_ROOT "/sd"
#define RG_STORAGE_SDMMC_HOST SDMMC_HOST_SLOT_1
// 10 MHz has been stable on this wiring.
#define RG_STORAGE_SDMMC_SPEED 20000
#define RG_STORAGE_SDMMC_FLAGS SDMMC_SLOT_FLAG_INTERNAL_PULLUP

// ---------- Audio ----------
// Assumes an external I2S DAC. Avoid GPIO2 because T7-S3 V1.1 uses it for battery detection.
#define RG_AUDIO_USE_INT_DAC 0
#define RG_AUDIO_USE_EXT_DAC 1
#define RG_AUDIO_MIX_TO_MONO 1

#define RG_GPIO_SND_I2S_DATA GPIO_NUM_1
#define RG_GPIO_SND_I2S_WS   GPIO_NUM_3
#define RG_GPIO_SND_I2S_BCK  GPIO_NUM_4

// ---------- Video ----------
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341/ST7789
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_INIT()                                                                                         \
    ILI9341_CMD(0xEF, 0x03, 0x80, 0x02);                                                                         \
    ILI9341_CMD(0xCF, 0x00, 0xC1, 0x30);                                                                         \
    ILI9341_CMD(0xED, 0x64, 0x03, 0x12, 0x81);                                                                   \
    ILI9341_CMD(0xE8, 0x85, 0x00, 0x78);                                                                         \
    ILI9341_CMD(0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02);                                                             \
    ILI9341_CMD(0xF7, 0x20);                                                                                     \
    ILI9341_CMD(0xEA, 0x00, 0x00);                                                                               \
    ILI9341_CMD(0xC0, 0x23);                 /* PWCTR1 */                                                        \
    ILI9341_CMD(0xC1, 0x10);                 /* PWCTR2 */                                                        \
    ILI9341_CMD(0xC5, 0x3E, 0x28);           /* VMCTR1 */                                                        \
    ILI9341_CMD(0xC7, 0x86);                 /* VMCTR2 */                                                        \
    ILI9341_CMD(0x36, 0x28);                 /* MADCTL: landscape, BGR */                                        \
    ILI9341_CMD(0x37, 0x00);                 /* VSCRSADD */                                                      \
    ILI9341_CMD(0xB1, 0x00, 0x18);           /* FRMCTR1 */                                                       \
    ILI9341_CMD(0xB6, 0x08, 0x82, 0x27);     /* DFUNCTR */                                                       \
    ILI9341_CMD(0xF2, 0x00);                 /* Disable 3-gamma */                                               \
    ILI9341_CMD(0x26, 0x01);                 /* GAMMASET */                                                      \
    ILI9341_CMD(0xE0, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00); \
    ILI9341_CMD(0xE1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F);

// ---------- Input ----------
// Assumes the same AW9523/compatible I2C GPIO expander wiring as the XIAO target.
// T7-S3 V1.1 pinmap shows the STEMMA QT / Qwiic connector on GPIO14 / GPIO13.
#define RG_GAMEPAD_DRIVER 3
#define RG_GAMEPAD_HAS_MENU_BTN   1
#define RG_GAMEPAD_HAS_OPTION_BTN 1

#define RG_I2C_GPIO_DRIVER 2
#define RG_I2C_GPIO_ADDR   0x21

#define RG_GPIO_I2C_SCL GPIO_NUM_13
#define RG_GPIO_I2C_SDA GPIO_NUM_14

#define RG_GAMEPAD_I2C_MAP {\
    {RG_KEY_UP,     .num = 1,  .level = 0},\
    {RG_KEY_RIGHT,  .num = 3,  .level = 0},\
    {RG_KEY_DOWN,   .num = 4,  .level = 0},\
    {RG_KEY_LEFT,   .num = 2,  .level = 0},\
    {RG_KEY_SELECT, .num = 5,  .level = 0},\
    {RG_KEY_START,  .num = 11, .level = 0},\
    {RG_KEY_A,      .num = 14, .level = 0},\
    {RG_KEY_B,      .num = 12, .level = 0},\
    {RG_KEY_X,      .num = 15, .level = 0},\
    {RG_KEY_Y,      .num = 13, .level = 0},\
}

#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_MENU,   .src = RG_KEY_SELECT | RG_KEY_B },\
    {RG_KEY_OPTION, .src = RG_KEY_SELECT | RG_KEY_UP},\
}

// ---------- Status LED ----------
#define RG_GPIO_LED GPIO_NUM_17

// ---------- Battery ----------
// T7-S3 V1.1 pinmap marks GPIO2 as battery detection (ADC1_CH1).
// Voltage scaling is assumed to be a 1:1 divider, matching common ESP32 battery monitor boards.
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_1
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3150.f) / (4150.f - 3150.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)

// ---------- SPI Display ----------
// Based on the T7-S3 V1.1 pinmap, keep the LCD on the exposed FSPI-capable group.
#define RG_GPIO_LCD_MISO GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI GPIO_NUM_11
#define RG_GPIO_LCD_CLK  GPIO_NUM_12
#define RG_GPIO_LCD_CS   GPIO_NUM_10
#define RG_GPIO_LCD_DC   GPIO_NUM_38
#define RG_GPIO_LCD_RST  GPIO_NUM_21
#define RG_GPIO_LCD_BCKL GPIO_NUM_7

// ---------- SDMMC / SDIO Card ----------
#define RG_GPIO_SDSPI_CLK   GPIO_NUM_45
#define RG_GPIO_SDSPI_CMD   GPIO_NUM_47
#define RG_GPIO_SDSPI_D0    GPIO_NUM_48
#define RG_GPIO_SDSPI_D1    GPIO_NUM_15
#define RG_GPIO_SDSPI_D2    GPIO_NUM_5
#define RG_GPIO_SDSPI_D3    GPIO_NUM_8
// #define RG_GPIO_SDSPI_CD    GPIO_NUM_16
