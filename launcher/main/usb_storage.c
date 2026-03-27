#include "usb_storage.h"

#include <rg_system.h>

#ifdef ESP_PLATFORM
#include <esp_err.h>
#include <tinyusb.h>
#include <tusb_msc_storage.h>
#endif

#include "webui.h"

#define SETTING_WEBUI "HTTPFileServer"

static bool stop_webui_for_storage(void)
{
#ifdef RG_ENABLE_NETWORKING
    bool should_restart = rg_settings_get_number(NS_APP, SETTING_WEBUI, 0);
    if (should_restart)
        webui_stop();
    return should_restart;
#else
    return false;
#endif
}

static void restart_webui_after_storage(bool restart)
{
#ifdef RG_ENABLE_NETWORKING
    if (restart)
        webui_start();
#else
    (void)restart;
#endif
}

static void bootstrap_storage_dirs(void)
{
    rg_storage_mkdir(RG_BASE_PATH_CACHE);
    rg_storage_mkdir(RG_BASE_PATH_CONFIG);
    rg_storage_mkdir(RG_BASE_PATH_BIOS);
    rg_storage_mkdir(RG_BASE_PATH_ROMS);
    rg_storage_mkdir(RG_BASE_PATH_SAVES);
}

#if defined(ESP_PLATFORM) && defined(CONFIG_TINYUSB_MSC_ENABLED) && (defined(RG_STORAGE_SDSPI_HOST) || defined(RG_STORAGE_SDMMC_HOST))
static esp_err_t usb_storage_prepare(void *media)
{
    const tinyusb_msc_sdmmc_config_t storage_config = {
        .card = media,
        .callback_mount_changed = NULL,
        .callback_premount_changed = NULL,
        .mount_config = {
            .format_if_mount_failed = false,
            .max_files = 4,
            .allocation_unit_size = 0,
            .disk_status_check_enable = false,
            .use_one_fat = false,
        },
    };

    esp_err_t err = tinyusb_msc_storage_init_sdmmc(&storage_config);
    return err;
}

static esp_err_t usb_storage_start(void)
{
    const tinyusb_config_t tusb_config = {0};

    esp_err_t err = tinyusb_msc_storage_mount(RG_STORAGE_ROOT);
    if (err != ESP_OK)
    {
        tinyusb_msc_storage_deinit();
        return err;
    }

    err = tinyusb_driver_install(&tusb_config);
    if (err != ESP_OK)
    {
        tinyusb_msc_storage_unmount();
        tinyusb_msc_storage_deinit();
        return err;
    }

    return ESP_OK;
}

static void usb_storage_stop(void)
{
    tinyusb_driver_uninstall();
    tinyusb_msc_storage_unmount();
    tinyusb_msc_storage_deinit();
}

static void usb_storage_loop(void)
{
    while (true)
    {
        bool host_has_storage = tinyusb_msc_storage_in_use_by_usb_host();
        rg_gui_draw_message(
            "%s\n\n%s\n%s",
            host_has_storage ? _("Your computer is using the drive.") : _("USB file transfer is ready."),
            host_has_storage ? _("Eject the drive on your computer before leaving this screen.") : _("Connect USB to browse and copy files."),
            _("Press B to exit.")
        );

        for (int i = 0; i < 20; ++i)
        {
            if (rg_input_read_gamepad() == RG_KEY_B)
            {
                if (host_has_storage)
                {
                    rg_gui_alert(_("USB storage"), _("Eject the drive on your computer first."));
                    break;
                }
                return;
            }
            rg_task_delay(50);
        }
    }
}
#endif

rg_gui_event_t usb_storage_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    (void)option;

    if (event != RG_DIALOG_ENTER)
        return RG_DIALOG_VOID;

#if defined(ESP_PLATFORM) && defined(CONFIG_TINYUSB_MSC_ENABLED) && (defined(RG_STORAGE_SDSPI_HOST) || defined(RG_STORAGE_SDMMC_HOST))
    if (!rg_storage_ready())
    {
        rg_gui_alert(_("USB file transfer"), _("Storage is not mounted."));
        return RG_DIALOG_REDRAW;
    }

    if (!rg_gui_confirm(_("USB file transfer"), _("Expose storage as a USB drive?\n\nEject it on your computer before pressing B to leave this screen."), true))
        return RG_DIALOG_VOID;

    bool restart_webui = stop_webui_for_storage();
    void *media = rg_storage_get_media();
    esp_err_t err = usb_storage_prepare(media);
    if (err != ESP_OK)
    {
        restart_webui_after_storage(restart_webui);
        rg_gui_alert(_("USB file transfer"), _("Failed to prepare USB mass storage."));
        return RG_DIALOG_REDRAW;
    }

    if (!rg_storage_suspend())
    {
        tinyusb_msc_storage_deinit();
        restart_webui_after_storage(restart_webui);
        rg_gui_alert(_("USB file transfer"), _("Failed to suspend storage."));
        return RG_DIALOG_REDRAW;
    }

    err = usb_storage_start();
    if (err != ESP_OK)
    {
        rg_storage_resume();
        tinyusb_msc_storage_deinit();
        restart_webui_after_storage(restart_webui);
        rg_gui_alert(_("USB file transfer"), _("Failed to start USB mass storage."));
        return RG_DIALOG_REDRAW;
    }

    usb_storage_loop();
    usb_storage_stop();
    rg_storage_resume();
    restart_webui_after_storage(restart_webui);
    return RG_DIALOG_REDRAW;
#else
    rg_gui_alert(_("USB file transfer"), _("TinyUSB MSC is not enabled in this build."));
    return RG_DIALOG_REDRAW;
#endif
}

rg_gui_event_t format_storage_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    (void)option;

    if (event != RG_DIALOG_ENTER)
        return RG_DIALOG_VOID;

    if (!rg_storage_ready())
    {
        rg_gui_alert(_("Format FAT32 storage"), _("Storage is not mounted."));
        return RG_DIALOG_REDRAW;
    }

    if (!rg_gui_confirm(_("Format FAT32 storage"), _("This will erase all files and create a new FAT32 volume.\n\nContinue?"), false))
        return RG_DIALOG_VOID;

    bool restart_webui = stop_webui_for_storage();
    rg_gui_draw_message(_("Formatting storage..."));

    if (!rg_storage_format())
    {
        restart_webui_after_storage(restart_webui);
        rg_gui_alert(_("Format FAT32 storage"), _("Formatting failed."));
        return RG_DIALOG_REDRAW;
    }

    bootstrap_storage_dirs();
    restart_webui_after_storage(restart_webui);
    rg_gui_alert(_("Format FAT32 storage"), _("Storage formatted."));
    return RG_DIALOG_REDRAW;
}
