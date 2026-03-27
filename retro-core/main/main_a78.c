#include "shared.h"

#define retro_set_video_refresh prosystem_retro_set_video_refresh
#define retro_set_audio_sample prosystem_retro_set_audio_sample
#define retro_set_audio_sample_batch prosystem_retro_set_audio_sample_batch
#define retro_set_input_poll prosystem_retro_set_input_poll
#define retro_set_input_state prosystem_retro_set_input_state
#define retro_set_environment prosystem_retro_set_environment
#define retro_get_system_av_info prosystem_retro_get_system_av_info
#define retro_set_controller_port_device prosystem_retro_set_controller_port_device
#define retro_serialize_size prosystem_retro_serialize_size
#define retro_serialize prosystem_retro_serialize
#define retro_unserialize prosystem_retro_unserialize
#define retro_load_game prosystem_retro_load_game
#define retro_unload_game prosystem_retro_unload_game
#define retro_init prosystem_retro_init
#define retro_deinit prosystem_retro_deinit
#define retro_reset prosystem_retro_reset
#define retro_run prosystem_retro_run

#include <libretro.h>

enum
{
    A78_CORE_AUDIO_SAMPLE_RATE = 31440,
    A78_MAX_WIDTH = 320,
    A78_MAX_HEIGHT = 292,
};

#ifndef RG_A78_AUDIO_FILTER_ENABLED
#define RG_A78_AUDIO_FILTER_ENABLED 1
#endif

#ifndef RG_A78_AUDIO_FILTER_LEVEL
#define RG_A78_AUDIO_FILTER_LEVEL "60"
#endif

#ifndef RG_A78_DUAL_STICK_HACK
#define RG_A78_DUAL_STICK_HACK 0
#endif

static rg_app_t *app;
static rg_surface_t *updates[2];
static rg_surface_t *currentUpdate;
static rg_surface_t *lastSubmittedUpdate;
static int updateCapacityWidth;
static int updateCapacityHeight;
static void *romData;
static size_t romSize;
static uint32_t gamepadState;
static int skipFrames;
static int coreSampleRate;
static bool slowFrame;
static bool drawFrame;
static bool coreInitialized;
static bool gameLoaded;

static const char *a78_get_core_variable(const char *key)
{
    if (!key)
        return NULL;

    if (strcmp(key, "prosystem_color_depth") == 0)
        return "16bit";

    if (strcmp(key, "prosystem_low_pass_filter") == 0)
        return RG_A78_AUDIO_FILTER_ENABLED ? "enabled" : "disabled";

    if (strcmp(key, "prosystem_low_pass_range") == 0)
        return RG_A78_AUDIO_FILTER_LEVEL;

    if (strcmp(key, "prosystem_gamepad_dual_stick_hack") == 0)
        return RG_A78_DUAL_STICK_HACK ? "enabled" : "disabled";

    return NULL;
}

static void a78_shutdown_core(void)
{
    if (gameLoaded)
    {
        retro_unload_game();
        gameLoaded = false;
    }

    if (coreInitialized)
    {
        retro_deinit();
        coreInitialized = false;
    }

    free(romData);
    romData = NULL;
    romSize = 0;
}

static uint32_t a78_get_joypad_mask(void)
{
    uint32_t mask = 0;

    if (gamepadState & RG_KEY_UP)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
    if (gamepadState & RG_KEY_DOWN)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
    if (gamepadState & RG_KEY_LEFT)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
    if (gamepadState & RG_KEY_RIGHT)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;

    if (gamepadState & (RG_KEY_A | RG_KEY_X))
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
    if (gamepadState & (RG_KEY_B | RG_KEY_Y))
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_A;

    if (gamepadState & RG_KEY_SELECT)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
    if (gamepadState & RG_KEY_START)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_START;

    return mask;
}

static bool environment_cb(unsigned cmd, void *data)
{
    switch (cmd)
    {
        case RETRO_ENVIRONMENT_GET_OVERSCAN:
            *(bool *)data = false;
            return true;

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *(bool *)data = false;
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;

        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            struct retro_variable *var = data;
            const char *value = a78_get_core_variable(var ? var->key : NULL);
            if (!var || !value)
                return false;
            var->value = value;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *(const char **)data = RG_BASE_PATH_BIOS;
            return true;

        case RETRO_ENVIRONMENT_SET_VARIABLES:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
            return true;

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *(bool *)data = false;
            return true;

        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            return true;

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return true;

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
            return false;

        default:
            return false;
    }
}

static void video_cb(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!drawFrame || !currentUpdate || data == NULL || width < 1 || height < 1)
        return;

    RG_ASSERT(width <= updateCapacityWidth && height <= updateCapacityHeight, "Unexpected Atari 7800 frame size");
    slowFrame = !rg_display_sync(false);

    const uint8_t *source = data;
    uint8_t *dest = currentUpdate->data;
    size_t rowSize = width * sizeof(uint16_t);

    if (pitch == (size_t)currentUpdate->stride && rowSize == pitch)
    {
        memcpy(dest, source, height * pitch);
    }
    else
    {
        for (unsigned y = 0; y < height; ++y)
            memcpy(dest + y * currentUpdate->stride, source + y * pitch, rowSize);
    }

    currentUpdate->width = width;
    currentUpdate->height = height;
    currentUpdate->offset = 0;

    rg_display_submit(currentUpdate, 0);
    lastSubmittedUpdate = currentUpdate;
    currentUpdate = updates[currentUpdate == updates[0]];
}

static void a78_audio_submit(const rg_audio_frame_t *frames, size_t count)
{
    if (!frames || count < 1)
        return;

    rg_audio_submit(frames, count);
}

