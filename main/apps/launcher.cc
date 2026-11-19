#include "launcher.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include <esp_log.h>
#include <esp_timer.h>

#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display/display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "material_symbols.h"
#include "music/local_player.h"
#include "english_app.h"
#include "games_app.h"
#include "ui_helpers.h"

#define TAG "Launcher"


using namespace apps;

namespace {

constexpr uint32_t kChatColor = 0x1565C0;
constexpr uint32_t kMusicColor = 0x6A1B9A;
constexpr uint32_t kGamesColor = 0x2E7D32;
constexpr uint32_t kEnglishColor = 0xEF6C00;
constexpr int kControlBar = 56;

}  // namespace

Launcher::Launcher(Display* display, LocalMusicPlayer* player, std::string music_dir,
                   AppChangedCallback on_app_changed)
    : display_(display),
      player_(player),
      music_dir_(std::move(music_dir)),
      on_app_changed_(std::move(on_app_changed)) {
    DisplayLockGuard lock(display_);
    chat_screen_ = lv_screen_active();
    BuildWelcomeScreen();
    BuildHomeScreen();
    BuildMusicScreen();
    games_ = std::make_unique<GamesApp>([this]() { Show(App::kHome); });
    english_ = std::make_unique<EnglishApp>([this]() { Show(App::kHome); },
                                            [this]() {
                                                // Practise speaking: open the chatbot, listening.
                                                Show(App::kChat);
                                                auto& app = Application::GetInstance();
                                                if (app.GetDeviceState() == kDeviceStateIdle) {
                                                    app.ToggleChatState();
                                                }
                                            });
    BuildHomeButton();
    SyncTextFont();
    lv_timer_create([](lv_timer_t* t) { static_cast<Launcher*>(lv_timer_get_user_data(t))->Tick(); },
                    150, this);
    Show(App::kWelcome);
}

Launcher::~Launcher() = default;

