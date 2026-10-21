#pragma once
/**
 * eye_display.h — animated robot "emotion eyes" for the S3-Cam board.
 *
 * A drop-in replacement for SpiLcdDisplay that draws two glowing robot eyes
 * (plus eyebrows) on a black face and animates them to express the emotions
 * the server sends. It keeps the full base UI (status bar, chat, alerts)
 * underneath a full-screen face panel, so nothing else in the app breaks.
 *
 * Wiring in compact_wifi_board_s3cam.cc:
 *   1. #include "eye_display.h"
 *   2. Change the member type:  LcdDisplay* display_;  ->  EyeDisplay* display_;
 *   3. Change the allocation:   new SpiLcdDisplay(...) -> new EyeDisplay(...)
 *      (identical constructor arguments)
 * Nothing else is required: the eyes are built when the app calls SetupUI()
 * and react to emotions through the overridden SetEmotion().
 *
 * Design notes
 *  - Each eye is a rounded rectangle. Shape (width/height/radius), vertical
 *    offset, colour and a per-eye "mask" (a black rounded rect that carves a
 *    crescent out of the top or bottom) express most emotions without needing
 *    pupils. Eyebrows are thin bars whose vertical offset and rotation give
 *    the strongest emotional cue (angry / sad / surprised).
 *  - All shape changes go through lv_anim so transitions are smooth.
 *  - Blinking and small idle gaze shifts run on esp_timers and mutate LVGL
 *    only while holding the display lock (LVGL mutex), matching the rest of
 *    the codebase.
 */

#include "display/lcd_display.h"   // SpiLcdDisplay, DisplayLockGuard
#include <lvgl.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_log.h>
#include <cstring>
#include <algorithm>

// ── Geometry is derived from the panel size at runtime; these are ratios ─────
namespace eye_cfg {
    static constexpr int   kEyeWidth      = 78;   // base eye width  (px)
    static constexpr int   kEyeHeight     = 96;   // base eye height (px)
    static constexpr int   kEyeGap        = 40;   // gap between the two eyes
    static constexpr int   kEyeRadius     = 26;   // base corner radius
    static constexpr int   kBrowWidth     = 84;
    static constexpr int   kBrowHeight    = 12;
    static constexpr int   kBrowGapAbove  = 28;   // brow distance above eye top
    static constexpr int   kBlinkHeight   = 6;    // eye height while blinking
    static constexpr int   kAnimMs        = 220;  // default transition time
    static constexpr int   kBlinkMs       = 110;  // eyelid-down duration
    static constexpr int   kBlinkMinMs    = 2600; // idle blink spacing
    static constexpr int   kBlinkMaxMs    = 6200;
    static constexpr int   kGazeMinMs     = 3200; // idle look-around spacing
    static constexpr int   kGazeMaxMs     = 8000;
    static constexpr uint32_t kColorDefault = 0x33E0FF;  // cyan glow
    static constexpr uint32_t kColorHappy   = 0x36F0C0;  // warm teal
    static constexpr uint32_t kColorAngry   = 0xFF5030;  // red-orange
    static constexpr uint32_t kColorSad     = 0x4FA8FF;  // cool blue
    static constexpr uint32_t kColorLove    = 0xFF74B0;  // pink
    static constexpr uint32_t kFaceBg       = 0x000000;  // black face
}

// How a per-eye mask carves the eyeball into a crescent.
enum class EyeMask { None, Top, Bottom };

// A full description of the eyes for one emotion. ApplyStyle() animates toward it.
struct EyeStyle {
    int      w        = eye_cfg::kEyeWidth;
    int      h        = eye_cfg::kEyeHeight;
    int      radius   = eye_cfg::kEyeRadius;
    int      cy_off   = 0;         // vertical shift of both eyes from face centre
    int      gaze_dx  = 0;         // horizontal pupil-less gaze (translate)
    uint32_t color    = eye_cfg::kColorDefault;
    EyeMask  mask     = EyeMask::None;
    int      mask_pct = 0;         // 0..100 fraction of eye height covered
    bool     brow_show = false;
    int      brow_dy   = 0;        // brow vertical offset (negative = raised)
    int      brow_deg  = 0;        // inner-end rotation in degrees (angry>0, sad<0)
    bool     wink_left = false;    // close the left eye (winking)
};

