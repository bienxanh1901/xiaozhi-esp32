#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <lvgl.h>

class Display;
class LocalMusicPlayer;
class LvglFont;
namespace apps {
class EnglishApp;
class GamesApp;
}

/**
 * At power on a welcome screen shows until startup is done, then the home
 * screen opens (or the chatbot, when activation or WiFi setup needs it).
 * A touch home screen with four apps: the chatbot (the display's existing UI),
 * a music player for files on the SD card, games, and English for children.
 *
 * Opt-in for boards with a touch-enabled LVGL display. Create it after the
 * display's SetupUI() has run (e.g. from Application::Schedule), from any task:
 * the constructor takes the display lock. All other methods run in the LVGL
 * context (event callbacks, LVGL timers) or with the display lock held.
 *
 * While Music or Games is open the chatbot is suspended so they never share the
 * speaker; leaving the app stops it and resumes the chatbot. On the
 * home screen a conversation (wake word, BOOT) switches to the chatbot app.
 */
class Launcher {
public:
    enum class App { kHome, kChat, kMusic, kGames, kWelcome, kEnglish };
    using AppChangedCallback = std::function<void(App)>;

    // `player` may be null, and `music_dir` empty, when there is no SD card.
    Launcher(Display* display, LocalMusicPlayer* player, std::string music_dir,
             AppChangedCallback on_app_changed);
    ~Launcher();

    void Show(App app);
    App current() const { return current_; }

    // BOOT button: play/pause in the Music app. Returns false when the press
    // should go to the chatbot instead.
    bool HandleBootButton();

private:
    Display* display_;
    LocalMusicPlayer* player_;
    std::string music_dir_;
    AppChangedCallback on_app_changed_;
    App current_ = App::kChat;
    // LVGL styles keep raw font pointers: holding the owner keeps the bound font
    // alive after the display replaces it, until SyncTextFont() has rebound it.
    std::shared_ptr<LvglFont> text_font_;

    lv_obj_t* chat_screen_ = nullptr;
    lv_obj_t* home_screen_ = nullptr;
    lv_obj_t* volume_button_ = nullptr;
    lv_obj_t* volume_button_label_ = nullptr;
    lv_obj_t* volume_panel_ = nullptr;
    lv_obj_t* volume_slider_ = nullptr;
    lv_obj_t* volume_value_label_ = nullptr;
    int shown_volume_ = -1;
    lv_obj_t* welcome_screen_ = nullptr;
    lv_obj_t* welcome_eyes_[2] = {};
    lv_obj_t* welcome_status_ = nullptr;
    lv_obj_t* music_screen_ = nullptr;
    std::unique_ptr<apps::GamesApp> games_;
    std::unique_ptr<apps::EnglishApp> english_;
    lv_obj_t* home_button_ = nullptr;  // On the top layer, over the chatbot UI

    lv_obj_t* track_list_ = nullptr;
    lv_obj_t* empty_label_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* volume_label_ = nullptr;
    lv_obj_t* play_icon_ = nullptr;
    uint32_t shown_revision_ = UINT32_MAX;
    int64_t activating_since_ms_ = 0;
    // The welcome screen stays until startup has been idle for kWelcomeIdleMs.
    static constexpr int64_t kWelcomeIdleMs = 1500;
    int64_t idle_since_ms_ = 0;
    uint32_t welcome_ticks_ = 0;
    bool chat_was_needed_ = false;
    bool end_chat_pending_ = false;  // Home pressed mid-reply: close once it listens

    void BuildWelcomeScreen();
    void TickWelcome(bool chat_started);
    // True on the tick the chatbot becomes needed (a conversation starts...).
    bool ChatNeeded();
    void LeaveChat();
    void BuildHomeScreen();
    void BuildVolumePanel();
    void ShowVolumePanel(bool show);
    void UpdateVolumeUi();
    void PlayVolumeBeep();
    // Sets and saves the speaker volume (0-100); `round` snaps to fives.
    void SetVolume(int volume, bool round);
    void BuildMusicScreen();
    void BuildHomeButton();
    void ReloadTracks();
    void RefreshPlayer();
    void ChangeVolume(int delta);
    void SyncTextFont();
    void Tick();
};
