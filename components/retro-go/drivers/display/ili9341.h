#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>

static QueueHandle_t lcd_free_buffers;
static QueueHandle_t lcd_inflight_buffers;
static esp_lcd_panel_io_handle_t lcd_io = NULL;

#define LCD_BUFFER_COUNT  (3)
#define LCD_BUFFER_BYTES  (LCD_BUFFER_LENGTH * sizeof(uint16_t))

static int lcd_window_left = 0;
static int lcd_window_width = 0;
static int lcd_window_next_y = 0;

static inline uint16_t *spi_take_buffer(void)
{
    uint16_t *buffer;
    if (xQueueReceive(lcd_free_buffers, &buffer, pdMS_TO_TICKS(2500)) != pdTRUE)
        RG_PANIC("display");
    return buffer;
}

static inline void spi_give_buffer(uint16_t *buffer)
{
    xQueueSend(lcd_free_buffers, &buffer, portMAX_DELAY);
}

static bool lcd_color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;

    BaseType_t high_task_wakeup = pdFALSE;
    void *buffer = NULL;

    if (xQueueReceiveFromISR(lcd_inflight_buffers, &buffer, &high_task_wakeup) == pdTRUE && buffer)
        xQueueSendFromISR(lcd_free_buffers, &buffer, &high_task_wakeup);

    return high_task_wakeup == pdTRUE;
}

static void lcd_reclaim_done_buffers(void)
{
    // Callback returns finished buffers directly to lcd_free_buffers.
}

static inline void lcd_write_cmd(uint8_t cmd, const void *data, size_t length)
{
    esp_err_t ret = esp_lcd_panel_io_tx_param(lcd_io, cmd, data, length);
    RG_ASSERT(ret == ESP_OK, "esp_lcd_panel_io_tx_param failed.");
}

#define ILI9341_CMD(cmd, data...)         \
    {                                     \
        const uint8_t x[] = {data};       \
        lcd_write_cmd((cmd), x, sizeof(x)); \
    }

static void spi_init(void)
{
    lcd_free_buffers = xQueueCreate(LCD_BUFFER_COUNT, sizeof(uint16_t *));
    lcd_inflight_buffers = xQueueCreate(LCD_BUFFER_COUNT, sizeof(uint16_t *));
    RG_ASSERT(lcd_free_buffers, "lcd free buffer queue alloc failed");
    RG_ASSERT(lcd_inflight_buffers, "lcd inflight buffer queue alloc failed");

    while (uxQueueSpacesAvailable(lcd_free_buffers))
    {
        void *buffer = rg_alloc(LCD_BUFFER_BYTES, MEM_DMA);
        RG_ASSERT(buffer, "lcd buffer alloc failed");
        xQueueSend(lcd_free_buffers, &buffer, portMAX_DELAY);
    }

    const spi_bus_config_t buscfg = {
#if defined(RG_STORAGE_SDSPI_HOST) && defined(RG_GPIO_SDSPI_MISO)
        // When the LCD and SD share a host, storage may have already initialized the bus
        // with a valid MISO pin. Keeping the SD MISO available is harmless for LCD-only writes.
        .miso_io_num = RG_GPIO_SDSPI_MISO,
#else
        .miso_io_num = RG_GPIO_LCD_MISO,
#endif
        .mosi_io_num = RG_GPIO_LCD_MOSI,
        .sclk_io_num = RG_GPIO_LCD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_BUFFER_BYTES + 16,
    };

    esp_err_t ret;
    ret = spi_bus_initialize(RG_SCREEN_HOST, &buscfg, SPI_DMA_CH_AUTO);
    RG_ASSERT(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, "spi_bus_initialize failed.");

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = RG_GPIO_LCD_CS,
        .dc_gpio_num = RG_GPIO_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = RG_SCREEN_SPEED,
        .trans_queue_depth = LCD_BUFFER_COUNT,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = lcd_color_trans_done_cb,
        .user_ctx = NULL,
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)RG_SCREEN_HOST, &io_config, &lcd_io);
    RG_ASSERT(ret == ESP_OK, "esp_lcd_new_panel_io_spi failed.");
}