// One eye: the coloured eyeball, its carving mask, a shine dot and a brow.
struct EyeObj {
    lv_obj_t* ball  = nullptr;
    lv_obj_t* mask  = nullptr;
    lv_obj_t* shine = nullptr;
    lv_obj_t* brow  = nullptr;
    int base_x = 0;   // centre-relative x offset (align)
};

class EyeDisplay : public SpiLcdDisplay {
public:
    EyeDisplay(esp_lcd_panel_io_handle_t panel_io,
               esp_lcd_panel_handle_t   panel,
               int width, int height,
               int offset_x, int offset_y,
               bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height,
                        offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    ~EyeDisplay() override { StopTimers(); }

    // Build the base UI first, then lay the eye face on top.
    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        BuildFace();
    }

    // Map a server emotion string to an eye expression.
    void SetEmotion(const char* emotion) override {
        if (emotion == nullptr) return;
        ESP_LOGI(kTag, "emotion -> %s", emotion);
        ApplyStyle(StyleFor(emotion));
    }

private:
    static constexpr const char* kTag = "EyeDisplay";

    lv_obj_t*  face_    = nullptr;
    EyeObj     left_, right_;
    EyeStyle   current_;
    bool       blinking_ = false;
    esp_timer_handle_t t_blink_close_ = nullptr;
    esp_timer_handle_t t_blink_open_  = nullptr;
    esp_timer_handle_t t_gaze_        = nullptr;