static void audio_cb(int16_t left, int16_t right)
{
    rg_audio_frame_t frame = {left, right};
    a78_audio_submit(&frame, 1);
}

static size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    if (data && frames)
        a78_audio_submit((const rg_audio_frame_t *)data, frames);
    return frames;
}

static void input_poll_cb(void)
{
}

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)index;

    if (port > 0)
        return 0;

    if (device == RETRO_DEVICE_JOYPAD)
    {
        uint32_t mask = a78_get_joypad_mask();

        if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return (int16_t)mask;

        return (mask & (1u << id)) ? 1 : 0;
    }

    if (device == RETRO_DEVICE_ANALOG)
        return 0;

    return 0;
}

static void event_handler(int event, void *arg)
{
    (void)arg;

    if (event == RG_EVENT_REDRAW && lastSubmittedUpdate)
        rg_display_submit(lastSubmittedUpdate, 0);
}

static bool screenshot_handler(const char *filename, int width, int height)
{
    const rg_surface_t *surface = lastSubmittedUpdate ? lastSubmittedUpdate : currentUpdate;
    return surface ? rg_surface_save_image_file(surface, filename, width, height) : false;
}

static bool save_state_handler(const char *filename)
{
    size_t size = retro_serialize_size();
    void *data = NULL;
    bool success = false;

    if (size < 1)
        return false;

    data = malloc(size);
    if (!data)
        return false;

    if (retro_serialize(data, size))
    {
        FILE *fp = fopen(filename, "wb");
        if (fp)
        {
            success = fwrite(data, 1, size, fp) == size;
            fclose(fp);
        }
    }

    free(data);
    return success;
}

static bool load_state_handler(const char *filename)
{
    void *data = NULL;
    size_t size = 0;
    bool success = false;

    if (rg_storage_read_file(filename, &data, &size, 0))
    {
        success = retro_unserialize(data, size);
        free(data);
    }

    if (!success)
        retro_reset();

    return success;
}

static bool reset_handler(bool hard)
{
    (void)hard;
    retro_reset();
    return true;
}

void a78_main(void)
{
    const rg_handlers_t handlers = {
        .loadState = &load_state_handler,
        .saveState = &save_state_handler,
        .reset = &reset_handler,
        .screenshot = &screenshot_handler,
        .event = &event_handler,
    };
    struct retro_game_info info = {0};
    struct retro_system_av_info av = {0};

    app = rg_system_reinit(A78_CORE_AUDIO_SAMPLE_RATE, &handlers, NULL);

    retro_set_environment(&environment_cb);
    retro_set_video_refresh(&video_cb);
    retro_set_audio_sample(&audio_cb);
    retro_set_audio_sample_batch(&audio_batch_cb);
    retro_set_input_poll(&input_poll_cb);
    retro_set_input_state(&input_state_cb);
    retro_init();
    coreInitialized = true;

    if (rg_extension_match(app->romPath, "zip"))
    {
        if (!rg_storage_unzip_file(app->romPath, NULL, &romData, &romSize, 0))
            RG_PANIC("ROM file unzipping failed!");
    }
    else if (!rg_storage_read_file(app->romPath, &romData, &romSize, 0))
    {
        RG_PANIC("ROM file loading failed!");
    }

    info.path = app->romPath;
    info.data = romData;
    info.size = romSize;

    if (!retro_load_game(&info))
        RG_PANIC("ROM file loading failed!");

    gameLoaded = true;

    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
    retro_get_system_av_info(&av);

    coreSampleRate = (av.timing.sample_rate > 1000) ? (int)(av.timing.sample_rate + 0.5f) : A78_CORE_AUDIO_SAMPLE_RATE;
    app->sampleRate = coreSampleRate;
    rg_audio_set_sample_rate(app->sampleRate);

    updateCapacityWidth = RG_MIN((int)av.geometry.max_width ?: av.geometry.base_width ?: A78_MAX_WIDTH, A78_MAX_WIDTH);
    updateCapacityHeight = RG_MIN((int)av.geometry.max_height ?: av.geometry.base_height ?: A78_MAX_HEIGHT, A78_MAX_HEIGHT);

    updates[0] = rg_surface_create(updateCapacityWidth, updateCapacityHeight, RG_PIXEL_565_LE, MEM_ANY);
    updates[1] = rg_surface_create(updateCapacityWidth, updateCapacityHeight, RG_PIXEL_565_LE, MEM_ANY);
    currentUpdate = updates[0];
    lastSubmittedUpdate = updates[0];

    rg_system_set_tick_rate((int)(av.timing.fps + 0.5f));
    app->frameskip = 0;
    app->autoFrameskip = false;
    skipFrames = 0;
    slowFrame = false;
    drawFrame = true;

    if (app->bootFlags & RG_BOOT_RESUME)
        rg_emu_load_state(app->saveSlot);

    while (true)
    {
        int64_t startTime = rg_system_timer();
        drawFrame = (skipFrames == 0);
        slowFrame = false;

        gamepadState = rg_input_read_gamepad();

        if (gamepadState & (RG_KEY_MENU | RG_KEY_OPTION))
        {
            if (gamepadState & RG_KEY_MENU)
                rg_gui_game_menu();
            else
                rg_gui_options_menu();
            continue;
        }

        retro_run();
        int elapsed = rg_system_timer() - startTime;
        rg_system_tick(elapsed);

        if (skipFrames == 0)
        {
            if (elapsed > app->frameTime + 1500)
                skipFrames = 1;
            else if (drawFrame && slowFrame)
                skipFrames = 1;
        }
        else if (skipFrames > 0)
        {
            skipFrames--;
        }
    }

    a78_shutdown_core();
}
