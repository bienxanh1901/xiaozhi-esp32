#pragma once

#include <functional>

#include <lvgl.h>

#include "calculator.h"

namespace apps {

/**
 * The Calculator app: a keypad with + - x : and a two-line display (the sum
 * being typed, and the answer so far). The rules live in calculator.*.
 * Everything runs in the LVGL context.
 */
class CalculatorApp {
public:
    explicit CalculatorApp(std::function<void()> on_exit);

    lv_obj_t* screen() const { return screen_; }
    // Called when the app closes: clears the sum.
    void OnHide();

private:
    std::function<void()> on_exit_;
    Calculator calculator_;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* hint_ = nullptr;
    lv_obj_t* main_ = nullptr;

    void Press(char key);
    void Update();
};

}  // namespace apps
