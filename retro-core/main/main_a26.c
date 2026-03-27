#include "shared.h"

#include <libretro.h>

enum
{
    A26_CORE_AUDIO_SAMPLE_RATE = 31400,
    A26_MAX_WIDTH = 320,
    A26_MAX_HEIGHT = 256,
};

#ifndef RG_A26_OVERCLOCK_LEVEL
#define RG_A26_OVERCLOCK_LEVEL 0
#endif

#ifndef RG_A26_AUDIO_FILTER_ENABLED
#define RG_A26_AUDIO_FILTER_ENABLED 1
#endif

#ifndef RG_A26_AUDIO_FILTER_LEVEL
#define RG_A26_AUDIO_FILTER_LEVEL "60"
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
static int leftDifficulty;
static int rightDifficulty;
static int tvMode;
static int skipFrames;
static bool slowFrame;
static bool drawFrame;
static bool coreInitialized;
static bool gameLoaded;

static const char *SETTING_LEFT_DIFFICULTY = "a26LeftDifficulty";
static const char *SETTING_RIGHT_DIFFICULTY = "a26RightDifficulty";
static const char *SETTING_TV_MODE = "a26TvMode";

static const char *a26_get_core_variable(const char *key)
{
    if (!key)
        return NULL;

    if (strcmp(key, "stella2014_color_depth") == 0)
        return "16bit";

    if (strcmp(key, "stella2014_mix_frames") == 0)
        return "disabled";

    if (strcmp(key, "stella2014_low_pass_filter") == 0)
        return RG_A26_AUDIO_FILTER_ENABLED ? "enabled" : "disabled";

    if (strcmp(key, "stella2014_low_pass_range") == 0)
        return RG_A26_AUDIO_FILTER_LEVEL;

    if (strcmp(key, "stella2014_paddle_digital_sensitivity") == 0)
        return "50";

    if (strcmp(key, "stella2014_paddle_analog_sensitivity") == 0)
        return "50";

    if (strcmp(key, "stella2014_paddle_analog_response") == 0)
        return "linear";

    if (strcmp(key, "stella2014_paddle_analog_deadzone") == 0)
        return "15";

    if (strcmp(key, "stella2014_stelladaptor_analog_sensitivity") == 0)
        return "20";

    if (strcmp(key, "stella2014_stelladaptor_analog_center") == 0)
        return "0";

    return NULL;
}

static void a26_shutdown_core(void)
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

static uint32_t a26_get_joypad_mask(void)
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

    if (gamepadState & (RG_KEY_A | RG_KEY_B | RG_KEY_X | RG_KEY_Y))
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_B;

    if (gamepadState & RG_KEY_SELECT)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
    if (gamepadState & RG_KEY_START)
        mask |= 1u << RETRO_DEVICE_ID_JOYPAD_START;

    mask |= 1u << (leftDifficulty ? RETRO_DEVICE_ID_JOYPAD_L2 : RETRO_DEVICE_ID_JOYPAD_L);
    mask |= 1u << (rightDifficulty ? RETRO_DEVICE_ID_JOYPAD_R2 : RETRO_DEVICE_ID_JOYPAD_R);
    mask |= 1u << (tvMode ? RETRO_DEVICE_ID_JOYPAD_R3 : RETRO_DEVICE_ID_JOYPAD_L3);

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
            const char *value = a26_get_core_variable(var ? var->key : NULL);
            if (!var || !value)
                return false;
            var->value = value;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
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
            return false;

        default:
            return false;
    }
}

static void video_cb(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!drawFrame || !currentUpdate || data == NULL || width < 1 || height < 1)
        return;

    RG_ASSERT(width <= updateCapacityWidth && height <= updateCapacityHeight, "Unexpected Atari 2600 frame size");
    slowFrame = !rg_display_sync(false);

    const uint8_t *source = data;
    uint8_t *dest = currentUpdate->data;
    size_t rowSize = width * sizeof(uint16_t);

    for (unsigned y = 0; y < height; ++y)
    {
        memcpy(dest + y * currentUpdate->stride, source + y * pitch, rowSize);
    }

    currentUpdate->width = width;
    currentUpdate->height = height;
    currentUpdate->offset = 0;

    rg_display_submit(currentUpdate, 0);
    lastSubmittedUpdate = currentUpdate;
    currentUpdate = updates[currentUpdate == updates[0]];
}

