#include "english_app.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <esp_random.h>

#include "application.h"
#include "assets.h"
#include "assets/lang_config.h"
#include "english_words.h"
#include "material_symbols.h"
#include "ui_helpers.h"

namespace apps {

using english::Picture;
using english::Quiz;
using english::Topic;
using english::Word;

namespace {

constexpr uint32_t kRightColor = 0x2E7D32;
constexpr uint32_t kWrongColor = 0xC62828;
constexpr uint32_t kMutedText = 0xB0BEC5;
constexpr uint32_t kStarColor = 0xFFCA28;
constexpr int kPicture = 104;     // Card picture diameter
constexpr int kButtonRow = 56;    // Bottom buttons height
constexpr uint32_t kNextQuestionMs = 1500;  // Time to hear the word

uint32_t HardwareRandom() { return esp_random(); }

// The English words are ASCII, so a large built-in Latin font fits them; the
// theme font (Vietnamese) is used when that font is not enabled.
const lv_font_t* WordFont() {
#if LV_FONT_MONTSERRAT_28
    return &lv_font_montserrat_28;
#else
    return nullptr;
#endif
}

// Played from the main task: the sound queue may block, the touch UI must not.
void PlaySound(std::string_view sound) {
    auto& app = Application::GetInstance();
    app.Schedule([&app, sound]() { app.PlaySound(sound); });
}

// The word's clip in the assets partition. Must match clip_name() in
// scripts/gen_english_audio.py.
std::string ClipName(const char* word) {
    std::string name = "en_";
    for (const char* c = word; *c != '\0'; ++c) name += *c == ' ' ? '_' : *c;
    return name + ".ogg";
}

// The word's picture in the assets partition. Must match image_name() in
// scripts/gen_english_images.py.
constexpr int kImageSize = 88;
std::string ImageName(const char* word) {
    std::string name = "img_";
    for (const char* c = word; *c != '\0'; ++c) name += *c == ' ' ? '_' : *c;
    return name + ".bin";
}

// Points `dsc` at the word's picture in the memory-mapped assets. False when
// the assets do not have it.
bool LoadPicture(const char* word, lv_image_dsc_t& dsc) {
    if (dsc.data != nullptr) return true;
    void* data = nullptr;
    size_t size = 0;
    constexpr size_t kBytes = kImageSize * kImageSize * 3;  // RGB565 + alpha
    if (!Assets::GetInstance().GetAssetData(ImageName(word), data, size) || size != kBytes) {
        return false;
    }
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc.header.w = kImageSize;
    dsc.header.h = kImageSize;
    dsc.header.stride = kImageSize * 2;
    dsc.data_size = kBytes;
    dsc.data = static_cast<const uint8_t*>(data);
    return true;
}

// Says the word, unless a conversation has the speaker or the clip is missing
// (assets from before the clips were added).
void SayWord(const char* word) {
    void* data = nullptr;
    size_t size = 0;
    if (!Assets::GetInstance().GetAssetData(ClipName(word), data, size)) {
        return;
    }
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateIdle) return;
    // The clip is memory-mapped flash, valid while the assets are applied.
    std::string_view clip(static_cast<const char*>(data), size);
    app.Schedule([&app, clip]() {
        if (app.GetDeviceState() == kDeviceStateIdle) app.PlaySound(clip);
    });
}

lv_obj_t* MakeText(lv_obj_t* parent, const char* text = "", uint32_t color = 0xFFFFFF) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

void SetWordFont(lv_obj_t* label) {
    if (const lv_font_t* font = WordFont()) lv_obj_set_style_text_font(label, font, 0);
}

// Scales a label about its center, for glyphs larger than the fonts on board.
void Enlarge(lv_obj_t* label, int percent) {
    lv_obj_set_style_transform_pivot_x(label, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(label, lv_pct(50), 0);
    lv_obj_set_style_transform_scale(label, 256 * percent / 100, 0);
}

// A round picture of a word: a color swatch, a digit, an icon or a letter.
class WordPicture {
public:
    WordPicture(lv_obj_t* parent, const Topic& topic)
        : topic_(topic), image_dscs_(topic.count, lv_image_dsc_t{}) {
        circle_ = lv_obj_create(parent);
        PlainBox(circle_);
        lv_obj_set_size(circle_, kPicture, kPicture);
        lv_obj_set_style_radius(circle_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(circle_, lv_color_hex(0x546E7A), 0);

        glyph_ = MakeText(circle_);
        lv_obj_center(glyph_);
        image_ = lv_image_create(circle_);
        lv_obj_center(image_);
        lv_obj_add_flag(image_, LV_OBJ_FLAG_HIDDEN);
        // The image passes taps on to the circle (which says the word).
        lv_obj_remove_flag(image_, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t* obj() const { return circle_; }

    void Show(int index) {
        const Word& word = topic_.words[index];
        char text[12] = "";
        uint32_t fill = topic_.color;
        bool show_image = false;
        Picture picture = topic_.picture;
        if (picture == Picture::kImage) {
            show_image = LoadImage(index);
            if (show_image) {
                fill = kImageBackground;  // Emoji colors read best on a light disc
            } else {
                // Assets without the pictures: the icon, or else the letter.
                picture = word.icon != nullptr ? Picture::kIcon : Picture::kLetter;
            }
        }
        switch (picture) {
            case Picture::kColor:
                fill = word.color;
                break;
            case Picture::kNumber:
                snprintf(text, sizeof(text), "%d", index + 1);
                break;
            case Picture::kIcon:
                snprintf(text, sizeof(text), "%s", word.icon);
                break;
            case Picture::kLetter:
                text[0] = static_cast<char>(word.en[0] - 'a' + 'A');
                break;
            case Picture::kImage:
                break;
        }
        SetGlyphStyle(picture);
        lv_obj_set_style_bg_color(circle_, lv_color_hex(fill), 0);
        // A dark swatch on the dark background needs an outline.
        lv_obj_set_style_border_width(circle_, picture == Picture::kColor ? 3 : 0, 0);
        lv_label_set_text(glyph_, text);
        if (show_image) {
            lv_obj_remove_flag(image_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(glyph_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(image_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(glyph_, LV_OBJ_FLAG_HIDDEN);
        }
    }

private:
    static constexpr uint32_t kImageBackground = 0xECEFF1;
    const Topic& topic_;
    lv_obj_t* circle_ = nullptr;
    lv_obj_t* glyph_ = nullptr;
    lv_obj_t* image_ = nullptr;
    std::vector<lv_image_dsc_t> image_dscs_;
    Picture glyph_style_ = Picture::kColor;  // Plain text

    // Points the image at the word's picture in the memory-mapped assets.
    // One descriptor per word: LVGL caches decoded images by descriptor address,
    // so a reused descriptor could show the previous picture.
    bool LoadImage(int index) {
        lv_image_dsc_t& dsc = image_dscs_[index];
        if (!LoadPicture(topic_.words[index].en, dsc)) return false;
        lv_image_set_src(image_, &dsc);
        return true;
    }

    void SetGlyphStyle(Picture picture) {
        if (picture == Picture::kImage || picture == glyph_style_) return;
        glyph_style_ = picture;
        if (picture == Picture::kIcon) {
            lv_obj_set_style_text_font(glyph_, &font_material_symbols_30_4, 0);
            Enlarge(glyph_, 200);
        } else if (picture == Picture::kNumber || picture == Picture::kLetter) {
            if (const lv_font_t* font = WordFont()) {
                lv_obj_set_style_text_font(glyph_, font, 0);
            } else {
                lv_obj_remove_local_style_prop(glyph_, LV_STYLE_TEXT_FONT, 0);
            }
            Enlarge(glyph_, WordFont() != nullptr ? 170 : 220);
        }
    }
};

}  // namespace

// One open topic: word cards, and the quiz over the same words.
class EnglishView {
public:
    EnglishView(lv_obj_t* parent, const Topic& topic, std::function<void()> on_back,
                std::function<void()> on_talk)
        : topic_(topic), on_back_(std::move(on_back)), on_talk_(std::move(on_talk)) {
        root_ = lv_obj_create(parent);
        PlainBox(root_);
        lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);

        lv_obj_t* bar = MakeTopBar(root_, MATERIAL_SYMBOLS_ARROW_BACK, [this]() { Back(); });
        title_ = MakeText(bar, topic_.name);
        lv_obj_align(title_, LV_ALIGN_LEFT_MID, kTopBarTitleLeft, 0);
        info_ = MakeText(bar);
        lv_obj_align(info_, LV_ALIGN_RIGHT_MID, -kRightReserve, 0);

        BuildCards();
        BuildQuiz();
        ShowCards();
    }

    ~EnglishView() {
        if (timer_ != nullptr) lv_timer_delete(timer_);
        lv_obj_delete(root_);
    }

private:
    const Topic& topic_;
    std::function<void()> on_back_;
    std::function<void()> on_talk_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* info_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    // Cards
    lv_obj_t* cards_ = nullptr;
    std::unique_ptr<WordPicture> card_picture_;
    lv_obj_t* card_word_ = nullptr;
    lv_obj_t* card_meaning_ = nullptr;
    int card_ = 0;

    // Quiz
    lv_obj_t* quiz_ = nullptr;
    std::unique_ptr<WordPicture> quiz_picture_;
    lv_obj_t* quiz_meaning_ = nullptr;
    lv_obj_t* choices_[Quiz::kChoices] = {};
    lv_obj_t* choice_labels_[Quiz::kChoices] = {};
    lv_obj_t* result_ = nullptr;
    Quiz quiz_state_;

    lv_obj_t* MakePanel() {
        lv_obj_t* panel = lv_obj_create(root_);
        PlainBox(panel);
        lv_obj_set_size(panel, lv_pct(100), LV_VER_RES - kTopBar);
        lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
        lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
        return panel;
    }

    lv_obj_t* MakeButton(lv_obj_t* parent, int w, int h, uint32_t color, std::function<void()> fn) {
        lv_obj_t* button = lv_button_create(parent);
        lv_obj_set_size(button, w, h);
        lv_obj_set_style_radius(button, 14, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_set_style_pad_all(button, 0, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
        BindClick(button, std::move(fn));
        return button;
    }

    void BuildCards() {
        cards_ = MakePanel();
        card_picture_ = std::make_unique<WordPicture>(cards_, topic_);
        lv_obj_align(card_picture_->obj(), LV_ALIGN_TOP_LEFT, 14, 10);
        // Tapping the picture says the word again.
        lv_obj_add_flag(card_picture_->obj(), LV_OBJ_FLAG_CLICKABLE);
        BindClick(card_picture_->obj(), [this]() { SayWord(topic_.words[card_].en); });

        const int text_left = 14 + kPicture + 14;
        const int text_width = LV_HOR_RES - text_left - 8;
        card_word_ = MakeText(cards_);
        SetWordFont(card_word_);
        lv_obj_set_width(card_word_, text_width);
        lv_label_set_long_mode(card_word_, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align(card_word_, LV_ALIGN_TOP_LEFT, text_left, 22);

        card_meaning_ = MakeText(cards_, "", kMutedText);
        lv_obj_set_width(card_meaning_, text_width);
        lv_label_set_long_mode(card_meaning_, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align_to(card_meaning_, card_word_, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

        // Previous, talk, quiz, next.
        lv_obj_t* row = lv_obj_create(cards_);
        PlainBox(row);
        lv_obj_set_size(row, lv_pct(100), kButtonRow);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        MakeIconButton(row, MATERIAL_SYMBOLS_KEYBOARD_ARROW_LEFT, 64, kButtonRow,
                       [this]() { ShowCard(card_ - 1); });
        MakeIconButton(row, MATERIAL_SYMBOLS_MIC, 64, kButtonRow, [this]() { on_talk_(); });
        lv_obj_t* quiz = MakeButton(row, 84, kButtonRow, kAccent, [this]() { StartQuiz(); });
        lv_obj_center(MakeIconLabel(quiz, MATERIAL_SYMBOLS_HELP));
        MakeIconButton(row, MATERIAL_SYMBOLS_KEYBOARD_ARROW_RIGHT, 64, kButtonRow,
                       [this]() { ShowCard(card_ + 1); });
    }

    void BuildQuiz() {
        quiz_ = MakePanel();
        lv_obj_add_flag(quiz_, LV_OBJ_FLAG_HIDDEN);

        quiz_picture_ = std::make_unique<WordPicture>(quiz_, topic_);
        lv_obj_align(quiz_picture_->obj(), LV_ALIGN_TOP_LEFT, 12, 12);
        quiz_meaning_ = MakeText(quiz_, "", kMutedText);
        lv_obj_set_width(quiz_meaning_, kPicture + 16);
        lv_obj_set_style_text_align(quiz_meaning_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(quiz_meaning_, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align_to(quiz_meaning_, quiz_picture_->obj(), LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

        // The answers stacked on the right: long words fit, and big targets.
        const int left = 12 + kPicture + 16, w = LV_HOR_RES - left - 10, h = 52, gap = 6;
        for (int i = 0; i < Quiz::kChoices; ++i) {
            choices_[i] = MakeButton(quiz_, w, h, kSurface, [this, i]() { Answer(i); });
            lv_obj_set_pos(choices_[i], left, 6 + i * (h + gap));
            choice_labels_[i] = MakeText(choices_[i]);
            SetWordFont(choice_labels_[i]);
            lv_obj_center(choice_labels_[i]);
        }

        // The round's result, over the quiz; tap to play again.
        result_ = MakeText(quiz_);
        lv_obj_set_width(result_, 280);
        lv_obj_set_style_text_align(result_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_color(result_, lv_color_hex(kRightColor), 0);
        lv_obj_set_style_bg_opa(result_, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(result_, 16, 0);
        lv_obj_set_style_radius(result_, 12, 0);
        lv_obj_add_flag(result_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(result_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(result_);
        BindClick(result_, [this]() { StartQuiz(); });
    }

    void Back() {
        if (!lv_obj_has_flag(quiz_, LV_OBJ_FLAG_HIDDEN)) {
            ShowCards();  // From the quiz back to the cards
        } else {
            on_back_();
        }
    }

    void StopTimer() {
        if (timer_ != nullptr) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

    void ShowCards() {
        StopTimer();
        lv_obj_add_flag(quiz_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(cards_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(title_, topic_.name);
        ShowCard(card_);
    }

    void ShowCard(int index) {
        card_ = (index + topic_.count) % topic_.count;
        const Word& word = topic_.words[card_];
        card_picture_->Show(card_);
        lv_label_set_text(card_word_, word.en);
        lv_label_set_text(card_meaning_, word.vi);
        lv_label_set_text_fmt(info_, "%d/%d", card_ + 1, topic_.count);
        SayWord(word.en);
    }

    void StartQuiz() {
        StopTimer();
        lv_obj_add_flag(cards_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(quiz_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(result_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(title_, "Đố vui");
        quiz_state_.Start(topic_.count, HardwareRandom);
        ShowQuestion();
    }

    void ShowQuestion() {
        const int asked = quiz_state_.Asked();
        quiz_picture_->Show(asked);
        lv_label_set_text(quiz_meaning_, topic_.words[asked].vi);
        for (int i = 0; i < Quiz::kChoices; ++i) {
            lv_obj_set_style_bg_color(choices_[i], lv_color_hex(kSurface), 0);
            lv_label_set_text(choice_labels_[i], topic_.words[quiz_state_.Choice(i)].en);
        }
        lv_label_set_text_fmt(info_, "%d/%d", quiz_state_.Question() + 1, Quiz::kQuestions);
    }

    void Answer(int choice) {
        switch (quiz_state_.Answer(choice)) {
            case Quiz::Result::kIgnored:
                return;
            case Quiz::Result::kWrong:
                lv_obj_set_style_bg_color(choices_[choice], lv_color_hex(kWrongColor), 0);
                PlaySound(Lang::Sounds::OGG_VIBRATION);
                return;
            case Quiz::Result::kRight:
                lv_obj_set_style_bg_color(choices_[choice], lv_color_hex(kRightColor), 0);
                // Hear the word just found (instead of the success chime).
                SayWord(topic_.words[quiz_state_.Asked()].en);
                timer_ = lv_timer_create(
                    [](lv_timer_t* t) {
                        auto* self = static_cast<EnglishView*>(lv_timer_get_user_data(t));
                        self->timer_ = nullptr;  // One-shot: LVGL deletes it after this call
                        self->NextQuestion();
                    },
                    kNextQuestionMs, this);
                lv_timer_set_repeat_count(timer_, 1);
                return;
        }
    }

    void NextQuestion() {
        if (quiz_state_.Next()) {
            ShowQuestion();
            return;
        }
        const int score = quiz_state_.Score();
        const char* praise = score == Quiz::kQuestions     ? "Xuất sắc!"
                             : score * 10 >= Quiz::kQuestions * 7 ? "Giỏi lắm!"
                                                                  : "Cố lên nhé!";
        lv_label_set_text_fmt(result_, "%s\n%d/%d\nChạm để chơi lại", praise, score,
                              Quiz::kQuestions);
        lv_obj_remove_flag(result_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(result_);
        lv_label_set_text_fmt(info_, "%d/%d", score, Quiz::kQuestions);
    }
};

EnglishApp::EnglishApp(std::function<void()> on_exit, std::function<void()> on_talk)
    : on_exit_(std::move(on_exit)), on_talk_(std::move(on_talk)) {
    screen_ = MakeScreen();
    BuildMenu();
}

EnglishApp::~EnglishApp() = default;

void EnglishApp::BuildMenu() {
    menu_ = lv_obj_create(screen_);
    PlainBox(menu_);
    lv_obj_set_size(menu_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(menu_, LV_OPA_TRANSP, 0);

    lv_obj_t* bar = MakeTopBar(menu_, MATERIAL_SYMBOLS_HOME, [this]() { on_exit_(); });
    lv_obj_align(MakeText(bar, "English"), LV_ALIGN_LEFT_MID, kTopBarTitleLeft, 0);

    // Two columns of tiles in a list that scrolls under the top bar.
    lv_obj_t* list = lv_obj_create(menu_);
    PlainBox(list);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_bottom(list, 8, 0);
    lv_obj_set_pos(list, 0, kTopBar);
    lv_obj_set_size(list, lv_pct(100), LV_VER_RES - kTopBar);

    // A topic with pictures shows its first word's picture on the tile.
    covers_.assign(english::TopicCount(), lv_image_dsc_t{});
    const int w = 148, h = 56, gap = 8, top = 4, picture = 44;
    for (int i = 0; i < english::TopicCount(); ++i) {
        const Topic& topic = english::GetTopic(i);
        lv_obj_t* tile = lv_button_create(list);
        lv_obj_set_size(tile, w, h);
        lv_obj_set_pos(tile, gap + (i % 2) * (w + gap), top + (i / 2) * (h + 4));
        lv_obj_set_style_radius(tile, 14, 0);
        lv_obj_set_style_shadow_width(tile, 0, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        // Let a swipe that starts on a tile scroll the list.
        lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_set_style_bg_color(tile, lv_color_hex(topic.color), 0);
        if (topic.picture == Picture::kImage && LoadPicture(topic.words[0].en, covers_[i])) {
            lv_obj_t* image = lv_image_create(tile);
            lv_image_set_src(image, &covers_[i]);
            lv_obj_set_size(image, picture, picture);
            lv_image_set_inner_align(image, LV_IMAGE_ALIGN_STRETCH);
            lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(image, LV_ALIGN_LEFT_MID, 2, 0);
        } else {
            lv_obj_align(MakeIconLabel(tile, topic.icon), LV_ALIGN_LEFT_MID, 6, 0);
        }
        // Long names wrap onto a second line.
        lv_obj_t* name = MakeText(tile, topic.name);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(name, w - 50);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 48, 0);
        BindClick(tile, [this, i]() { OpenTopic(i); });
    }
}

void EnglishApp::OpenTopic(int topic) {
    lv_obj_add_flag(menu_, LV_OBJ_FLAG_HIDDEN);
    view_ = std::make_unique<EnglishView>(
        screen_, english::GetTopic(topic),
        [this]() {
            // Runs inside the back button's click event, and closing the topic
            // deletes that button, so finish the event first.
            lv_async_call([](void* self) { static_cast<EnglishApp*>(self)->CloseView(); }, this);
        },
        on_talk_);
}

void EnglishApp::CloseView() {
    view_.reset();
    lv_obj_remove_flag(menu_, LV_OBJ_FLAG_HIDDEN);
}

void EnglishApp::OnHide() { CloseView(); }

}  // namespace apps
