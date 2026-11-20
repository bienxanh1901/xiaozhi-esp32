#include "calculator_app.h"

#include <string>
#include <utility>

#include "material_symbols.h"
#include "ui_helpers.h"

namespace apps {

namespace {

constexpr int kMargin = 4;
constexpr int kGap = 4;
constexpr int kColumns = 5;
constexpr int kRows = 4;
constexpr int kKeypadTop = 96;
constexpr int kMainScale = 150;    // Percent: the theme font is small for a display
constexpr size_t kMainChars = 20;  // What fits on the scaled display line

constexpr uint32_t kDigitColor = 0x2B343C;
constexpr uint32_t kSignColor = 0xEF6C00;
constexpr uint32_t kClearColor = 0xC62828;
constexpr uint32_t kEqualsColor = 0x2E7D32;

struct Key {
    char key;
    const char* label;  // nullptr: the backspace icon
    int column;
    int row;
    int span;
    uint32_t color;
};

constexpr Key kKeys[] = {
    {'7', "7", 0, 0, 1, kDigitColor}, {'8', "8", 1, 0, 1, kDigitColor},
    {'9', "9", 2, 0, 1, kDigitColor}, {':', ":", 3, 0, 1, kSignColor},
    {'C', "C", 4, 0, 1, kClearColor}, {'4', "4", 0, 1, 1, kDigitColor},
    {'5', "5", 1, 1, 1, kDigitColor}, {'6', "6", 2, 1, 1, kDigitColor},
    {'x', "x", 3, 1, 1, kSignColor},  {'B', nullptr, 4, 1, 1, kClearColor},
    {'1', "1", 0, 2, 1, kDigitColor}, {'2', "2", 1, 2, 1, kDigitColor},
    {'3', "3", 2, 2, 1, kDigitColor}, {'-', "-", 3, 2, 1, kSignColor},
    {'+', "+", 4, 2, 1, kSignColor},  {'0', "0", 0, 3, 2, kDigitColor},
    {'.', ".", 2, 3, 1, kDigitColor}, {'=', "=", 3, 3, 2, kEqualsColor},
};

}  // namespace

CalculatorApp::CalculatorApp(std::function<void()> on_exit) : on_exit_(std::move(on_exit)) {
    screen_ = MakeScreen();
    const int width = LV_HOR_RES;
    const int height = LV_VER_RES;

    // A small home button, so the display and keys get the room.
    lv_obj_t* home =
        MakeIconButton(screen_, MATERIAL_SYMBOLS_HOME, 64, 38, [this]() { on_exit_(); });
    lv_obj_set_ext_click_area(home, kMargin);
    lv_obj_align(home, LV_ALIGN_TOP_LEFT, kMargin, 2);
    lv_obj_t* title = lv_label_create(screen_);
    lv_label_set_text(title, "Calculator");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 80, 12);

    // The display: the answer so far above, the sum (or answer) below.
    lv_obj_t* display = lv_obj_create(screen_);
    PlainBox(display);
    lv_obj_set_size(display, width - 2 * kMargin, kKeypadTop - 42 - kGap);
    lv_obj_set_pos(display, kMargin, 42);
    lv_obj_set_style_radius(display, 10, 0);
    lv_obj_set_style_bg_color(display, lv_color_hex(kSurface), 0);
    lv_obj_set_style_pad_hor(display, 8, 0);

    hint_ = lv_label_create(display);
    lv_obj_set_width(hint_, lv_pct(100));
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(hint_, lv_color_hex(0x9AA5AE), 0);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_align(hint_, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Scaled about its right edge, so it grows to the left like a calculator's.
    const int main_width = (width - 2 * kMargin - 16) * 100 / kMainScale;
    main_ = lv_label_create(display);
    lv_obj_set_width(main_, main_width);
    lv_obj_set_style_text_align(main_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(main_, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_align(main_, LV_ALIGN_BOTTOM_RIGHT, 0, -6);
    lv_obj_set_style_transform_pivot_x(main_, lv_pct(100), 0);
    lv_obj_set_style_transform_pivot_y(main_, lv_pct(100), 0);
    lv_obj_set_style_transform_scale(main_, 256 * kMainScale / 100, 0);

    // The keypad.
    const int key_w = (width - 2 * kMargin - (kColumns - 1) * kGap) / kColumns;
    const int key_h = (height - kKeypadTop - kMargin - (kRows - 1) * kGap) / kRows;
    const int left = (width - (kColumns * key_w + (kColumns - 1) * kGap)) / 2;
    for (const Key& key : kKeys) {
        lv_obj_t* button = lv_button_create(screen_);
        lv_obj_set_size(button, key.span * key_w + (key.span - 1) * kGap, key_h);
        lv_obj_set_pos(button, left + key.column * (key_w + kGap),
                       kKeypadTop + key.row * (key_h + kGap));
        lv_obj_set_style_pad_all(button, 0, 0);
        lv_obj_set_style_radius(button, 10, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(key.color), 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(kAccent), LV_STATE_PRESSED);
        if (key.label == nullptr) {
            lv_obj_center(MakeIconLabel(button, MATERIAL_SYMBOLS_ARROW_BACK));
        } else {
            lv_obj_t* label = lv_label_create(button);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_label_set_text(label, key.label);
            lv_obj_center(label);
            lv_obj_set_style_transform_pivot_x(label, lv_pct(50), 0);
            lv_obj_set_style_transform_pivot_y(label, lv_pct(50), 0);
            lv_obj_set_style_transform_scale(label, 256 * 140 / 100, 0);
        }
        const char k = key.key;
        BindClick(button, [this, k]() { Press(k); });
    }
    Update();
}

void CalculatorApp::OnHide() {
    calculator_.Press('C');
    Update();
}

void CalculatorApp::Press(char key) {
    calculator_.Press(key);
    Update();
}

void CalculatorApp::Update() {
    lv_label_set_text(hint_, calculator_.Hint().c_str());
    // Show the end of a long sum: that is where the typing happens.
    std::string text = calculator_.Main();
    if (text.size() > kMainChars)
        text = ".." + text.substr(text.size() - (kMainChars - 2));
    lv_label_set_text(main_, text.c_str());
    lv_obj_set_style_text_color(main_, lv_color_hex(calculator_.HasError() ? 0xFF8A80 : 0xFFFFFF),
                                0);
}

}  // namespace apps