    // ── Animation helpers ────────────────────────────────────────────────
    static void AnimTo(lv_obj_t* obj, lv_anim_exec_xcb_t cb,
                       int32_t from, int32_t to, uint32_t ms) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, obj);
        lv_anim_set_exec_cb(&a, cb);
        lv_anim_set_values(&a, from, to);
        lv_anim_set_time(&a, ms);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }
    static void SetTransRotation(void* o, int32_t v) {
        lv_obj_set_style_transform_rotation(static_cast<lv_obj_t*>(o), v, 0);
    }
    static void SetTransX(void* o, int32_t v) {
        lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(o), v, 0);
    }
    static void SetTransY(void* o, int32_t v) {
        lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(o), v, 0);
    }

    // ── Build the face and both eyes ─────────────────────────────────────
    void BuildFace() {
        DisplayLockGuard lock(this);
        lv_obj_t* screen = lv_screen_active();
        if (!screen) { ESP_LOGE(kTag, "no active screen"); return; }

        // Hide the base emoji widgets; the eyes replace them.
        if (emoji_image_) lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        if (emoji_label_) lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);

        face_ = lv_obj_create(screen);
        lv_obj_remove_style_all(face_);
        lv_obj_set_size(face_, width_, height_);
        lv_obj_align(face_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(face_, lv_color_hex(eye_cfg::kFaceBg), 0);
        lv_obj_set_style_bg_opa(face_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(face_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(face_, LV_OBJ_FLAG_CLICKABLE);

        int half = (eye_cfg::kEyeWidth + eye_cfg::kEyeGap) / 2;
        left_.base_x  = -half;
        right_.base_x =  half;
        MakeEye(left_);
        MakeEye(right_);

        current_ = EyeStyle{};
        ApplyStyleLocked(current_, /*animate=*/false);

        StartTimers();
        ESP_LOGI(kTag, "eyes ready (%dx%d)", width_, height_);
    }

    void MakeEye(EyeObj& e) {
        // Eyebrow (built first so it sits behind the eyeball visually is fine).
        e.brow = lv_obj_create(face_);
        lv_obj_remove_style_all(e.brow);
        lv_obj_set_size(e.brow, eye_cfg::kBrowWidth, eye_cfg::kBrowHeight);
        lv_obj_set_style_radius(e.brow, eye_cfg::kBrowHeight / 2, 0);
        lv_obj_set_style_bg_color(e.brow, lv_color_hex(eye_cfg::kColorDefault), 0);
        lv_obj_set_style_bg_opa(e.brow, LV_OPA_COVER, 0);
        lv_obj_set_style_transform_pivot_x(e.brow, eye_cfg::kBrowWidth / 2, 0);
        lv_obj_set_style_transform_pivot_y(e.brow, eye_cfg::kBrowHeight / 2, 0);
        lv_obj_clear_flag(e.brow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(e.brow, LV_OBJ_FLAG_HIDDEN);

        // Eyeball.
        e.ball = lv_obj_create(face_);
        lv_obj_remove_style_all(e.ball);
        lv_obj_set_size(e.ball, eye_cfg::kEyeWidth, eye_cfg::kEyeHeight);
        lv_obj_set_style_radius(e.ball, eye_cfg::kEyeRadius, 0);
        lv_obj_set_style_bg_color(e.ball, lv_color_hex(eye_cfg::kColorDefault), 0);
        lv_obj_set_style_bg_opa(e.ball, LV_OPA_COVER, 0);
        lv_obj_align(e.ball, LV_ALIGN_CENTER, e.base_x, 0);
        lv_obj_clear_flag(e.ball, LV_OBJ_FLAG_SCROLLABLE);

        // Carving mask (same colour as the face; hidden by default).
        e.mask = lv_obj_create(e.ball);
        lv_obj_remove_style_all(e.mask);
        lv_obj_set_width(e.mask, lv_pct(120));
        lv_obj_set_height(e.mask, 0);
        lv_obj_set_style_bg_color(e.mask, lv_color_hex(eye_cfg::kFaceBg), 0);
        lv_obj_set_style_bg_opa(e.mask, LV_OPA_COVER, 0);
        lv_obj_align(e.mask, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_clear_flag(e.mask, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(e.mask, LV_OBJ_FLAG_HIDDEN);

        // Shine highlight for a bit of life.
        e.shine = lv_obj_create(e.ball);
        lv_obj_remove_style_all(e.shine);
        lv_obj_set_size(e.shine, 14, 14);
        lv_obj_set_style_radius(e.shine, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(e.shine, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(e.shine, LV_OPA_60, 0);
        lv_obj_align(e.shine, LV_ALIGN_TOP_RIGHT, -12, 14);
        lv_obj_clear_flag(e.shine, LV_OBJ_FLAG_SCROLLABLE);
    }

    // ── Apply a style (public entry takes the lock) ──────────────────────
    void ApplyStyle(const EyeStyle& s) {
        DisplayLockGuard lock(this);
        current_ = s;
        ApplyStyleLocked(s, /*animate=*/true);
    }

    void ApplyStyleLocked(const EyeStyle& s, bool animate) {
        if (!left_.ball || !right_.ball) return;
        uint32_t ms = animate ? eye_cfg::kAnimMs : 0;
        ApplyEye(left_,  s, ms, /*is_left=*/true);
        ApplyEye(right_, s, ms, /*is_left=*/false);
    }

    void ApplyEye(EyeObj& e, const EyeStyle& s, uint32_t ms, bool is_left) {
        lv_color_t col = lv_color_hex(s.color);
        lv_obj_set_style_bg_color(e.ball, col, 0);
        lv_obj_set_style_radius(e.ball, s.radius, 0);

        int target_w = s.w;
        int target_h = (is_left && s.wink_left) ? eye_cfg::kBlinkHeight : s.h;

        if (ms) {
            AnimTo(e.ball, (lv_anim_exec_xcb_t)lv_obj_set_width,  lv_obj_get_width(e.ball),  target_w, ms);
            AnimTo(e.ball, (lv_anim_exec_xcb_t)lv_obj_set_height, lv_obj_get_height(e.ball), target_h, ms);
            AnimTo(e.ball, SetTransX, lv_obj_get_style_translate_x(e.ball, LV_PART_MAIN), s.gaze_dx, ms);
        } else {
            lv_obj_set_width(e.ball, target_w);
            lv_obj_set_height(e.ball, target_h);
            lv_obj_set_style_translate_x(e.ball, s.gaze_dx, 0);
        }
        // Keep the eye vertically centred at the face centre + cy_off.
        lv_obj_align(e.ball, LV_ALIGN_CENTER, e.base_x, s.cy_off);

        // Carving mask.
        if (s.mask == EyeMask::None || s.mask_pct <= 0) {
            lv_obj_add_flag(e.mask, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(e.mask, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(e.mask, s.mask == EyeMask::Top ? LV_ALIGN_TOP_MID
                                                        : LV_ALIGN_BOTTOM_MID, 0, 0);
            int mh = target_h * s.mask_pct / 100;
            lv_obj_set_height(e.mask, mh);
        }

        // Shine hidden when the eye is squinted/masked heavily.
        bool hide_shine = (target_h < 30) || (s.mask != EyeMask::None && s.mask_pct >= 40);
        if (hide_shine) lv_obj_add_flag(e.shine, LV_OBJ_FLAG_HIDDEN);
        else            lv_obj_remove_flag(e.shine, LV_OBJ_FLAG_HIDDEN);

        // Eyebrow.
        if (!s.brow_show) {
            lv_obj_add_flag(e.brow, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(e.brow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(e.brow, col, 0);
            int brow_y = s.cy_off - (target_h / 2) - eye_cfg::kBrowGapAbove + s.brow_dy;
            lv_obj_align(e.brow, LV_ALIGN_CENTER, e.base_x, brow_y);
            // Inner ends move: left brow rotates +deg, right brow mirrors.
            int deg = is_left ? s.brow_deg : -s.brow_deg;
            int rot = deg * 10;  // LVGL uses 0.1-degree units
            if (ms) AnimTo(e.brow, SetTransRotation,
                           lv_obj_get_style_transform_rotation(e.brow, LV_PART_MAIN), rot, ms);
            else    lv_obj_set_style_transform_rotation(e.brow, rot, 0);
        }
    }

    // ── Emotion -> style table ───────────────────────────────────────────
    EyeStyle StyleFor(const char* em) {
        using namespace eye_cfg;
        EyeStyle s;  // defaults = neutral

        auto is = [&](const char* n) { return strcmp(em, n) == 0; };

        if (is("happy") || is("laughing") || is("funny") || is("silly") ||
            is("delicious") || is("kissy")) {
            // Upward crescents (smiling eyes).
            s.color = kColorHappy;
            s.h = kEyeHeight;
            s.mask = EyeMask::Bottom;   // cover lower half -> top crescent
            s.mask_pct = 55;
            s.radius = kEyeRadius + 8;
        } else if (is("loving")) {
            s.color = kColorLove;
            s.mask = EyeMask::Bottom;
            s.mask_pct = 50;
            s.radius = kEyeRadius + 10;
        } else if (is("sad") || is("crying")) {
            s.color = kColorSad;
            s.h = kEyeHeight - 18;
            s.cy_off = 10;
            s.mask = EyeMask::Top;      // droopy bottom crescent
            s.mask_pct = 30;
            s.brow_show = true;
            s.brow_dy = 6;
            s.brow_deg = -16;           // inner ends up
        } else if (is("angry")) {
            s.color = kColorAngry;
            s.h = kEyeHeight - 22;
            s.mask = EyeMask::Top;
            s.mask_pct = 25;
            s.brow_show = true;
            s.brow_dy = 2;
            s.brow_deg = 18;            // inner ends down
        } else if (is("surprised") || is("shocked")) {
            s.w = kEyeWidth + 8;
            s.h = kEyeHeight + 22;
            s.radius = LV_RADIUS_CIRCLE;
            s.brow_show = true;
            s.brow_dy = -10;            // brows raised
            s.brow_deg = 0;
        } else if (is("thinking") || is("confused")) {
            s.gaze_dx = 22;             // look to the side
            s.cy_off = -4;
            s.brow_show = true;
            s.brow_dy = -6;
            s.brow_deg = 8;
        } else if (is("sleepy")) {
            s.h = kEyeHeight / 2;
            s.cy_off = 6;
            s.mask = EyeMask::Top;
            s.mask_pct = 35;
        } else if (is("cool") || is("confident")) {
            s.h = kEyeHeight - 26;
            s.mask = EyeMask::Top;
            s.mask_pct = 20;
            s.radius = kEyeRadius;
        } else if (is("embarrassed")) {
            s.color = kColorLove;
            s.gaze_dx = -16;
            s.cy_off = 6;
            s.h = kEyeHeight - 12;
        } else if (is("winking")) {
            s.wink_left = true;
            s.color = kColorHappy;
        }
        // neutral / relaxed / robot_2 / staticstate / anything else -> defaults
        return s;
    }

    // ── Idle timers: blink + occasional gaze shift ───────────────────────
    void StartTimers() {
        esp_timer_create_args_t close_args = {
            .callback = [](void* a) { static_cast<EyeDisplay*>(a)->BlinkClose(); },
            .arg = this, .dispatch_method = ESP_TIMER_TASK, .name = "eye_blink_c"
        };
        esp_timer_create(&close_args, &t_blink_close_);
        esp_timer_create_args_t open_args = {
            .callback = [](void* a) { static_cast<EyeDisplay*>(a)->BlinkOpen(); },
            .arg = this, .dispatch_method = ESP_TIMER_TASK, .name = "eye_blink_o"
        };
        esp_timer_create(&open_args, &t_blink_open_);
        esp_timer_create_args_t gaze_args = {
            .callback = [](void* a) { static_cast<EyeDisplay*>(a)->IdleGaze(); },
            .arg = this, .dispatch_method = ESP_TIMER_TASK, .name = "eye_gaze"
        };
        esp_timer_create(&gaze_args, &t_gaze_);
        ScheduleBlink();
        ScheduleGaze();
    }

    static uint32_t RandBetween(uint32_t lo, uint32_t hi) {
        return lo + (esp_random() % (hi - lo));
    }
    void ScheduleBlink() {
        if (t_blink_close_)
            esp_timer_start_once(t_blink_close_,
                (uint64_t)RandBetween(eye_cfg::kBlinkMinMs, eye_cfg::kBlinkMaxMs) * 1000);
    }
    void ScheduleGaze() {
        if (t_gaze_)
            esp_timer_start_once(t_gaze_,
                (uint64_t)RandBetween(eye_cfg::kGazeMinMs, eye_cfg::kGazeMaxMs) * 1000);
    }

    void BlinkClose() {
        {
            DisplayLockGuard lock(this);
            if (!left_.ball || blinking_) { }
            else {
                blinking_ = true;
                AnimTo(left_.ball,  (lv_anim_exec_xcb_t)lv_obj_set_height,
                       lv_obj_get_height(left_.ball),  eye_cfg::kBlinkHeight, eye_cfg::kBlinkMs);
                AnimTo(right_.ball, (lv_anim_exec_xcb_t)lv_obj_set_height,
                       lv_obj_get_height(right_.ball), eye_cfg::kBlinkHeight, eye_cfg::kBlinkMs);
            }
        }
        if (blinking_ && t_blink_open_)
            esp_timer_start_once(t_blink_open_, (uint64_t)eye_cfg::kBlinkMs * 1000);
        else
            ScheduleBlink();
    }

    void BlinkOpen() {
        {
            DisplayLockGuard lock(this);
            if (left_.ball) {
                int lh = current_.wink_left ? eye_cfg::kBlinkHeight : current_.h;
                AnimTo(left_.ball,  (lv_anim_exec_xcb_t)lv_obj_set_height,
                       lv_obj_get_height(left_.ball),  lh, eye_cfg::kBlinkMs);
                AnimTo(right_.ball, (lv_anim_exec_xcb_t)lv_obj_set_height,
                       lv_obj_get_height(right_.ball), current_.h, eye_cfg::kBlinkMs);
            }
            blinking_ = false;
        }
        ScheduleBlink();
    }

    void IdleGaze() {
        {
            DisplayLockGuard lock(this);
            if (left_.ball && !blinking_) {
                int dx = current_.gaze_dx + (int)RandBetween(0, 3) * 12 - 12;  // -12..+12
                AnimTo(left_.ball,  SetTransX, lv_obj_get_style_translate_x(left_.ball, LV_PART_MAIN),  dx, 400);
                AnimTo(right_.ball, SetTransX, lv_obj_get_style_translate_x(right_.ball, LV_PART_MAIN), dx, 400);
            }
        }
        ScheduleGaze();
    }

    void StopTimers() {
        for (esp_timer_handle_t* t : {&t_blink_close_, &t_blink_open_, &t_gaze_}) {
            if (*t) { esp_timer_stop(*t); esp_timer_delete(*t); *t = nullptr; }
        }
    }
};
