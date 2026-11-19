#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <lvgl.h>

namespace apps {

class EnglishView;

/**
 * The English app for children: topics such as colors, numbers, animals, food
 * and vehicles, in a scrolling menu. Each topic opens word cards (picture, English word, Vietnamese
 * meaning) and a quiz. The words and quiz rules live in english_words.*.
 * A topic's screens are built when opened and freed when closed.
 * Everything runs in the LVGL context.
 */
class EnglishApp {
public:
    // `on_talk` opens the chatbot to practise speaking.
    EnglishApp(std::function<void()> on_exit, std::function<void()> on_talk);
    ~EnglishApp();

    lv_obj_t* screen() const { return screen_; }
    // Called when the app closes: closes the open topic.
    void OnHide();

private:
    std::function<void()> on_exit_;
    std::function<void()> on_talk_;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* menu_ = nullptr;
    std::unique_ptr<EnglishView> view_;
    std::vector<lv_image_dsc_t> covers_;  // Menu tile pictures, one per topic

    void BuildMenu();
    void OpenTopic(int topic);
    void CloseView();
};

}  // namespace apps
