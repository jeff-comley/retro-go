// Target definition
#define RG_TARGET_NAME "XIAO-ESP32S3"

// ---------- Storage ----------
#define RG_STORAGE_ROOT "/sd"
#define RG_STORAGE_SDSPI_HOST SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED SDMMC_FREQ_DEFAULT

// ---------- Audio ----------
#define RG_AUDIO_USE_INT_DAC 0
#define RG_AUDIO_USE_EXT_DAC 1

#define RG_GPIO_SND_I2S_DATA GPIO_NUM_1
#define RG_GPIO_SND_I2S_WS   GPIO_NUM_2
#define RG_GPIO_SND_I2S_BCK  GPIO_NUM_3

// Video
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341/ST7789
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_20M // Safer for hand-wired SPI displays
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
#define RG_GAMEPAD_DRIVER 3
#define RG_GAMEPAD_HAS_MENU_BTN   1
#define RG_GAMEPAD_HAS_OPTION_BTN 1

#define RG_I2C_GPIO_DRIVER 2
#define RG_I2C_GPIO_ADDR   0x21

#define RG_GPIO_I2C_SCL GPIO_NUM_6
#define RG_GPIO_I2C_SDA GPIO_NUM_5

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

// ---------- SPI Display ----------
// Adafruit 2090 / ILI9341 breakout:
// TFT does not need MISO, but the shared SPI bus can still use MISO for the onboard microSD slot.
#define RG_GPIO_LCD_MISO GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI GPIO_NUM_9
#define RG_GPIO_LCD_CLK  GPIO_NUM_7
#define RG_GPIO_LCD_CS   GPIO_NUM_43
#define RG_GPIO_LCD_DC   GPIO_NUM_44

// ---------- SPI SD Card ----------
#define RG_GPIO_SDSPI_MISO GPIO_NUM_8
#define RG_GPIO_SDSPI_MOSI RG_GPIO_LCD_MOSI
#define RG_GPIO_SDSPI_CLK  RG_GPIO_LCD_CLK
#define RG_GPIO_SDSPI_CS   GPIO_NUM_4
