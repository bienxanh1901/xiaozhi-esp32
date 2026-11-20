#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <wifi_station.h>
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "display/eye_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "adc_battery_monitor.h"
#include "apps/launcher.h"
#include "music/local_player.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include "led/single_led.h"
#include "system_reset.h"
#include "esp_lcd_ili9341.h"
#include "material_symbols.h"

LV_FONT_DECLARE(font_material_symbols_30_4);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

#define TAG "LcdwikiEs3c28p"

class TouchDriver {
public:
    TouchDriver() : dev_(nullptr) {}

    bool Init(i2c_master_bus_handle_t bus, uint8_t addr) {
        i2c_device_config_t cfg = {
            .device_address = addr,
            .scl_speed_hz = 400000,
            .scl_wait_us = 0,
        };
        return i2c_master_bus_add_device(bus, &cfg, &dev_) == ESP_OK;
    }

    bool Read(bool &touched, uint16_t &x, uint16_t &y) {
        touched = false;
        x = y = 0;
        if (!dev_) return false;

        uint8_t reg = 0x02;
        uint8_t buf[5];
        if (i2c_master_transmit_receive(dev_, &reg, 1, buf, 5, 50) != ESP_OK) return false;

        uint8_t points = buf[0] & 0x0F;
        if (points == 0) return true;

        touched = true;
        x = ((buf[1] & 0x0F) << 8) | buf[2];
        y = ((buf[3] & 0x0F) << 8) | buf[4];
        return true;
    }

private:
    i2c_master_dev_handle_t dev_;
};

class LcdwikiEs3c28pBoard : public WifiBoard {
private:
    Button boot_button_;
    EyeDisplay *display_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    TouchDriver touch_;
    AdcBatteryMonitor* adc_battery_monitor_;
    lv_obj_t* talk_button_ = nullptr;
    lv_obj_t* talk_icon_ = nullptr;
    lv_indev_t* touch_indev_ = nullptr;
    bool touch_down_ = false;
    int64_t touch_down_ms_ = 0;
    bool sd_mounted_ = false;
    LocalMusicPlayer* music_player_ = nullptr;
    Launcher* launcher_ = nullptr;
    lv_obj_t* boot_cover_ = nullptr;  // Black until the launcher is up
    DeviceState talk_button_state_ = kDeviceStateUnknown;
    lv_obj_t* battery_icon_ = nullptr;
    lv_obj_t* battery_text_ = nullptr;
    esp_timer_handle_t battery_timer_ = nullptr;
    int logged_level_ = -1;
    bool logged_charging_ = false;

