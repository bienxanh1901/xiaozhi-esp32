#pragma once

#include <functional>
#include <utility>

#include <lvgl.h>

LV_FONT_DECLARE(font_material_symbols_30_4);

// Small LVGL helpers shared by the launcher and its apps. Everything here must
// run in the LVGL context or with the display lock held.
namespace apps {

inline constexpr uint32_t kBackground = 0x101418;
inline constexpr uint32_t kSurface = 0x1E252B;
inline constexpr uint32_t kAccent = 0x8E24AA;
// Taps near the top of the panel tend to land lower than they look, so the bar
// is tall and its button fills the corner.
inline constexpr int kTopBar = 56;
inline constexpr int kTopBarButtonWidth = 64;
inline constexpr int kTopBarTitleLeft = kTopBarButtonWidth + 12;
// Keep the top-right corner free: boards may show a battery status there.
inline constexpr int kRightReserve = 92;

// Event handlers are std::functions owned by the object they belong to.
inline void BindEvent(lv_obj_t* obj, lv_event_code_t code, std::function<void()> fn) {
    auto* holder = new std::function<void()>(std::move(fn));
    lv_obj_add_event_cb(obj, [](lv_event_t* e) {
        (*static_cast<std::function<void()>*>(lv_event_get_user_data(e)))();
    }, code, holder);
    lv_obj_add_event_cb(obj, [](lv_event_t* e) {
        delete static_cast<std::function<void()>*>(lv_event_get_user_data(e));
    }, LV_EVENT_DELETE, holder);
}

inline void BindClick(lv_obj_t* obj, std::function<void()> fn) {
    BindEvent(obj, LV_EVENT_CLICKED, std::move(fn));
}

inline lv_obj_t* MakeScreen() {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

inline void PlainBox(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

inline lv_obj_t* MakeIconLabel(lv_obj_t* parent, const char* icon, uint32_t color = 0xFFFFFF) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &font_material_symbols_30_4, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, icon);
    return label;
}

inline lv_obj_t* MakeIconButton(lv_obj_t* parent, const char* icon, int w, int h,
                                std::function<void()> on_click) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(kSurface), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(kAccent), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_center(MakeIconLabel(button, icon));
    BindClick(button, std::move(on_click));
    return button;
}

// A top bar with an icon button on the left. Returns the bar.
inline lv_obj_t* MakeTopBar(lv_obj_t* parent, const char* icon, std::function<void()> on_click) {
    lv_obj_t* bar = lv_obj_create(parent);
    PlainBox(bar);
    lv_obj_set_size(bar, lv_pct(100), kTopBar);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kSurface), 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t* button = MakeIconButton(bar, icon, kTopBarButtonWidth, kTopBar - 8, std::move(on_click));
    lv_obj_set_ext_click_area(button, 4);  // Out to the edges of the bar
    lv_obj_align(button, LV_ALIGN_LEFT_MID, 4, 0);
    return bar;
}

}  // namespace apps
