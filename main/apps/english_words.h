#pragma once

#include <cstdint>

// Words and quiz rules for the English learning app. Plain C++ with no LVGL or
// ESP-IDF, so the host tests can check them (scripts/tests/english_words_test.cc).
namespace english {

// How a topic's words are pictured on the card.
enum class Picture {
    kColor,   // A swatch in the word's color
    kNumber,  // The digit
    kIcon,    // The word's icon
    kLetter,  // The word's first letter ("C is for cat")
    // A picture from the assets (scripts/gen_english_images.py); the icon, or
    // else the first letter, when the assets do not have it.
    kImage,
};

struct Word {
    const char* en;
    const char* vi;
    uint32_t color = 0;          // Swatch color (kColor)
    const char* icon = nullptr;  // Material Symbols glyph (kIcon)
};

struct Topic {
    const char* name;  // Vietnamese, shown on the menu
    const char* icon;  // Material Symbols glyph for the menu tile
    uint32_t color;    // Menu tile and card color
    Picture picture;
    const Word* words;
    int count;
};

int TopicCount();
const Topic& GetTopic(int index);

// A round of questions on one topic: show a word's picture and Vietnamese
// meaning, pick its English word from kChoices. Every word is asked once before
// any repeats. Only a right first try scores; a wrong pick may be retried.
class Quiz {
public:
    static constexpr int kChoices = 3;
    static constexpr int kQuestions = 10;
    static constexpr int kMaxWords = 32;
    using Random = uint32_t (*)();

    enum class Result { kIgnored, kWrong, kRight };

    // `word_count` is the topic's size, at least kChoices and at most kMaxWords.
    void Start(int word_count, Random random);
    // After kRight the question stays answered (so it can be shown) until Next().
    Result Answer(int choice);
    // Moves on to the next question. Returns false when the round is over.
    bool Next();

    int Question() const { return question_; }  // 0-based, kQuestions when done
    int Asked() const { return word_; }         // Index of the word asked
    bool Answered() const { return answered_; }
    int Choice(int i) const { return choices_[i]; }  // Word index on button i
    bool Tried(int i) const { return tried_[i]; }
    int Score() const { return score_; }
    bool IsOver() const { return question_ >= kQuestions; }

private:
    Random random_ = nullptr;
    int word_count_ = 0;
    int order_[kMaxWords] = {};
    int next_in_order_ = 0;
    int question_ = 0;
    int word_ = 0;
    int choices_[kChoices] = {};
    bool tried_[kChoices] = {};
    bool missed_ = false;
    bool answered_ = false;
    int score_ = 0;

    void Ask();
    void Shuffle();
};

}  // namespace english