static void spi_deinit(void)
{
    if (lcd_io)
    {
        esp_lcd_panel_io_del(lcd_io);
        lcd_io = NULL;
    }
}

static void lcd_set_backlight(float percent)
{
    float level = RG_MIN(RG_MAX(percent / 100.f, 0), 1.f);
    int error_code = 0;

#if defined(RG_GPIO_LCD_BCKL)
    error_code = ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0x1FFF * level, 50, 0);
#endif

    if (error_code)
        RG_LOGE("failed setting backlight to %d%% (0x%02X)\n", (int)(100 * level), error_code);
    else
        RG_LOGI("backlight set to %d%%\n", (int)(100 * level));
}

static void lcd_set_window(int left, int top, int width, int height)
{
    (void)height;
    lcd_window_left = left;
    lcd_window_width = width;
    lcd_window_next_y = top;
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    (void)length;
    lcd_reclaim_done_buffers();
    return spi_take_buffer();
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    if (length == 0)
    {
        spi_give_buffer(buffer);
        return;
    }

    RG_ASSERT(lcd_window_width > 0, "lcd window width invalid");

    const int lines = length / lcd_window_width;
    RG_ASSERT(lines > 0, "lcd_send_buffer: zero lines");
    RG_ASSERT((lines * lcd_window_width) == (int)length, "lcd_send_buffer: partial line chunk");

    const int left = lcd_window_left;
    const int right = left + lcd_window_width - 1;
    const int top = lcd_window_next_y;
    const int bottom = top + lines - 1;
    const uint8_t col_data[] = {left >> 8, left & 0xff, right >> 8, right & 0xff};
    const uint8_t row_data[] = {top >> 8, top & 0xff, bottom >> 8, bottom & 0xff};

    lcd_write_cmd(0x2A, col_data, sizeof(col_data));
    lcd_write_cmd(0x2B, row_data, sizeof(row_data));

    xQueueSend(lcd_inflight_buffers, &buffer, portMAX_DELAY);

    esp_err_t ret = esp_lcd_panel_io_tx_color(lcd_io, 0x2C, buffer, length * sizeof(*buffer));
    if (ret != ESP_OK)
    {
        void *tmp = NULL;
        xQueueReceive(lcd_inflight_buffers, &tmp, 0);
        spi_give_buffer(buffer);
        RG_ASSERT(false, "esp_lcd_panel_io_tx_color failed.");
    }

    lcd_window_next_y += lines;
}

static void lcd_sync(void)
{
    while (uxQueueMessagesWaiting(lcd_inflight_buffers) > 0)
        vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_init(void)
{
#ifdef RG_GPIO_LCD_BCKL
    ledc_timer_config(&(ledc_timer_config_t){
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
    });
    ledc_channel_config(&(ledc_channel_config_t){
        .gpio_num = RG_GPIO_LCD_BCKL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    #ifdef RG_GPIO_LCD_BCKL_INVERT
        .flags.output_invert = 1,
    #endif
    });
    ledc_fade_func_install(0);
#endif

    spi_init();

#if defined(RG_GPIO_LCD_RST)
    gpio_set_direction(RG_GPIO_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(RG_GPIO_LCD_RST, 0);
    rg_usleep(100 * 1000);
    gpio_set_level(RG_GPIO_LCD_RST, 1);
    rg_usleep(150 * 1000);
#endif

    lcd_write_cmd(0x01, NULL, 0);
    rg_usleep(150 * 1000);

    {
        const uint8_t pixel_format = 0x55;
        lcd_write_cmd(0x3A, &pixel_format, 1);
    }

#ifdef RG_SCREEN_INIT
    RG_SCREEN_INIT();
#else
    #warning "LCD init sequence is not defined for this device!"
#endif

    lcd_write_cmd(0x11, NULL, 0);
    rg_usleep(150 * 1000);
    lcd_write_cmd(0x29, NULL, 0);
}

static void lcd_deinit(void)
{
#ifdef RG_SCREEN_DEINIT
    RG_SCREEN_DEINIT();
#endif
    lcd_sync();
    spi_deinit();
}

const rg_display_driver_t rg_display_driver_ili9341 = {
    .name = "ili9341",
};