void Launcher::BuildWelcomeScreen() {
    welcome_screen_ = MakeScreen();

    // A friendly robot face: a rounded head with two eyes that blink and a smile.
    lv_obj_t* head = lv_obj_create(welcome_screen_);
    PlainBox(head);
    lv_obj_set_size(head, 136, 100);
    lv_obj_set_style_radius(head, 30, 0);
    lv_obj_set_style_bg_color(head, lv_color_hex(kChatColor), 0);
    lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 34);

    lv_obj_t* antenna = lv_obj_create(welcome_screen_);
    PlainBox(antenna);
    lv_obj_set_size(antenna, 16, 16);
    lv_obj_set_style_radius(antenna, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(antenna, lv_color_hex(0xFFB300), 0);
    lv_obj_align_to(antenna, head, LV_ALIGN_OUT_TOP_MID, 0, -4);

    for (int i = 0; i < 2; ++i) {
        lv_obj_t* eye = lv_obj_create(head);
        PlainBox(eye);
        lv_obj_set_size(eye, 26, 34);
        lv_obj_set_style_radius(eye, 13, 0);
        lv_obj_set_style_bg_color(eye, lv_color_white(), 0);
        lv_obj_align(eye, LV_ALIGN_CENTER, i == 0 ? -30 : 30, -10);
        welcome_eyes_[i] = eye;
    }

    lv_obj_t* mouth = lv_obj_create(head);
    PlainBox(mouth);
    lv_obj_set_size(mouth, 44, 10);
    lv_obj_set_style_radius(mouth, 5, 0);
    lv_obj_set_style_bg_color(mouth, lv_color_white(), 0);
    lv_obj_align(mouth, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t* hello = lv_label_create(welcome_screen_);
    lv_label_set_text(hello, Lang::Strings::HELLO_MY_FRIEND);
    lv_obj_align(hello, LV_ALIGN_TOP_MID, 0, 152);

    welcome_status_ = lv_label_create(welcome_screen_);
    lv_obj_set_style_text_color(welcome_status_, lv_color_hex(0x9AA5AE), 0);
    lv_label_set_text(welcome_status_, Lang::Strings::PLEASE_WAIT);
    lv_obj_align(welcome_status_, LV_ALIGN_TOP_MID, 0, 186);
}

void Launcher::BuildHomeScreen() {
    home_screen_ = MakeScreen();

    struct Tile {
        const char* icon;
        const char* name;
        uint32_t color;
        App app;
    };
    const Tile tiles[] = {
        {MATERIAL_SYMBOLS_MIC, "Chatbot", kChatColor, App::kChat},
        {MATERIAL_SYMBOLS_MUSIC_NOTE, "Music", kMusicColor, App::kMusic},
        {MATERIAL_SYMBOLS_SPORTS_ESPORTS, "Games", kGamesColor, App::kGames},
        {MATERIAL_SYMBOLS_LANGUAGE, "English", kEnglishColor, App::kEnglish},
    };
    // A 2x2 grid under the volume button and the battery status.
    const int tile_w = 144, tile_h = 88, gap = 8, top = 48;
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* tile = lv_button_create(home_screen_);
        lv_obj_set_size(tile, tile_w, tile_h);
        lv_obj_set_pos(tile, 12 + (i % 2) * (tile_w + gap), top + (i / 2) * (tile_h + gap));
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_set_style_radius(tile, 18, 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(tiles[i].color), 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(tiles[i].color), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(tile, LV_OPA_80, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(tile, 0, 0);

        lv_obj_t* icon = lv_label_create(tile);
        lv_obj_set_style_text_font(icon, &font_material_symbols_30_4, 0);
        lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        lv_label_set_text(icon, tiles[i].icon);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, -14);

        lv_obj_t* name = lv_label_create(tile);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_label_set_text(name, tiles[i].name);
        lv_obj_align(name, LV_ALIGN_CENTER, 0, 22);

        const App app = tiles[i].app;
        BindClick(tile, [this, app]() { Show(app); });
    }
    BuildVolumePanel();
}

void Launcher::BuildVolumePanel() {
    // The button: top-left, clear of the battery status on the right.
    volume_button_ = MakeIconButton(home_screen_, MATERIAL_SYMBOLS_VOLUME_UP, 108, 40,
                                    [this]() { ShowVolumePanel(true); });
    lv_obj_align(volume_button_, LV_ALIGN_TOP_LEFT, 12, 4);
    lv_obj_t* icon = lv_obj_get_child(volume_button_, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
    volume_button_label_ = lv_label_create(volume_button_);
    lv_obj_set_style_text_color(volume_button_label_, lv_color_white(), 0);
    lv_obj_align(volume_button_label_, LV_ALIGN_LEFT_MID, 46, 0);
    lv_obj_set_ext_click_area(volume_button_, 4);

    // The panel: over the whole home screen, so taps outside it close it.
    volume_panel_ = lv_obj_create(home_screen_);
    PlainBox(volume_panel_);
    lv_obj_set_size(volume_panel_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(volume_panel_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(volume_panel_, LV_OPA_60, 0);
    lv_obj_add_flag(volume_panel_, LV_OBJ_FLAG_HIDDEN);
    BindClick(volume_panel_, [this]() { ShowVolumePanel(false); });

    lv_obj_t* card = lv_obj_create(volume_panel_);
    PlainBox(card);
    lv_obj_set_size(card, 288, 196);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(kSurface), 0);
    lv_obj_center(card);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);  // Taps on the card keep it open

    lv_obj_t* title = lv_label_create(card);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Âm lượng");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 14);
    volume_value_label_ = lv_label_create(card);
    lv_obj_set_style_text_color(volume_value_label_, lv_color_white(), 0);
    lv_obj_align(volume_value_label_, LV_ALIGN_TOP_RIGHT, -16, 14);

    // Drag the slider, or step with the buttons below it.
    volume_slider_ = lv_slider_create(card);
    lv_slider_set_range(volume_slider_, 0, 100);
    lv_obj_set_size(volume_slider_, 236, 16);
    lv_obj_align(volume_slider_, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_ext_click_area(volume_slider_, 18);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(kAccent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_white(), LV_PART_KNOB);
    // While dragging only the number follows: each set is saved to flash.
    BindEvent(volume_slider_, LV_EVENT_VALUE_CHANGED, [this]() {
        lv_label_set_text_fmt(volume_value_label_, "%d%%", static_cast<int>(lv_slider_get_value(volume_slider_)));
    });
    // Set it, with one beep at the new level, when the finger lifts.
    BindEvent(volume_slider_, LV_EVENT_RELEASED, [this]() {
        SetVolume(static_cast<int>(lv_slider_get_value(volume_slider_)), true);
        PlayVolumeBeep();
    });

    const int bw = 76, bh = 56;
    lv_obj_t* down = MakeIconButton(card, MATERIAL_SYMBOLS_VOLUME_DOWN, bw, bh, [this]() {
        ChangeVolume(-10);
        PlayVolumeBeep();
    });
    lv_obj_align(down, LV_ALIGN_BOTTOM_LEFT, 14, -14);
    lv_obj_t* up = MakeIconButton(card, MATERIAL_SYMBOLS_VOLUME_UP, bw, bh, [this]() {
        ChangeVolume(10);
        PlayVolumeBeep();
    });
    lv_obj_align(up, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_t* done = MakeIconButton(card, MATERIAL_SYMBOLS_CHECK, bw, bh,
                                    [this]() { ShowVolumePanel(false); });
    lv_obj_set_style_bg_color(done, lv_color_hex(kAccent), 0);
    lv_obj_align(done, LV_ALIGN_BOTTOM_RIGHT, -14, -14);

    UpdateVolumeUi();
}

void Launcher::ShowVolumePanel(bool show) {
    if (show) {
        UpdateVolumeUi();
        lv_obj_remove_flag(volume_panel_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(volume_panel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Launcher::PlayVolumeBeep() {
    // Lets a child hear how loud it is now. From the main task: the sound
    // queue may block. Not during a conversation.
    auto& app = Application::GetInstance();
    app.Schedule([&app]() {
        if (app.GetDeviceState() == kDeviceStateIdle) app.PlaySound(Lang::Sounds::OGG_POPUP);
    });
}

void Launcher::UpdateVolumeUi() {
    const int volume = Board::GetInstance().GetAudioCodec()->output_volume();
    if (volume == shown_volume_ || lv_obj_has_state(volume_slider_, LV_STATE_PRESSED)) return;
    shown_volume_ = volume;
    lv_label_set_text_fmt(volume_button_label_, "%d%%", volume);
    lv_label_set_text_fmt(volume_value_label_, "%d%%", volume);
    lv_slider_set_value(volume_slider_, volume, LV_ANIM_OFF);
}

void Launcher::BuildMusicScreen() {
    music_screen_ = MakeScreen();
    const int width = lv_display_get_horizontal_resolution(nullptr);
    const int height = lv_display_get_vertical_resolution(nullptr);

    // Top bar: home, now playing, volume.
    lv_obj_t* bar = MakeTopBar(music_screen_, MATERIAL_SYMBOLS_HOME, [this]() { Show(App::kHome); });

    title_label_ = lv_label_create(bar);
    lv_obj_set_width(title_label_, width - kTopBarTitleLeft - kRightReserve - 56);
    lv_label_set_long_mode(title_label_, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_text(title_label_, "Music");
    lv_obj_align(title_label_, LV_ALIGN_LEFT_MID, kTopBarTitleLeft, 0);

    volume_label_ = lv_label_create(bar);
    lv_obj_align(volume_label_, LV_ALIGN_RIGHT_MID, -kRightReserve, 0);

    // Track list: a scrolling column of buttons (lv_list is not enabled).
    track_list_ = lv_obj_create(music_screen_);
    lv_obj_set_size(track_list_, width, height - kTopBar - kControlBar);
    lv_obj_align(track_list_, LV_ALIGN_TOP_MID, 0, kTopBar);
    lv_obj_set_style_bg_color(track_list_, lv_color_hex(kBackground), 0);
    lv_obj_set_style_border_width(track_list_, 0, 0);
    lv_obj_set_style_radius(track_list_, 0, 0);
    lv_obj_set_style_pad_all(track_list_, 4, 0);
    lv_obj_set_style_pad_row(track_list_, 2, 0);
    lv_obj_set_flex_flow(track_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(track_list_, LV_DIR_VER);

    empty_label_ = lv_label_create(music_screen_);
    lv_obj_set_style_text_align(empty_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(empty_label_, LV_ALIGN_CENTER, 0, (kTopBar - kControlBar) / 2);
    lv_obj_add_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);

    // Controls: previous, play/pause, next, volume down, volume up.
    lv_obj_t* controls = lv_obj_create(music_screen_);
    PlainBox(controls);
    lv_obj_set_size(controls, width, kControlBar);
    lv_obj_set_style_bg_color(controls, lv_color_hex(kSurface), 0);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    const int bw = 54, bh = 44;
    MakeIconButton(controls, MATERIAL_SYMBOLS_SKIP_PREVIOUS, bw, bh, [this]() {
        if (player_) player_->Previous();
    });
    lv_obj_t* play = MakeIconButton(controls, MATERIAL_SYMBOLS_PLAY_ARROW, bw, bh, [this]() {
        if (player_) player_->TogglePause();
    });
    play_icon_ = lv_obj_get_child(play, 0);
    MakeIconButton(controls, MATERIAL_SYMBOLS_SKIP_NEXT, bw, bh, [this]() {
        if (player_) player_->Next();
    });
    MakeIconButton(controls, MATERIAL_SYMBOLS_VOLUME_DOWN, bw, bh, [this]() { ChangeVolume(-10); });
    MakeIconButton(controls, MATERIAL_SYMBOLS_VOLUME_UP, bw, bh, [this]() { ChangeVolume(10); });
}

void Launcher::BuildHomeButton() {
    // Over the chatbot's full-screen UI, so it lives on the top layer and is
    // only shown while the chatbot app is open.
    // A corner is hard to hit, so touches count all the way to the screen edge.
    constexpr int kSize = 64;
    constexpr int kMargin = 12;
    home_button_ = MakeIconButton(lv_layer_top(), MATERIAL_SYMBOLS_HOME, kSize, kSize,
                                  [this]() { LeaveChat(); });
    lv_obj_set_style_radius(home_button_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(home_button_, LV_OPA_70, 0);
    lv_obj_set_ext_click_area(home_button_, kMargin);
    lv_obj_align(home_button_, LV_ALIGN_BOTTOM_LEFT, kMargin, -kMargin);
}

void Launcher::Show(App app) {
    // The chatbot must not talk over the music or a game, and a wake word heard
    // mid-game would pull the player out of it. It also frees CPU for both.
    const auto owns_speaker = [](App a) { return a == App::kMusic || a == App::kGames; };
    if (current_ == App::kMusic && app != App::kMusic && player_) {
        player_->Stop();
    }
    if (current_ == App::kGames && app != App::kGames) {
        games_->OnHide();
    }
    // Practising with the chatbot keeps the topic open for coming back.
    if (current_ == App::kEnglish && app != App::kEnglish && app != App::kChat) {
        english_->OnHide();
    }
    if (owns_speaker(app) != owns_speaker(current_)) {
        Application::GetInstance().SetChatSuspended(owns_speaker(app));
    }
    if (app == App::kMusic && current_ != App::kMusic) {
        ReloadTracks();
    }

    current_ = app;
    lv_obj_t* screen = chat_screen_;
    if (app == App::kHome) {
        screen = home_screen_;
        ShowVolumePanel(false);
    }
    if (app == App::kWelcome) screen = welcome_screen_;
    if (app == App::kMusic) screen = music_screen_;
    if (app == App::kGames) screen = games_->screen();
    if (app == App::kEnglish) screen = english_->screen();
    lv_screen_load(screen);
    if (app == App::kChat) {
        lv_obj_remove_flag(home_button_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(home_button_, LV_OBJ_FLAG_HIDDEN);
    }
    shown_revision_ = UINT32_MAX;
    RefreshPlayer();
    ESP_LOGI(TAG, "App: %s", app == App::kWelcome ? "welcome"
                            : app == App::kHome  ? "home"
                            : app == App::kMusic ? "music"
                            : app == App::kGames ? "games"
                            : app == App::kEnglish ? "english"
                                                 : "chat");
    if (on_app_changed_) on_app_changed_(app);
}

bool Launcher::HandleBootButton() {
    if (current_ == App::kGames) {
        return true;  // Nothing to do: the chatbot is paused, and a game plays on
    }
    if (current_ != App::kMusic || player_ == nullptr) {
        return false;
    }
    player_->TogglePause();
    return true;
}

void Launcher::ReloadTracks() {
    lv_obj_clean(track_list_);
    size_t count = 0;
    if (player_ != nullptr && !music_dir_.empty()) {
        count = player_->Scan(music_dir_);
    }
    for (size_t i = 0; i < count; ++i) {
        lv_obj_t* item = lv_button_create(track_list_);
        lv_obj_set_width(item, lv_pct(100));
        lv_obj_set_height(item, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(item, 8, 0);
        lv_obj_set_style_pad_hor(item, 10, 0);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_shadow_width(item, 0, 0);
        lv_obj_set_style_bg_color(item, lv_color_hex(kBackground), 0);
        lv_obj_t* name = lv_label_create(item);
        lv_obj_set_width(name, lv_pct(100));
        lv_label_set_text(name, player_->TrackName(i).c_str());
        lv_obj_set_style_bg_color(item, lv_color_hex(kAccent), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(item, lv_color_white(), 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        BindClick(item, [this, i]() {
            if (player_) player_->Play(i);
        });
    }

    if (count == 0) {
        lv_label_set_text(empty_label_, music_dir_.empty() ? "No SD card" : "No music on the SD card\n(MP3, M4A, AAC, FLAC, WAV)");
        lv_obj_remove_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Launcher::ChangeVolume(int delta) {
    SetVolume(Board::GetInstance().GetAudioCodec()->output_volume() + delta, true);
}

void Launcher::SetVolume(int volume, bool round) {
    auto* codec = Board::GetInstance().GetAudioCodec();
    volume = std::clamp(volume, 0, 100);
    if (round) volume = (volume + 2) / 5 * 5;  // Whole steps of five
    if (volume == codec->output_volume()) return;
    codec->SetOutputVolume(volume);  // Saved, so it is kept after a restart
    shown_revision_ = UINT32_MAX;
    RefreshPlayer();
    UpdateVolumeUi();
}

void Launcher::RefreshPlayer() {
    if (current_ != App::kMusic) return;

    char volume[8];
    snprintf(volume, sizeof(volume), "%d%%", Board::GetInstance().GetAudioCodec()->output_volume());
    lv_label_set_text(volume_label_, volume);

    if (player_ == nullptr) return;
    uint32_t revision = player_->Revision();
    if (revision == shown_revision_) return;
    shown_revision_ = revision;

    auto state = player_->GetState();
    int current = player_->CurrentIndex();
    lv_label_set_text(play_icon_, state == LocalMusicPlayer::State::kPlaying ? MATERIAL_SYMBOLS_PAUSE
                                                                             : MATERIAL_SYMBOLS_PLAY_ARROW);
    std::string title = current >= 0 && state != LocalMusicPlayer::State::kStopped
                            ? player_->TrackName(current)
                            : "Music";
    lv_label_set_text(title_label_, title.c_str());

    uint32_t children = lv_obj_get_child_count(track_list_);
    for (uint32_t i = 0; i < children; ++i) {
        lv_obj_t* item = lv_obj_get_child(track_list_, i);
        if (static_cast<int>(i) == current && state != LocalMusicPlayer::State::kStopped) {
            lv_obj_add_state(item, LV_STATE_CHECKED);
            lv_obj_scroll_to_view(item, LV_ANIM_ON);
        } else {
            lv_obj_remove_state(item, LV_STATE_CHECKED);
        }
    }
}

void Launcher::SyncTextFont() {
    // The theme font covers the build language (e.g. Vietnamese song names) and is
    // replaced when the assets load. Labels inherit it from their screen.
    auto* theme = static_cast<LvglTheme*>(display_->GetTheme());
    auto font = theme != nullptr ? theme->text_font() : nullptr;
    if (font == nullptr || font == text_font_) return;
    for (lv_obj_t* screen : {welcome_screen_, home_screen_, music_screen_, games_->screen(),
                               english_->screen()}) {
        lv_obj_set_style_text_font(screen, font->font(), 0);
    }
    text_font_ = std::move(font);
}

bool Launcher::ChatNeeded() {
    auto state = Application::GetInstance().GetDeviceState();
    // Every boot passes through activating for a few seconds; staying there
    // means the device is waiting for its activation code to be entered,
    // which the chatbot screen shows.
    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (state != kDeviceStateActivating) {
        activating_since_ms_ = 0;
    } else if (activating_since_ms_ == 0) {
        activating_since_ms_ = now_ms;
    }
    bool waiting_for_code = activating_since_ms_ != 0 && now_ms - activating_since_ms_ > 5000;
    // A conversation, server playback, WiFi setup or upgrade started: show it.
    bool busy = state != kDeviceStateIdle && state != kDeviceStateUnknown &&
                state != kDeviceStateStarting && state != kDeviceStateActivating;
    // Only when it starts: a user who left the chatbot mid-conversation (the
    // home button) is not pulled straight back while it winds down.
    const bool needed = busy || waiting_for_code;
    const bool started = needed && !chat_was_needed_;
    chat_was_needed_ = needed;
    return started;
}

void Launcher::LeaveChat() {
    // The home button ends the conversation: the user is leaving it.
    auto& app = Application::GetInstance();
    switch (app.GetDeviceState()) {
        case kDeviceStateListening:
            app.ToggleChatState();  // Closes the audio channel
            break;
        case kDeviceStateSpeaking:
            // Stop the reply (from the main task, which owns the protocol); it
            // then listens, and Tick() closes that.
            app.Schedule([&app]() { app.AbortSpeaking(kAbortReasonNone); });
            end_chat_pending_ = true;
            break;
        case kDeviceStateConnecting:
            end_chat_pending_ = true;  // Tick() closes it once it listens
            break;
        default:
            break;
    }
    Show(App::kHome);
}

void Launcher::TickWelcome(bool chat_started) {
    if (chat_started) {
        Show(App::kChat);
        return;
    }
    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
        idle_since_ms_ = 0;
    } else if (idle_since_ms_ == 0) {
        idle_since_ms_ = now_ms;
    } else if (now_ms - idle_since_ms_ >= kWelcomeIdleMs) {
        Show(App::kHome);
        return;
    }

    // Blink about every 3 s, and animate the dots after "please wait".
    ++welcome_ticks_;
    const int eye_height = welcome_ticks_ % 20 == 0 ? 6 : 34;
    for (lv_obj_t* eye : welcome_eyes_) {
        lv_obj_set_height(eye, eye_height);
    }
    if (welcome_ticks_ % 3 == 0) {
        std::string text = Lang::Strings::PLEASE_WAIT;
        while (!text.empty() && text.back() == '.') text.pop_back();
        text.append((welcome_ticks_ / 3) % 4, '.');
        lv_label_set_text(welcome_status_, text.c_str());
    }
}

void Launcher::Tick() {
    SyncTextFont();
    // Evaluated on every tick so that starts are seen as they happen.
    const bool chat_started = ChatNeeded();
    if (end_chat_pending_) {
        auto state = Application::GetInstance().GetDeviceState();
        if (state == kDeviceStateListening) {
            Application::GetInstance().ToggleChatState();
            end_chat_pending_ = false;
        } else if (state != kDeviceStateSpeaking && state != kDeviceStateConnecting) {
            end_chat_pending_ = false;
        }
    }
    if (current_ == App::kWelcome) {
        TickWelcome(chat_started);
    }
    if (current_ == App::kHome) {
        // The chatbot can change the volume too ("nói to lên").
        UpdateVolumeUi();
    }
    if (current_ == App::kHome || current_ == App::kEnglish) {
        // A wake word or BOOT starts a conversation: show it.
        if (chat_started && !end_chat_pending_) {
            Show(App::kChat);
        }
    } else if (current_ == App::kMusic) {
        RefreshPlayer();
    }
}