static void a26_audio_submit(const rg_audio_frame_t *frames, size_t count)
{
    if (!frames || count < 1)
        return;

    rg_audio_submit(frames, count);
}

static void audio_cb(int16_t left, int16_t right)
{
    rg_audio_frame_t frame = {left, right};
    a26_audio_submit(&frame, 1);
}

static size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    if (data && frames)
        a26_audio_submit((const rg_audio_frame_t *)data, frames);
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
        uint32_t mask = a26_get_joypad_mask();

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

static rg_gui_event_t left_difficulty_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT)
    {
        leftDifficulty = !leftDifficulty;
        rg_settings_set_number(NS_APP, SETTING_LEFT_DIFFICULTY, leftDifficulty);
    }

    strcpy(option->value, leftDifficulty ? "B" : "A");
    return RG_DIALOG_VOID;
}

static rg_gui_event_t right_difficulty_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT)
    {
        rightDifficulty = !rightDifficulty;
        rg_settings_set_number(NS_APP, SETTING_RIGHT_DIFFICULTY, rightDifficulty);
    }

    strcpy(option->value, rightDifficulty ? "B" : "A");
    return RG_DIALOG_VOID;
}

static rg_gui_event_t tv_mode_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT)
    {
        tvMode = !tvMode;
        rg_settings_set_number(NS_APP, SETTING_TV_MODE, tvMode);
    }

    strcpy(option->value, tvMode ? _("B/W") : _("Color"));
    return RG_DIALOG_VOID;
}

static void options_handler(rg_gui_option_t *dest)
{
    *dest++ = (rg_gui_option_t){0, _("Left difficulty"), "-", RG_DIALOG_FLAG_NORMAL, &left_difficulty_cb};
    *dest++ = (rg_gui_option_t){0, _("Right difficulty"), "-", RG_DIALOG_FLAG_NORMAL, &right_difficulty_cb};
    *dest++ = (rg_gui_option_t){0, _("TV mode"), "-", RG_DIALOG_FLAG_NORMAL, &tv_mode_cb};
    *dest++ = (rg_gui_option_t)RG_DIALOG_END;
}

void a26_main(void)
{
    const rg_handlers_t handlers = {
        .loadState = &load_state_handler,
        .saveState = &save_state_handler,
        .reset = &reset_handler,
        .screenshot = &screenshot_handler,
        .event = &event_handler,
        .options = &options_handler,
    };
    struct retro_game_info info = {0};
    struct retro_system_av_info av = {0};

    app = rg_system_reinit(A26_CORE_AUDIO_SAMPLE_RATE, &handlers, NULL);

#if RG_A26_OVERCLOCK_LEVEL > 0
    if (rg_system_get_overclock() < RG_A26_OVERCLOCK_LEVEL)
        rg_system_set_overclock(RG_A26_OVERCLOCK_LEVEL);
#endif

    leftDifficulty = rg_settings_get_number(NS_APP, SETTING_LEFT_DIFFICULTY, 0) ? 1 : 0;
    rightDifficulty = rg_settings_get_number(NS_APP, SETTING_RIGHT_DIFFICULTY, 0) ? 1 : 0;
    tvMode = rg_settings_get_number(NS_APP, SETTING_TV_MODE, 0) ? 1 : 0;

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

    updateCapacityWidth = RG_MIN((int)av.geometry.base_width ?: A26_MAX_WIDTH, A26_MAX_WIDTH);
    updateCapacityHeight = RG_MIN((int)av.geometry.max_height ?: av.geometry.base_height ?: A26_MAX_HEIGHT, A26_MAX_HEIGHT);

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

    a26_shutdown_core();
}