    // The status bar's battery icon sits under the full-screen eye face, so
    // show the level in the top-right corner, above the face.
    void InitializeBatteryIndicator() {
        {
            DisplayLockGuard lock(display_);
            lv_obj_t* box = lv_obj_create(lv_layer_top());
            lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(box, 0, 0);
            lv_obj_set_style_pad_all(box, 0, 0);
            lv_obj_set_style_pad_column(box, 4, 0);
            lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(box, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_align(box, LV_ALIGN_TOP_RIGHT, -8, 6);

            battery_text_ = lv_label_create(box);
            lv_obj_set_style_text_font(battery_text_, &BUILTIN_TEXT_FONT, 0);
            lv_label_set_text(battery_text_, "");
            battery_icon_ = lv_label_create(box);
            lv_obj_set_style_text_font(battery_icon_, &BUILTIN_ICON_FONT, 0);
            lv_label_set_text(battery_icon_, "");
        }

        esp_timer_create_args_t args = {};
        args.callback = [](void* arg) { static_cast<LcdwikiEs3c28pBoard*>(arg)->UpdateBatteryIndicator(); };
        args.arg = this;
        args.name = "battery_ui";
        ESP_ERROR_CHECK(esp_timer_create(&args, &battery_timer_));
        // The first estimate needs a few ADC samples; refresh every 10 s.
        ESP_ERROR_CHECK(esp_timer_start_periodic(battery_timer_, 10 * 1000 * 1000));
    }

    void UpdateBatteryIndicator() {
        int level;
        bool charging, discharging;
        if (!GetBatteryLevel(level, charging, discharging)) return;
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        // Logged on change, so charging can be checked over USB.
        if (level != logged_level_ || charging != logged_charging_) {
            logged_level_ = level;
            logged_charging_ = charging;
            ESP_LOGI(TAG, "Battery %d%%%s", level, charging ? ", charging" : "");
        }

        static const char* const kLevels[] = {
            MATERIAL_SYMBOLS_BATTERY_ANDROID_0,       MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_1,
            MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_2, MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_3,
            MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_4, MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_5,
            MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_6, MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_FULL,
        };
        const char* icon;
        lv_color_t color;
        if (charging) {
            icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_BOLT;
            color = lv_color_hex(0x66BB6A);
        } else {
            // Same buckets as the standard status bar.
            int index = level <= 0 ? 0 : (level >= 100 ? 7 : 1 + ((level - 1) * 6 / 99));
            icon = kLevels[index];
            color = level <= 15 ? lv_color_hex(0xEF5350) : lv_color_white();
        }

        char text[8];
        snprintf(text, sizeof(text), "%d%%", level);
        DisplayLockGuard lock(display_);
        lv_label_set_text(battery_icon_, icon);
        lv_label_set_text(battery_text_, text);
        lv_obj_set_style_text_color(battery_icon_, color, 0);
        lv_obj_set_style_text_color(battery_text_, color, 0);
    }

    // The touch panel reports portrait coordinates; the screen runs landscape.
    static void TouchToScreen(uint16_t raw_x, uint16_t raw_y, int &sx, int &sy) {
        sx = TOUCH_SWAP_XY ? raw_y : raw_x;
        sy = TOUCH_SWAP_XY ? raw_x : raw_y;
        if (TOUCH_MIRROR_X) sx = DISPLAY_WIDTH - 1 - sx;
        if (TOUCH_MIRROR_Y) sy = DISPLAY_HEIGHT - 1 - sy;
    }

    // An on-screen talk button over the eye face, on the top layer. It does
    // what BOOT does; its look follows the device state (see UpdateTalkButton).
    // Call with the display lock held.
    void InitializeTalkButton() {
        talk_button_ = lv_obj_create(lv_layer_top());
        lv_obj_remove_flag(talk_button_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(talk_button_, [](lv_event_t*) {
            Application::GetInstance().ToggleChatState();
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_set_style_transform_pivot_x(talk_button_, TALK_BUTTON_SIZE / 2, 0);
        lv_obj_set_style_transform_pivot_y(talk_button_, TALK_BUTTON_SIZE / 2, 0);
        lv_obj_set_style_transform_scale(talk_button_, 230, LV_STATE_PRESSED);
        lv_obj_set_size(talk_button_, TALK_BUTTON_SIZE, TALK_BUTTON_SIZE);
        lv_obj_set_style_radius(talk_button_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(talk_button_, 2, 0);
        lv_obj_set_style_border_color(talk_button_, lv_color_white(), 0);
        lv_obj_set_style_border_opa(talk_button_, LV_OPA_50, 0);
        lv_obj_set_style_pad_all(talk_button_, 0, 0);
        lv_obj_align(talk_button_, LV_ALIGN_BOTTOM_RIGHT, -TALK_BUTTON_MARGIN, -TALK_BUTTON_MARGIN);

        talk_icon_ = lv_label_create(talk_button_);
        lv_obj_set_style_text_font(talk_icon_, &font_material_symbols_30_4, 0);
        lv_obj_set_style_text_color(talk_icon_, lv_color_white(), 0);
        lv_label_set_text(talk_icon_, MATERIAL_SYMBOLS_MIC);
        lv_obj_center(talk_icon_);
        UpdateTalkButton(kDeviceStateIdle);
        // Follow the device state from the LVGL task, which holds the lock.
        lv_timer_create([](lv_timer_t* t) {
            auto* self = static_cast<LcdwikiEs3c28pBoard*>(lv_timer_get_user_data(t));
            self->UpdateTalkButton(Application::GetInstance().GetDeviceState());
        }, 100, this);
    }

    // The button shows what a tap will do: blue mic to talk, green while
    // listening, orange while the bot speaks (a tap interrupts it), grey when
    // busy. While a song or story plays it becomes a red stop button; the tap
    // takes the same path as BOOT, which ends playback and returns to idle.
    void UpdateTalkButton(DeviceState state) {
        if (talk_button_ == nullptr || state == talk_button_state_) return;
        talk_button_state_ = state;
        uint32_t color;
        const char* icon = MATERIAL_SYMBOLS_MIC;
        switch (state) {
            case kDeviceStateListening: color = 0x2E7D32; break;
            case kDeviceStateSpeaking: color = 0xEF6C00; break;
            case kDeviceStateNotifying:
                color = 0xC62828;
                icon = MATERIAL_SYMBOLS_STOP;
                break;
            case kDeviceStateIdle: color = 0x1565C0; break;
            default: color = 0x546E7A; break;
        }
        lv_obj_set_style_bg_color(talk_button_, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(talk_button_, LV_OPA_COVER, 0);
        lv_label_set_text(talk_icon_, icon);
    }

    void InitializeBatteryMonitor() {
        adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_8, 200000, 200000, GPIO_NUM_NC);
    }

    // LVGL pointer input from the FT6336G. Runs in the LVGL task.
    static void TouchRead(lv_indev_t* indev, lv_indev_data_t* data) {
        auto* self = static_cast<LcdwikiEs3c28pBoard*>(lv_indev_get_user_data(indev));
        bool touched;
        uint16_t x, y;
        if (!self->touch_.Read(touched, x, y)) touched = false;

        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (touched) {
            int sx, sy;
            TouchToScreen(x, y, sx, sy);
            data->point.x = sx;
            data->point.y = sy;
            data->state = LV_INDEV_STATE_PRESSED;
            if (!self->touch_down_) {
                self->touch_down_ = true;
                self->touch_down_ms_ = now_ms;
                ESP_LOGI(TAG, "Touch raw=(%u,%u) screen=(%d,%d)", x, y, sx, sy);
            }
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
            if (self->touch_down_) {
                self->touch_down_ = false;
                // Hold for 3 s in the chatbot: WiFi configuration. Not in the
                // other apps, where a long press is just a slow tap or a scroll.
                bool in_chat = self->launcher_ == nullptr ||
                               self->launcher_->current() == Launcher::App::kChat;
                if (in_chat && now_ms - self->touch_down_ms_ > 3000) {
                    Application::GetInstance().Schedule([self]() { self->EnterWifiConfigMode(); });
                }
            }
        }
    }

    void InitializeTouch() {
        // The ES3N28P variant has no touch panel.
        if (i2c_master_probe(codec_i2c_bus_, TOUCH_I2C_ADDR, 100) != ESP_OK) {
            ESP_LOGI(TAG, "No touch panel found");
            return;
        }
        if (!touch_.Init(codec_i2c_bus_, TOUCH_I2C_ADDR)) return;
        {
            DisplayLockGuard lock(display_);
            touch_indev_ = lv_indev_create();
            lv_indev_set_type(touch_indev_, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(touch_indev_, TouchRead);
            lv_indev_set_user_data(touch_indev_, this);
            InitializeTalkButton();
            // Hide the chatbot UI (robot eyes) that the app shows first, until
            // the launcher's welcome screen replaces it.
            boot_cover_ = lv_obj_create(lv_layer_sys());
            lv_obj_remove_style_all(boot_cover_);
            lv_obj_set_size(boot_cover_, LV_PCT(100), LV_PCT(100));
            lv_obj_set_style_bg_color(boot_cover_, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(boot_cover_, LV_OPA_COVER, 0);
        }
        // The launcher needs the display's UI, which the app builds after the
        // board, so create it from the main loop.
        Application::GetInstance().Schedule([this]() { InitializeLauncher(); });
    }

    void InitializeSdCard() {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_1;
        host.max_freq_khz = SDMMC_FREQ_DEFAULT;

        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 4;
        slot.clk = SD_CLK_PIN;
        slot.cmd = SD_CMD_PIN;
        slot.d0 = SD_D0_PIN;
        slot.d1 = SD_D1_PIN;
        slot.d2 = SD_D2_PIN;
        slot.d3 = SD_D3_PIN;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
        mount_config.format_if_mount_failed = false;
        mount_config.max_files = 4;
        mount_config.allocation_unit_size = 16 * 1024;

        sdmmc_card_t* card = nullptr;
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_config, &card);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "No SD card mounted: %s", esp_err_to_name(ret));
            return;
        }
        sd_mounted_ = true;
        ESP_LOGI(TAG, "SD card mounted at %s (%llu MB)", SD_MOUNT_POINT,
                 static_cast<unsigned long long>(card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    }

    void InitializeLauncher() {
        auto& app = Application::GetInstance();
        music_player_ = new LocalMusicPlayer(app.GetAudioService(), GetAudioCodec()->output_sample_rate());
        launcher_ = new Launcher(display_, music_player_, sd_mounted_ ? SD_MOUNT_POINT : "",
                                 [this](Launcher::App current) {
                                     // The talk button belongs to the chatbot app.
                                     if (current == Launcher::App::kChat) {
                                         lv_obj_remove_flag(talk_button_, LV_OBJ_FLAG_HIDDEN);
                                     } else {
                                         lv_obj_add_flag(talk_button_, LV_OBJ_FLAG_HIDDEN);
                                     }
                                 });
        DisplayLockGuard lock(display_);
        lv_obj_delete(boot_cover_);
        boot_cover_ = nullptr;
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = AUDIO_CODEC_I2C_NUM,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MIS0_PIN;
        buscfg.sclk_io_num = DISPLAY_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (launcher_ != nullptr && launcher_->HandleBootButton()) {
                return;  // play/pause in the music app
            }
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        ESP_LOGI(TAG, "Install LCD driver ILI9341");
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new EyeDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTools() {
    }

public:
    LcdwikiEs3c28pBoard(): boot_button_(BOOT_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeBatteryMonitor();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeSdCard();
        InitializeTouch();
        InitializeBatteryIndicator();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    virtual Led *GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, AUDIO_CODEC_I2C_NUM,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR, true, true);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        charging = adc_battery_monitor_->IsCharging();
        discharging = adc_battery_monitor_->IsDischarging();
        level = adc_battery_monitor_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(LcdwikiEs3c28pBoard);
