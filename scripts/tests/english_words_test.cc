// Host test for main/apps/english_words.cc; built and run by test_english_words.py.
#include <cstdio>
#include <cstring>
#include <random>
#include <set>
#include <string>

#include "english_words.h"

using namespace english;

static int failures = 0;
#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAILED line %d: %s\n", __LINE__, #cond);       \
            ++failures;                                            \
        }                                                          \
    } while (0)

static std::mt19937 engine(7);
static uint32_t TestRandom() { return static_cast<uint32_t>(engine()); }

static void TestTopics() {
    CHECK(TopicCount() >= 6 && TopicCount() <= 20);  // The menu scrolls
    for (int t = 0; t < TopicCount(); ++t) {
        const Topic& topic = GetTopic(t);
        CHECK(topic.name != nullptr && topic.icon != nullptr);
        CHECK(topic.count >= Quiz::kChoices && topic.count <= Quiz::kMaxWords);
        std::set<std::string> english, vietnamese;
        for (int w = 0; w < topic.count; ++w) {
            const Word& word = topic.words[w];
            CHECK(word.en != nullptr && word.en[0] != '\0');
            CHECK(word.vi != nullptr && word.vi[0] != '\0');
            CHECK(strlen(word.en) <= 11);  // Fits an answer button
            // Quiz buttons must be told apart, and so must the prompts.
            CHECK(english.insert(word.en).second);
            CHECK(vietnamese.insert(word.vi).second);
            if (topic.picture == Picture::kIcon) CHECK(word.icon != nullptr);
            if (strlen(word.en) + 8 > 31) CHECK(false);  // "img_" + word + ".bin" fits an asset name
        }
        if (topic.picture == Picture::kNumber) CHECK(topic.count <= 10);  // One digit... or "10"
    }
}

static void TestQuizRound(int word_count) {
    Quiz quiz;
    quiz.Start(word_count, TestRandom);
    CHECK(quiz.Question() == 0 && quiz.Score() == 0 && !quiz.IsOver());

    std::set<int> asked;
    int previous = -1;
    int answer_positions[Quiz::kChoices] = {};
    int questions = 0;
    do {
        CHECK(!quiz.Answered());
        CHECK(quiz.Asked() != previous);  // Never the same word twice in a row
        previous = quiz.Asked();
        // Every word once before any repeats.
        if (questions < word_count) CHECK(asked.insert(quiz.Asked()).second);

        std::set<int> shown;
        int answer = -1;
        for (int i = 0; i < Quiz::kChoices; ++i) {
            CHECK(quiz.Choice(i) >= 0 && quiz.Choice(i) < word_count);
            CHECK(shown.insert(quiz.Choice(i)).second);  // Distinct buttons
            if (quiz.Choice(i) == quiz.Asked()) answer = i;
        }
        CHECK(answer >= 0);
        if (answer < 0) return;
        ++answer_positions[answer];

        // Odd questions: a wrong pick first, which costs the point.
        const bool miss = questions % 2 == 1;
        if (miss) {
            const int wrong = (answer + 1) % Quiz::kChoices;
            CHECK(quiz.Answer(wrong) == Quiz::Result::kWrong);
            CHECK(quiz.Tried(wrong));
            CHECK(quiz.Answer(wrong) == Quiz::Result::kIgnored);  // Already tried
            CHECK(!quiz.Next());  // Not answered yet
        }
        const int score = quiz.Score();
        CHECK(quiz.Answer(answer) == Quiz::Result::kRight);
        CHECK(quiz.Answered());
        CHECK(quiz.Score() == score + (miss ? 0 : 1));
        // Answered: the buttons hold still for the UI, and taps are ignored.
        CHECK(quiz.Choice(answer) == quiz.Asked());
        CHECK(quiz.Answer((answer + 2) % Quiz::kChoices) == Quiz::Result::kIgnored);
        ++questions;
    } while (quiz.Next());

    CHECK(questions == Quiz::kQuestions);
    CHECK(quiz.IsOver());
    CHECK(quiz.Score() == (Quiz::kQuestions + 1) / 2);
    CHECK(quiz.Answer(0) == Quiz::Result::kIgnored);
    CHECK(!quiz.Next());
    (void)answer_positions;
}

static void TestAnswerPositions() {
    // Over many questions the answer shows up on every button.
    Quiz quiz;
    int positions[Quiz::kChoices] = {};
    for (int round = 0; round < 20; ++round) {
        quiz.Start(10, TestRandom);
        do {
            for (int i = 0; i < Quiz::kChoices; ++i) {
                if (quiz.Choice(i) == quiz.Asked()) {
                    ++positions[i];
                    quiz.Answer(i);
                }
            }
        } while (quiz.Next());
    }
    for (int count : positions) CHECK(count > 20);
}

static void TestRestart() {
    Quiz quiz;
    quiz.Start(5, TestRandom);
    for (int i = 0; i < Quiz::kChoices; ++i) {
        if (quiz.Choice(i) == quiz.Asked()) quiz.Answer(i);
    }
    quiz.Next();
    quiz.Start(5, TestRandom);
    CHECK(quiz.Question() == 0 && quiz.Score() == 0 && !quiz.Answered());
}

int main() {
    TestTopics();
    for (int count : {3, 4, 10, 11, 15, Quiz::kMaxWords}) TestQuizRound(count);
    TestAnswerPositions();
    TestRestart();
    if (failures == 0) printf("OK\n");
    return failures == 0 ? 0 : 1;
}
