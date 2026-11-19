#include "games_app.h"

#include <cstdio>
#include <deque>
#include <string_view>
#include <utility>

#include <esp_random.h>

#include "application.h"
#include "assets/lang_config.h"
#include "game_logic.h"
#include "material_symbols.h"
#include "settings.h"
#include "ui_helpers.h"

namespace apps {

namespace {

constexpr uint32_t kStarColor = 0xFFCA28;
constexpr uint32_t kRobotColor = 0x4DD0E1;
constexpr uint32_t kCardBack = 0x37474F;
constexpr uint32_t kCellColor = 0x263238;
constexpr uint32_t kWinColor = 0x2E7D32;

uint32_t HardwareRandom() { return esp_random(); }

// Played from the main task: the sound queue may block, the touch UI must not.
void PlaySound(std::string_view sound) {
    auto& app = Application::GetInstance();
    app.Schedule([&app, sound]() { app.PlaySound(sound); });
}

lv_obj_t* MakeTextLabel(lv_obj_t* parent, const char* text = "") {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, text);
    return label;
}

}  // namespace

// One open game: a full-screen panel with a back button, a title and a short
// status line, plus a one-shot timer for its delayed moves.
class GameView {
public:
    GameView(lv_obj_t* parent, const char* title, std::function<void()> on_back)
        : on_back_(std::move(on_back)) {
        root_ = lv_obj_create(parent);
        PlainBox(root_);
        lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);

        lv_obj_t* bar = MakeTopBar(root_, MATERIAL_SYMBOLS_ARROW_BACK, [this]() { on_back_(); });
        lv_obj_align(MakeTextLabel(bar, title), LV_ALIGN_LEFT_MID, kTopBarTitleLeft, 0);
        info_ = MakeTextLabel(bar);
        lv_obj_align(info_, LV_ALIGN_RIGHT_MID, -kRightReserve, 0);
    }

    virtual ~GameView() {
        if (delay_ != nullptr) lv_timer_delete(delay_);
        lv_obj_delete(root_);  // Frees everything the game built on it
    }

protected:
    lv_obj_t* root_ = nullptr;
    lv_obj_t* info_ = nullptr;

    // A message over the board that does something when tapped. Starts hidden.
    lv_obj_t* MakeBanner(std::function<void()> on_tap) {
        lv_obj_t* banner = MakeTextLabel(root_);
        lv_obj_set_width(banner, 280);
        lv_obj_set_style_text_align(banner, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_color(banner, lv_color_hex(kWinColor), 0);
        lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(banner, 16, 0);
        lv_obj_set_style_radius(banner, 12, 0);
        lv_obj_add_flag(banner, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(banner, LV_ALIGN_CENTER, 0, kTopBar / 2);
        BindClick(banner, std::move(on_tap));
        return banner;
    }

    static void ShowBanner(lv_obj_t* banner, const char* text) {
        lv_label_set_text(banner, text);
        lv_obj_remove_flag(banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(banner);
    }

    void CancelDelay() {
        if (delay_ != nullptr) lv_timer_delete(delay_);
        delay_ = nullptr;
    }

    void After(uint32_t ms, std::function<void()> fn) {
        CancelDelay();
        delayed_ = std::move(fn);
        delay_ = lv_timer_create([](lv_timer_t* timer) {
            auto* self = static_cast<GameView*>(lv_timer_get_user_data(timer));
            self->delay_ = nullptr;  // A repeat count of 1 deletes the timer after this
            auto fn = std::move(self->delayed_);
            fn();
        }, ms, this);
        lv_timer_set_repeat_count(delay_, 1);
    }

private:
    std::function<void()> on_back_;
    std::function<void()> delayed_;
    lv_timer_t* delay_ = nullptr;
};

namespace {

// ── Tic-Tac-Toe: your star against the robot ──────────────────────────────
class TicTacToeView : public GameView {
public:
    TicTacToeView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Tic-Tac-Toe", std::move(on_back)) {
        const int cell = 56, gap = 4, left = 12, top = kTopBar + 4;
        for (int i = 0; i < games::TicTacToe::kCells; ++i) {
            lv_obj_t* button = lv_button_create(root_);
            lv_obj_set_size(button, cell, cell);
            lv_obj_set_pos(button, left + (i % 3) * (cell + gap), top + (i / 3) * (cell + gap));
            lv_obj_set_style_radius(button, 10, 0);
            lv_obj_set_style_shadow_width(button, 0, 0);
            lv_obj_set_style_pad_all(button, 0, 0);
            lv_obj_set_style_bg_color(button, lv_color_hex(kCellColor), 0);
            BindClick(button, [this, i]() { OnCell(i); });
            cells_[i] = button;
            marks_[i] = MakeIconLabel(button, "");
            lv_obj_center(marks_[i]);
        }

        const int panel = left + 3 * cell + 2 * gap + 8;
        status_ = MakeTextLabel(root_);
        lv_obj_set_pos(status_, panel, top + 4);
        lv_obj_set_width(status_, 320 - panel - 6);
        lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_WRAP);

        score_ = MakeTextLabel(root_);
        lv_obj_set_pos(score_, panel, top + 60);

        lv_obj_t* again = MakeIconButton(root_, MATERIAL_SYMBOLS_REFRESH, 88, 48, [this]() { NewRound(); });
        lv_obj_set_pos(again, panel, top + 124);

        NewRound();
    }

private:
    using Mark = games::TicTacToe::Mark;
    games::TicTacToe game_;
    lv_obj_t* cells_[games::TicTacToe::kCells];
    lv_obj_t* marks_[games::TicTacToe::kCells];
    lv_obj_t* status_;
    lv_obj_t* score_;
    int wins_ = 0;
    int losses_ = 0;
    int round_ = 0;
    bool robot_turn_ = false;

    void NewRound() {
        CancelDelay();  // The robot may be mid-move in the round being abandoned
        game_.Reset();
        for (int i = 0; i < games::TicTacToe::kCells; ++i) {
            lv_label_set_text(marks_[i], "");
            lv_obj_set_style_bg_color(cells_[i], lv_color_hex(kCellColor), 0);
        }
        ShowScore();
        // Take turns going first.
        if (round_++ % 2 == 0) {
            robot_turn_ = false;
            lv_label_set_text(status_, "Your turn");
        } else {
            RobotMove();
        }
    }

    void ShowScore() {
        char text[32];
        snprintf(text, sizeof(text), "You %d\nRobot %d", wins_, losses_);
        lv_label_set_text(score_, text);
    }

    void Place(int cell, Mark mark) {
        game_.Play(cell, mark);
        const bool player = mark == Mark::kPlayer;
        lv_label_set_text(marks_[cell], player ? MATERIAL_SYMBOLS_STAR : MATERIAL_SYMBOLS_ROBOT_2);
        lv_obj_set_style_text_color(marks_[cell], lv_color_hex(player ? kStarColor : kRobotColor), 0);
    }

    bool FinishIfOver() {
        if (!game_.IsOver()) return false;
        const Mark winner = game_.Winner();
        if (winner == Mark::kEmpty) {
            lv_label_set_text(status_, "Draw!");
            PlaySound(Lang::Sounds::OGG_POPUP);
        } else {
            for (int cell : game_.WinningLine()) {
                lv_obj_set_style_bg_color(cells_[cell], lv_color_hex(kWinColor), 0);
            }
            const bool won = winner == Mark::kPlayer;
            (won ? wins_ : losses_)++;
            lv_label_set_text(status_, won ? "You win!" : "Robot wins");
            PlaySound(won ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
            ShowScore();
        }
        return true;
    }

    void OnCell(int cell) {
        if (robot_turn_ || game_.IsOver() || game_.At(cell) != Mark::kEmpty) return;
        Place(cell, Mark::kPlayer);
        if (!FinishIfOver()) RobotMove();
    }

    void RobotMove() {
        robot_turn_ = true;
        lv_label_set_text(status_, "Robot is thinking");
        After(500, [this]() {
            // Misses a block one time in four, so a young player can win.
            Place(game_.ChooseComputerMove(HardwareRandom, 25), Mark::kComputer);
            robot_turn_ = false;
            if (!FinishIfOver()) lv_label_set_text(status_, "Your turn");
        });
    }
};

// ── Memory: find the six pairs ────────────────────────────────────────────
class MemoryView : public GameView {
public:
    MemoryView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Memory", std::move(on_back)) {
        const int w = 70, h = 56, gap = 4, left = (320 - (4 * w + 3 * gap)) / 2, top = kTopBar + 4;
        for (int i = 0; i < games::MemoryGame::kCards; ++i) {
            lv_obj_t* card = lv_button_create(root_);
            lv_obj_set_size(card, w, h);
            lv_obj_set_pos(card, left + (i % 4) * (w + gap), top + (i / 4) * (h + gap));
            lv_obj_set_style_radius(card, 10, 0);
            lv_obj_set_style_shadow_width(card, 0, 0);
            lv_obj_set_style_pad_all(card, 0, 0);
            BindClick(card, [this, i]() { OnCard(i); });
            cards_[i] = card;
            faces_[i] = MakeIconLabel(card, "");
            lv_obj_center(faces_[i]);
        }
        won_ = MakeBanner([this]() { NewGame(); });
        NewGame();
    }

private:
    struct Face {
        const char* icon;
        uint32_t color;
    };
    static constexpr Face kFaces[games::MemoryGame::kPairs] = {
        {MATERIAL_SYMBOLS_FAVORITE, 0xEF5350}, {MATERIAL_SYMBOLS_STAR, kStarColor},
        {MATERIAL_SYMBOLS_MUSIC_NOTE, 0xCE93D8}, {MATERIAL_SYMBOLS_ROBOT_2, kRobotColor},
        {MATERIAL_SYMBOLS_HEADPHONES, 0x81C784}, {MATERIAL_SYMBOLS_ALARM, 0xFFB74D},
    };
    games::MemoryGame game_;
    lv_obj_t* cards_[games::MemoryGame::kCards];
    lv_obj_t* faces_[games::MemoryGame::kCards];
    lv_obj_t* won_;

    void NewGame() {
        game_.Deal(HardwareRandom);
        for (int i = 0; i < games::MemoryGame::kCards; ++i) Show(i, false);
        lv_obj_add_flag(won_, LV_OBJ_FLAG_HIDDEN);
        ShowMoves();
    }

    void Show(int card, bool up) {
        lv_obj_set_style_bg_color(cards_[card], lv_color_hex(up ? kCellColor : kCardBack), 0);
        if (up) {
            const Face& face = kFaces[game_.Face(card)];
            lv_label_set_text(faces_[card], face.icon);
            lv_obj_set_style_text_color(faces_[card], lv_color_hex(face.color), 0);
        } else {
            lv_label_set_text(faces_[card], "");
        }
    }

    void ShowMoves() {
        char text[16];
        snprintf(text, sizeof(text), "%d", game_.Moves());
        lv_label_set_text(info_, text);
    }

    void OnCard(int card) {
        const auto result = game_.FlipCard(card);
        if (result == games::MemoryGame::Flip::kIgnored) return;
        Show(card, true);
        ShowMoves();
        if (result == games::MemoryGame::Flip::kMismatch) {
            After(800, [this]() {
                for (int hidden : game_.HideMismatch()) Show(hidden, false);
            });
        } else if (result == games::MemoryGame::Flip::kMatch) {
            if (game_.IsWon()) {
                ShowBanner(won_, "You found them all!\nTap to play again");
                PlaySound(Lang::Sounds::OGG_SUCCESS);
            } else {
                PlaySound(Lang::Sounds::OGG_POPUP);
            }
        }
    }
};

// ── Snake: tap where you want it to turn ──────────────────────────────────
class SnakeView : public GameView {
public:
    SnakeView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Snake", std::move(on_back)), game_(kCols, kRows) {
        best_ = Settings("games").GetInt("snake_best", 0);

        area_ = lv_obj_create(root_);
        PlainBox(area_);
        lv_obj_set_size(area_, kCols * kCell, kRows * kCell);
        lv_obj_set_pos(area_, (320 - kCols * kCell) / 2, kTopBar + 2);
        lv_obj_set_style_bg_color(area_, lv_color_hex(0x0B0F12), 0);
        lv_obj_add_flag(area_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(area_, OnPressed, LV_EVENT_PRESSED, this);

        food_ = MakeBlock(0xEF5350);
        lv_obj_add_flag(food_, LV_OBJ_FLAG_HIDDEN);

        message_ = MakeTextLabel(area_);
        lv_obj_set_style_text_align(message_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(message_);
        lv_label_set_text(message_, "Tap above, below or beside\nthe snake to turn\n\nTap to start");
        ShowScore();
    }

    ~SnakeView() override {
        if (tick_ != nullptr) lv_timer_delete(tick_);
    }

private:
    static constexpr int kCell = 16;
    static constexpr int kCols = 19;
    static constexpr int kRows = 11;
    enum class Phase { kReady, kPlaying, kOver };

    games::SnakeGame game_;
    lv_obj_t* area_ = nullptr;
    lv_obj_t* food_ = nullptr;
    lv_obj_t* message_ = nullptr;
    std::deque<lv_obj_t*> segments_;  // Head first, matching game_.Body()
    lv_timer_t* tick_ = nullptr;
    Phase phase_ = Phase::kReady;
    int best_ = 0;
    bool can_restart_ = false;

    lv_obj_t* MakeBlock(uint32_t color) {
        lv_obj_t* block = lv_obj_create(area_);
        PlainBox(block);
        lv_obj_remove_flag(block, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(block, kCell - 2, kCell - 2);
        lv_obj_set_style_radius(block, 4, 0);
        lv_obj_set_style_bg_color(block, lv_color_hex(color), 0);
        return block;
    }

    static void Put(lv_obj_t* block, const games::SnakeGame::Cell& cell) {
        lv_obj_set_pos(block, cell.x * kCell + 1, cell.y * kCell + 1);
    }

    void ShowScore() {
        char text[24];
        snprintf(text, sizeof(text), "%d/%d", game_.Body().empty() ? 0 : game_.Score(), best_);
        lv_label_set_text(info_, text);
    }

    static void OnPressed(lv_event_t* e) {
        static_cast<SnakeView*>(lv_event_get_user_data(e))->HandlePress();
    }

    void HandlePress() {
        if (phase_ == Phase::kReady || (phase_ == Phase::kOver && can_restart_)) {
            Start();
        } else if (phase_ == Phase::kPlaying) {
            lv_point_t point;
            lv_indev_get_point(lv_indev_active(), &point);
            lv_area_t area;
            lv_obj_get_coords(area_, &area);
            const auto& head = game_.Body().front();
            game_.TurnToward(point.x - area.x1 - (head.x * kCell + kCell / 2),
                             point.y - area.y1 - (head.y * kCell + kCell / 2));
        }
    }

    void Start() {
        for (lv_obj_t* segment : segments_) lv_obj_delete(segment);
        segments_.clear();
        game_.Reset(HardwareRandom);
        for (const auto& cell : game_.Body()) {
            lv_obj_t* segment = MakeBlock(0x66BB6A);
            Put(segment, cell);
            segments_.push_back(segment);
        }
        Put(food_, game_.Food());
        lv_obj_remove_flag(food_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
        phase_ = Phase::kPlaying;
        can_restart_ = false;
        ShowScore();
        tick_ = lv_timer_create([](lv_timer_t* timer) {
            static_cast<SnakeView*>(lv_timer_get_user_data(timer))->Tick();
        }, PeriodMs(), this);
    }

    // Speeds up as the snake grows, down to a limit a child can still steer.
    int PeriodMs() const {
        const int period = 280 - game_.Score() * 8;
        return period < 130 ? 130 : period;
    }

    void Tick() {
        const auto step = game_.Advance(HardwareRandom);
        switch (step) {
            case games::SnakeGame::Step::kMoved: {
                // Reuse the tail block as the new head: only two cells redraw.
                lv_obj_t* tail = segments_.back();
                segments_.pop_back();
                Put(tail, game_.Body().front());
                segments_.push_front(tail);
                break;
            }
            case games::SnakeGame::Step::kAte: {
                lv_obj_t* head = MakeBlock(0x66BB6A);
                Put(head, game_.Body().front());
                segments_.push_front(head);
                Put(food_, game_.Food());
                lv_timer_set_period(tick_, PeriodMs());
                ShowScore();
                PlaySound(Lang::Sounds::OGG_POPUP);
                break;
            }
            case games::SnakeGame::Step::kDied:
            case games::SnakeGame::Step::kWon:
                GameOver(step == games::SnakeGame::Step::kWon);
                break;
        }
    }

    void GameOver(bool won) {
        lv_timer_delete(tick_);
        tick_ = nullptr;
        phase_ = Phase::kOver;
        const int score = game_.Score();
        const bool record = score > best_;
        if (record) {
            best_ = score;
            Settings("games", true).SetInt("snake_best", best_);
        }
        ShowScore();
        char text[64];
        snprintf(text, sizeof(text), "%s\nScore %d%s\n\nTap to play again",
                 won ? "You filled the board!" : "Game over", score, record ? " - new best!" : "");
        lv_label_set_text(message_, text);
        lv_obj_remove_flag(message_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(message_);
        PlaySound(won || record ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
        // The finger that was steering is probably still tapping: don't restart
        // the game before the message has been seen.
        After(800, [this]() { can_restart_ = true; });
    }
};

// ── Copy Me: watch the pads light up, then repeat them ────────────────────
class CopyView : public GameView {
public:
    CopyView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Copy Me", std::move(on_back)) {
        best_ = Settings("games").GetInt("copy_best", 0);
        static constexpr const char* kIcons[games::CopyGame::kPads] = {
            MATERIAL_SYMBOLS_STAR, MATERIAL_SYMBOLS_FAVORITE, MATERIAL_SYMBOLS_MUSIC_NOTE,
            MATERIAL_SYMBOLS_ROBOT_2};
        static constexpr uint32_t kColors[games::CopyGame::kPads] = {0xF9A825, 0xD81B60, 0x8E24AA,
                                                                     0x00897B};
        const int w = 148, h = 86, gap = 8, top = kTopBar + 4;
        for (int i = 0; i < games::CopyGame::kPads; ++i) {
            lv_obj_t* pad = lv_button_create(root_);
            lv_obj_set_size(pad, w, h);
            lv_obj_set_pos(pad, gap + (i % 2) * (w + gap), top + (i / 2) * (h + 4));
            lv_obj_set_style_radius(pad, 14, 0);
            lv_obj_set_style_shadow_width(pad, 0, 0);
            lv_obj_set_style_bg_color(pad, lv_color_hex(kColors[i]), 0);
            lv_obj_set_style_bg_color(pad, lv_color_hex(kColors[i]), LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, LV_STATE_PRESSED);
            lv_obj_center(MakeIconLabel(pad, kIcons[i]));
            BindEvent(pad, LV_EVENT_PRESSED, [this, i]() { OnPad(i); });
            pads_[i] = pad;
        }
        SetInput(false);
        banner_ = MakeBanner([this]() {
            if (can_start_) Start();
        });
        ShowBanner(banner_, "Watch the pads, then copy them\n\nTap to start");
        ShowScore();
    }

private:
    static constexpr lv_opa_t kDim = LV_OPA_30;
    static constexpr lv_opa_t kReady = LV_OPA_80;

    games::CopyGame game_;
    lv_obj_t* pads_[games::CopyGame::kPads];
    lv_obj_t* banner_;
    int best_ = 0;
    bool can_start_ = true;

    void ShowScore() {
        char text[24];
        snprintf(text, sizeof(text), "%d/%d", game_.RoundsDone(), best_);
        lv_label_set_text(info_, text);
    }

    // Bright pads that respond to touch mean it is the player's turn.
    void SetInput(bool on) {
        for (lv_obj_t* pad : pads_) {
            lv_obj_set_style_bg_opa(pad, on ? kReady : kDim, 0);
            if (on) {
                lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
            } else {
                lv_obj_remove_flag(pad, LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }

    void Start() {
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
        game_.Reset(HardwareRandom);
        ShowScore();
        After(500, [this]() { ShowStep(0); });
    }

    // Starts slow, and gets quicker as the sequence gets longer.
    int LitMs() const {
        const int ms = 560 - game_.Length() * 25;
        return ms < 240 ? 240 : ms;
    }

    void ShowStep(int index) {
        if (index == game_.Length()) {
            SetInput(true);
            return;
        }
        lv_obj_t* pad = pads_[game_.Step(index)];
        lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
        After(LitMs(), [this, pad, index]() {
            lv_obj_set_style_bg_opa(pad, kDim, 0);
            // The gap shows a repeated pad as two flashes.
            After(180, [this, index]() { ShowStep(index + 1); });
        });
    }

    void OnPad(int pad) {
        using Result = games::CopyGame::Result;
        switch (game_.Press(pad)) {
            case Result::kRoundDone:
                SetInput(false);
                ShowScore();
                PlaySound(Lang::Sounds::OGG_POPUP);
                After(800, [this]() {
                    game_.NextRound(HardwareRandom);
                    ShowStep(0);
                });
                break;
            case Result::kWrong:
                GameOver(false);
                break;
            case Result::kWon:
                GameOver(true);
                break;
            case Result::kRight:
            case Result::kIgnored:
                break;
        }
    }

    void GameOver(bool won) {
        SetInput(false);
        const int score = game_.RoundsDone();
        const bool record = score > best_;
        if (record) {
            best_ = score;
            Settings("games", true).SetInt("copy_best", best_);
        }
        ShowScore();
        char text[72];
        snprintf(text, sizeof(text), "%s\nRounds %d%s\n\nTap to play again",
                 won ? "You copied them all!" : "Oops!", score, record ? " - new best!" : "");
        ShowBanner(banner_, text);
        PlaySound(won || record ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
        // The finger that was copying may still be tapping.
        can_start_ = false;
        After(800, [this]() { can_start_ = true; });
    }
};

// ── Bop the Bot: tap the robots before they duck away ─────────────────────
class BopView : public GameView {
public:
    BopView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Bop!", std::move(on_back)) {
        best_ = Settings("games").GetInt("bop_best", 0);
        const int w = 98, h = 56, gap = 4, left = 9, top = kTopBar + 6;

        time_bar_ = lv_obj_create(root_);
        PlainBox(time_bar_);
        lv_obj_remove_flag(time_bar_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(time_bar_, 0, kTopBar);
        lv_obj_set_size(time_bar_, 320, 4);
        lv_obj_set_style_bg_color(time_bar_, lv_color_hex(kStarColor), 0);

        for (int i = 0; i < games::BopGame::kCells; ++i) {
            lv_obj_t* cell = lv_button_create(root_);
            lv_obj_set_size(cell, w, h);
            lv_obj_set_pos(cell, left + (i % 3) * (w + gap), top + (i / 3) * (h + gap));
            lv_obj_set_style_radius(cell, 12, 0);
            lv_obj_set_style_shadow_width(cell, 0, 0);
            lv_obj_set_style_pad_all(cell, 0, 0);
            lv_obj_set_style_bg_color(cell, lv_color_hex(kCellColor), 0);
            lv_obj_set_style_bg_color(cell, lv_color_hex(kCellColor), LV_STATE_PRESSED);
            robots_[i] = MakeIconLabel(cell, MATERIAL_SYMBOLS_ROBOT_2);
            lv_obj_set_style_text_color(robots_[i], lv_color_hex(kRobotColor), 0);
            lv_obj_add_flag(robots_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_center(robots_[i]);
            BindEvent(cell, LV_EVENT_PRESSED, [this, i]() { OnCell(i); });
        }
        banner_ = MakeBanner([this]() {
            if (can_start_) Start();
        });
        ShowBanner(banner_, "Tap the robots before they hide!\n\nTap to start");
        ShowScore();
    }

    ~BopView() override {
        if (tick_ != nullptr) lv_timer_delete(tick_);
    }

private:
    static constexpr int kTickMs = 100;

    games::BopGame game_;
    lv_obj_t* time_bar_;
    lv_obj_t* robots_[games::BopGame::kCells];
    lv_obj_t* banner_;
    lv_timer_t* tick_ = nullptr;
    int best_ = 0;
    bool can_start_ = true;

    void ShowScore() {
        char text[24];
        snprintf(text, sizeof(text), "%d/%d", game_.Score(), best_);
        lv_label_set_text(info_, text);
    }

    void ShowRobots() {
        for (int i = 0; i < games::BopGame::kCells; ++i) {
            // Only touch the robots that changed: each change redraws its cell.
            const bool hidden = lv_obj_has_flag(robots_[i], LV_OBJ_FLAG_HIDDEN);
            if (game_.IsUp(i) && hidden) {
                lv_obj_remove_flag(robots_[i], LV_OBJ_FLAG_HIDDEN);
            } else if (!game_.IsUp(i) && !hidden) {
                lv_obj_add_flag(robots_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    void Start() {
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
        game_.Reset();
        ShowScore();
        lv_obj_set_width(time_bar_, 320);
        tick_ = lv_timer_create([](lv_timer_t* timer) {
            static_cast<BopView*>(lv_timer_get_user_data(timer))->Tick();
        }, kTickMs, this);
    }

    void Tick() {
        game_.Advance(kTickMs, HardwareRandom);
        ShowRobots();
        lv_obj_set_width(time_bar_, 320 * game_.TimeLeftMs() / games::BopGame::kRoundMs);
        if (game_.IsOver()) GameOver();
    }

    void OnCell(int cell) {
        if (game_.Bop(cell)) {
            lv_obj_add_flag(robots_[cell], LV_OBJ_FLAG_HIDDEN);
            ShowScore();
            PlaySound(Lang::Sounds::OGG_POPUP);
        }
    }

    void GameOver() {
        lv_timer_delete(tick_);
        tick_ = nullptr;
        const int score = game_.Score();
        const bool record = score > best_;
        if (record) {
            best_ = score;
            Settings("games", true).SetInt("bop_best", best_);
        }
        ShowScore();
        char text[72];
        snprintf(text, sizeof(text), "Time's up!\nYou bopped %d%s\n\nTap to play again", score,
                 record ? " - new best!" : "");
        ShowBanner(banner_, text);
        PlaySound(Lang::Sounds::OGG_SUCCESS);
        // The finger that was bopping is probably still tapping.
        can_start_ = false;
        After(800, [this]() { can_start_ = true; });
    }
};

// ── Slide Puzzle: put the numbers back in order ───────────────────────────
class SlideView : public GameView {
public:
    SlideView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Slide Puzzle", std::move(on_back)) {
        const int w = 98, h = 56, gap = 4, left = 9, top = kTopBar + 4;
        for (int i = 0; i < games::SlidePuzzle::kCells; ++i) {
            lv_obj_t* tile = lv_button_create(root_);
            lv_obj_set_size(tile, w, h);
            lv_obj_set_pos(tile, left + (i % 3) * (w + gap), top + (i / 3) * (h + gap));
            lv_obj_set_style_radius(tile, 12, 0);
            lv_obj_set_style_shadow_width(tile, 0, 0);
            lv_obj_set_style_pad_all(tile, 0, 0);
            lv_obj_set_style_bg_color(tile, lv_color_hex(0x1565C0), 0);
            numbers_[i] = MakeTextLabel(tile);
            lv_obj_center(numbers_[i]);
            BindClick(tile, [this, i]() { OnTile(i); });
            tiles_[i] = tile;
        }
        won_ = MakeBanner([this]() { NewGame(); });
        NewGame();
    }

private:
    games::SlidePuzzle game_;
    lv_obj_t* tiles_[games::SlidePuzzle::kCells];
    lv_obj_t* numbers_[games::SlidePuzzle::kCells];
    lv_obj_t* won_;

    void NewGame() {
        game_.Shuffle(HardwareRandom);
        lv_obj_add_flag(won_, LV_OBJ_FLAG_HIDDEN);
        ShowBoard();
    }

    void ShowBoard() {
        for (int i = 0; i < games::SlidePuzzle::kCells; ++i) {
            const int number = game_.At(i);
            if (number == 0) {
                lv_obj_add_flag(tiles_[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(tiles_[i], LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(numbers_[i], "%d", number);
            }
        }
        lv_label_set_text_fmt(info_, "%d", game_.Moves());
    }

    void OnTile(int cell) {
        if (!game_.Slide(cell)) return;
        ShowBoard();
        if (game_.IsSolved()) {
            char text[56];
            snprintf(text, sizeof(text), "Solved in %d moves!\nTap to play again", game_.Moves());
            ShowBanner(won_, text);
            PlaySound(Lang::Sounds::OGG_SUCCESS);
        }
    }
};

// ── Four in a Row: your yellow discs against the robot's blue ones ────────
class FourView : public GameView {
public:
    FourView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Four in a Row", std::move(on_back)) {
        const int top = kTopBar + 2;
        for (int col = 0; col < kCols; ++col) {
            // The whole column is the touch target: the discs are too small.
            lv_obj_t* column = lv_button_create(root_);
            lv_obj_set_size(column, kColumn, kRows * kDisc);
            lv_obj_set_pos(column, kLeft + col * kColumn, top);
            lv_obj_set_style_radius(column, 0, 0);
            lv_obj_set_style_shadow_width(column, 0, 0);
            lv_obj_set_style_pad_all(column, 0, 0);
            lv_obj_set_style_bg_opa(column, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(column, lv_color_hex(kCellColor), LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(column, LV_OPA_COVER, LV_STATE_PRESSED);
            BindClick(column, [this, col]() { OnColumn(col); });
            for (int row = 0; row < kRows; ++row) {
                lv_obj_t* disc = lv_obj_create(column);
                PlainBox(disc);
                lv_obj_remove_flag(disc, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_size(disc, kDisc - 4, kDisc - 4);
                lv_obj_set_pos(disc, (kColumn - kDisc + 4) / 2, (kRows - 1 - row) * kDisc + 2);
                lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_border_color(disc, lv_color_white(), 0);
                discs_[col][row] = disc;
            }
        }

        const int panel = kLeft + kCols * kColumn + 8;
        status_ = MakeTextLabel(root_);
        lv_obj_set_pos(status_, panel, top + 2);
        lv_obj_set_width(status_, 320 - panel - 4);
        lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_WRAP);
        score_ = MakeTextLabel(root_);
        lv_obj_set_pos(score_, panel, top + 68);
        lv_obj_t* again = MakeIconButton(root_, MATERIAL_SYMBOLS_REFRESH, 76, 48, [this]() { NewRound(); });
        lv_obj_set_pos(again, panel, top + 128);
        NewRound();
    }

private:
    using Game = games::FourInARow;
    using Mark = Game::Mark;
    static constexpr int kCols = Game::kCols;
    static constexpr int kRows = Game::kRows;
    static constexpr int kDisc = 30;
    static constexpr int kColumn = 32;
    static constexpr int kLeft = 4;

    Game game_;
    lv_obj_t* discs_[kCols][kRows];
    lv_obj_t* status_;
    lv_obj_t* score_;
    int wins_ = 0;
    int losses_ = 0;
    int round_ = 0;
    bool robot_turn_ = false;

    void NewRound() {
        CancelDelay();  // The robot may be mid-move in the round being abandoned
        game_.Reset();
        for (auto& column : discs_) {
            for (lv_obj_t* disc : column) {
                lv_obj_set_style_bg_color(disc, lv_color_hex(kCellColor), 0);
                lv_obj_set_style_border_width(disc, 0, 0);
            }
        }
        ShowScore();
        // Take turns going first.
        if (round_++ % 2 == 0) {
            robot_turn_ = false;
            lv_label_set_text(status_, "Your turn");
        } else {
            RobotMove();
        }
    }

    void ShowScore() {
        char text[32];
        snprintf(text, sizeof(text), "You %d\nRobot %d", wins_, losses_);
        lv_label_set_text(score_, text);
    }

    bool Place(int col, Mark mark) {
        const int row = game_.Drop(col, mark);
        if (row < 0) return false;
        const uint32_t color = mark == Mark::kPlayer ? kStarColor : kRobotColor;
        lv_obj_set_style_bg_color(discs_[col][row], lv_color_hex(color), 0);
        return true;
    }

    bool FinishIfOver() {
        if (!game_.IsOver()) return false;
        const Mark winner = game_.Winner();
        if (winner == Mark::kEmpty) {
            lv_label_set_text(status_, "Draw!");
            PlaySound(Lang::Sounds::OGG_POPUP);
            return true;
        }
        for (const auto& cell : game_.WinningLine()) {
            lv_obj_set_style_border_width(discs_[cell.col][cell.row], 3, 0);
        }
        const bool won = winner == Mark::kPlayer;
        (won ? wins_ : losses_)++;
        lv_label_set_text(status_, won ? "You win!" : "Robot wins");
        PlaySound(won ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
        ShowScore();
        return true;
    }

    void OnColumn(int col) {
        if (robot_turn_ || !Place(col, Mark::kPlayer)) return;
        if (!FinishIfOver()) RobotMove();
    }

    void RobotMove() {
        robot_turn_ = true;
        lv_label_set_text(status_, "Robot is thinking");
        After(500, [this]() {
            // Misses a block one time in five, so a young player can win.
            Place(game_.ChooseComputerMove(HardwareRandom, 20), Mark::kComputer);
            robot_turn_ = false;
            if (!FinishIfOver()) lv_label_set_text(status_, "Your turn");
        });
    }
};

// ── Quick Sums: pick the right answer ─────────────────────────────────────
class SumsView : public GameView {
public:
    SumsView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Quick Sums", std::move(on_back)) {
        question_ = MakeTextLabel(root_);
        SetBigFont(question_);
        lv_obj_align(question_, LV_ALIGN_TOP_MID, 0, kTopBar + 36);

        const int w = 98, h = 72, gap = 4, left = 9, top = kTopBar + 100;
        for (int i = 0; i < games::SumsGame::kChoices; ++i) {
            lv_obj_t* button = lv_button_create(root_);
            lv_obj_set_size(button, w, h);
            lv_obj_set_pos(button, left + i * (w + gap), top);
            lv_obj_set_style_radius(button, 12, 0);
            lv_obj_set_style_shadow_width(button, 0, 0);
            lv_obj_set_style_pad_all(button, 0, 0);
            answers_[i] = MakeTextLabel(button);
            SetBigFont(answers_[i]);
            lv_obj_center(answers_[i]);
            BindClick(button, [this, i]() { OnAnswer(i); });
            buttons_[i] = button;
        }
        banner_ = MakeBanner([this]() { NewRound(); });
        NewRound();
    }

private:
    static constexpr uint32_t kAnswerColor = 0x1565C0;
    static constexpr uint32_t kWrongColor = 0xC62828;

    games::SumsGame game_;
    lv_obj_t* question_;
    lv_obj_t* buttons_[games::SumsGame::kChoices];
    lv_obj_t* answers_[games::SumsGame::kChoices];
    lv_obj_t* banner_;

    static void SetBigFont(lv_obj_t* label) {
#if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
#else
        (void)label;
#endif
    }

    void NewRound() {
        CancelDelay();
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
        game_.Start(HardwareRandom);
        ShowQuestion();
    }

    void ShowQuestion() {
        lv_label_set_text_fmt(question_, "%d %c %d = ?", game_.A(), game_.IsPlus() ? '+' : '-', game_.B());
        for (int i = 0; i < games::SumsGame::kChoices; ++i) {
            lv_label_set_text_fmt(answers_[i], "%d", game_.Choice(i));
            lv_obj_set_style_bg_color(buttons_[i], lv_color_hex(kAnswerColor), 0);
        }
        lv_label_set_text_fmt(info_, "%d/%d", game_.Question() + 1, games::SumsGame::kQuestions);
    }

    void OnAnswer(int choice) {
        const auto result = game_.Answer(choice);
        if (result == games::SumsGame::Result::kIgnored) return;
        if (result == games::SumsGame::Result::kWrong) {
            lv_obj_set_style_bg_color(buttons_[choice], lv_color_hex(kWrongColor), 0);
            PlaySound(Lang::Sounds::OGG_VIBRATION);
            return;
        }
        lv_obj_set_style_bg_color(buttons_[choice], lv_color_hex(kWinColor), 0);
        lv_label_set_text_fmt(question_, "%d %c %d = %d", game_.A(), game_.IsPlus() ? '+' : '-', game_.B(),
                              game_.Total());
        PlaySound(Lang::Sounds::OGG_POPUP);
        After(900, [this]() {
            if (game_.Next(HardwareRandom)) {
                ShowQuestion();
            } else {
                Finish();
            }
        });
    }

    void Finish() {
        char text[64];
        snprintf(text, sizeof(text), "You got %d out of %d right!\n\nTap to play again", game_.Score(),
                 games::SumsGame::kQuestions);
        ShowBanner(banner_, text);
        PlaySound(Lang::Sounds::OGG_SUCCESS);
    }
};

// ── Catch the Stars: slide the basket, dodge the red marks ────────────────
class CatchView : public GameView {
public:
    CatchView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Catch Stars", std::move(on_back)) {
        best_ = Settings("games").GetInt("catch_best", 0);

        area_ = lv_obj_create(root_);
        PlainBox(area_);
        lv_obj_set_size(area_, Game::kWidth, Game::kHeight);
        lv_obj_set_pos(area_, 0, kTopBar + 4);
        lv_obj_set_style_bg_color(area_, lv_color_hex(0x0B0F12), 0);
        lv_obj_add_flag(area_, LV_OBJ_FLAG_CLICKABLE);
        BindEvent(area_, LV_EVENT_PRESSED, [this]() { OnTouch(); });
        BindEvent(area_, LV_EVENT_PRESSING, [this]() { OnTouch(); });

        for (int i = 0; i < Game::kStartLives; ++i) {
            lives_[i] = MakeIconLabel(area_, MATERIAL_SYMBOLS_FAVORITE);
            lv_obj_set_style_text_color(lives_[i], lv_color_hex(0xEF5350), 0);
            lv_obj_set_style_text_opa(lives_[i], LV_OPA_50, 0);
            lv_obj_set_pos(lives_[i], 2 + i * 32, 0);
        }
        basket_ = lv_obj_create(area_);
        PlainBox(basket_);
        lv_obj_remove_flag(basket_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(basket_, Game::kBasketWidth, Game::kHeight - Game::kBasketTop);
        lv_obj_set_style_bg_color(basket_, lv_color_hex(kRobotColor), 0);
        lv_obj_set_style_radius(basket_, 8, 0);
        for (int i = 0; i < Game::kMaxItems; ++i) {
            items_[i] = MakeIconLabel(area_, MATERIAL_SYMBOLS_STAR);
            lv_obj_add_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
        }
        banner_ = MakeBanner([this]() {
            if (can_start_) Start();
        });
        game_.Reset();
        ShowField();
        ShowBanner(banner_, "Slide the basket to catch the stars.\nDodge the red marks!\n\nTap to start");
        ShowScore();
    }

    ~CatchView() override {
        if (tick_ != nullptr) lv_timer_delete(tick_);
    }

private:
    using Game = games::CatchGame;
    static constexpr int kTickMs = 40;
    enum class Shown { kHidden, kStar, kBad };

    Game game_;
    lv_obj_t* area_;
    lv_obj_t* lives_[Game::kStartLives];
    lv_obj_t* basket_;
    lv_obj_t* items_[Game::kMaxItems];
    Shown shown_[Game::kMaxItems] = {};
    lv_obj_t* banner_;
    lv_timer_t* tick_ = nullptr;
    int best_ = 0;
    bool can_start_ = true;

    void ShowScore() {
        char text[24];
        snprintf(text, sizeof(text), "%d/%d", game_.Score(), best_);
        lv_label_set_text(info_, text);
    }

    void ShowField() {
        lv_obj_set_pos(basket_, game_.BasketLeft(), Game::kBasketTop);
        for (int i = 0; i < Game::kStartLives; ++i) {
            if (i < game_.Lives()) {
                lv_obj_remove_flag(lives_[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(lives_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        for (int i = 0; i < Game::kMaxItems; ++i) {
            const auto& item = game_.At(i);
            const Shown wanted = !item.active ? Shown::kHidden : item.bad ? Shown::kBad : Shown::kStar;
            if (wanted != shown_[i]) {
                shown_[i] = wanted;
                if (wanted == Shown::kHidden) {
                    lv_obj_add_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    const bool bad = wanted == Shown::kBad;
                    lv_label_set_text(items_[i], bad ? MATERIAL_SYMBOLS_CANCEL : MATERIAL_SYMBOLS_STAR);
                    lv_obj_set_style_text_color(items_[i], lv_color_hex(bad ? 0xEF5350 : kStarColor), 0);
                    lv_obj_remove_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            if (item.active) lv_obj_set_pos(items_[i], item.x, item.y_milli / 1000);
        }
    }

    void Start() {
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
        game_.Reset();
        ShowScore();
        ShowField();
        tick_ = lv_timer_create([](lv_timer_t* timer) {
            static_cast<CatchView*>(lv_timer_get_user_data(timer))->Tick();
        }, kTickMs, this);
    }

    void OnTouch() {
        if (tick_ == nullptr) return;
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        game_.MoveBasket(point.x);
    }

    void Tick() {
        const auto events = game_.Advance(kTickMs, HardwareRandom);
        ShowField();
        if (events.caught > 0) {
            ShowScore();
            PlaySound(Lang::Sounds::OGG_POPUP);
        }
        if (events.lost > 0 && !game_.IsOver()) PlaySound(Lang::Sounds::OGG_VIBRATION);
        if (game_.IsOver()) GameOver();
    }

    void GameOver() {
        lv_timer_delete(tick_);
        tick_ = nullptr;
        const int score = game_.Score();
        const bool record = score > best_;
        if (record) {
            best_ = score;
            Settings("games", true).SetInt("catch_best", best_);
        }
        ShowScore();
        char text[72];
        snprintf(text, sizeof(text), "You caught %d stars%s\n\nTap to play again", score,
                 record ? " - new best!" : "");
        ShowBanner(banner_, text);
        PlaySound(record ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
        // The finger on the basket is probably still down.
        can_start_ = false;
        After(800, [this]() { can_start_ = true; });
    }
};

// ── Robot Hop: tap to jump over the blocks ────────────────────────────────
class HopView : public GameView {
public:
    HopView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Robot Hop", std::move(on_back)) {
        best_ = Settings("games").GetInt("hop_best", 0);

        area_ = lv_obj_create(root_);
        PlainBox(area_);
        lv_obj_set_size(area_, Game::kWidth, kAreaHeight);
        lv_obj_set_pos(area_, 0, kTopBar + 4);
        lv_obj_set_style_bg_color(area_, lv_color_hex(0x0B0F12), 0);
        lv_obj_add_flag(area_, LV_OBJ_FLAG_CLICKABLE);
        BindEvent(area_, LV_EVENT_PRESSED, [this]() {
            if (tick_ != nullptr) game_.Jump();
        });

        lv_obj_t* ground = lv_obj_create(area_);
        PlainBox(ground);
        lv_obj_remove_flag(ground, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(ground, Game::kWidth, kAreaHeight - kGround);
        lv_obj_set_pos(ground, 0, kGround);
        lv_obj_set_style_bg_color(ground, lv_color_hex(0x37474F), 0);

        for (auto& block : blocks_) {
            block = lv_obj_create(area_);
            PlainBox(block);
            lv_obj_remove_flag(block, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(block, lv_color_hex(0xEF6C00), 0);
            lv_obj_set_style_radius(block, 4, 0);
            lv_obj_add_flag(block, LV_OBJ_FLAG_HIDDEN);
        }
        robot_ = MakeIconLabel(area_, MATERIAL_SYMBOLS_ROBOT_2);
        lv_obj_set_style_text_color(robot_, lv_color_hex(kRobotColor), 0);
        banner_ = MakeBanner([this]() {
            if (can_start_) Start();
        });
        ShowRobot();
        ShowBanner(banner_, "Tap to jump over the blocks!\n\nTap to start");
        ShowScore();
    }

    ~HopView() override {
        if (tick_ != nullptr) lv_timer_delete(tick_);
    }

private:
    using Game = games::HopGame;
    static constexpr int kTickMs = 33;
    static constexpr int kAreaHeight = 180;
    static constexpr int kGround = 150;

    Game game_;
    lv_obj_t* area_;
    lv_obj_t* robot_;
    lv_obj_t* blocks_[Game::kMaxBlocks];
    lv_obj_t* banner_;
    lv_timer_t* tick_ = nullptr;
    int best_ = 0;
    bool can_start_ = true;

    void ShowScore() {
        char text[24];
        snprintf(text, sizeof(text), "%d/%d", game_.Score(), best_);
        lv_label_set_text(info_, text);
    }

    void ShowRobot() {
        lv_obj_set_pos(robot_, Game::kRobotX, kGround - Game::kRobotSize - game_.RobotHeight());
    }

    void ShowBlocks() {
        for (int i = 0; i < Game::kMaxBlocks; ++i) {
            const auto& block = game_.At(i);
            if (!block.active) {
                lv_obj_add_flag(blocks_[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_set_size(blocks_[i], Game::kBlockWidth, block.height);
            lv_obj_set_pos(blocks_[i], block.x_milli / 1000, kGround - block.height);
            lv_obj_remove_flag(blocks_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    void Start() {
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
        game_.Reset(HardwareRandom);
        ShowScore();
        ShowRobot();
        ShowBlocks();
        tick_ = lv_timer_create([](lv_timer_t* timer) {
            static_cast<HopView*>(lv_timer_get_user_data(timer))->Tick();
        }, kTickMs, this);
    }

    void Tick() {
        const int score = game_.Score();
        const bool alive = game_.Advance(kTickMs, HardwareRandom);
        ShowRobot();
        ShowBlocks();
        if (game_.Score() != score) ShowScore();
        if (!alive) GameOver();
    }

    void GameOver() {
        lv_timer_delete(tick_);
        tick_ = nullptr;
        const int score = game_.Score();
        const bool record = score > best_;
        if (record) {
            best_ = score;
            Settings("games", true).SetInt("hop_best", best_);
        }
        ShowScore();
        char text[72];
        snprintf(text, sizeof(text), "Bump! You cleared %d%s\n\nTap to play again", score,
                 record ? " - new best!" : "");
        ShowBanner(banner_, text);
        PlaySound(record ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
        // A late tap meant as a jump must not restart the game.
        can_start_ = false;
        After(800, [this]() { can_start_ = true; });
    }
};

// ── Odd One Out: find the tile with a different color ─────────────────────
class OddOneView : public GameView {
public:
    OddOneView(lv_obj_t* parent, std::function<void()> on_back)
        : GameView(parent, "Odd One Out", std::move(on_back)) {
        best_ = Settings("games").GetInt("odd_best", 0);
        const int w = 72, h = 56, gap = 4, left = 10, top = kTopBar + 4;
        for (int i = 0; i < Game::kCells; ++i) {
            lv_obj_t* tile = lv_button_create(root_);
            lv_obj_set_size(tile, w, h);
            lv_obj_set_pos(tile, left + (i % Game::kCols) * (w + gap), top + (i / Game::kCols) * (h + gap));
            lv_obj_set_style_radius(tile, 10, 0);
            lv_obj_set_style_shadow_width(tile, 0, 0);
            lv_obj_set_style_border_color(tile, lv_color_white(), 0);
            BindClick(tile, [this, i]() { OnTile(i); });
            tiles_[i] = tile;
        }
        banner_ = MakeBanner([this]() { NewGame(); });
        NewGame();
    }

private:
    using Game = games::OddOneGame;

    Game game_;
    lv_obj_t* tiles_[Game::kCells];
    lv_obj_t* banner_;
    int best_ = 0;

    void NewGame() {
        CancelDelay();
        lv_obj_add_flag(banner_, LV_OBJ_FLAG_HIDDEN);
        game_.Reset(HardwareRandom);
        ShowRound();
    }

    void ShowRound() {
        for (int i = 0; i < Game::kCells; ++i) {
            const uint32_t color = i == game_.Odd() ? game_.OddColor() : game_.BaseColor();
            lv_obj_set_style_bg_color(tiles_[i], lv_color_hex(color), 0);
            lv_obj_set_style_border_width(tiles_[i], 0, 0);
        }
        ShowScore();
    }

    void ShowScore() {
        char text[24];
        snprintf(text, sizeof(text), "%d/%d", game_.Score(), best_);
        lv_label_set_text(info_, text);
    }

    void OnTile(int cell) {
        if (game_.IsOver()) return;
        if (game_.Tap(cell, HardwareRandom)) {
            ShowRound();
            PlaySound(Lang::Sounds::OGG_POPUP);
            return;
        }
        // Show where it was before the banner covers the board.
        lv_obj_set_style_border_width(tiles_[game_.Odd()], 4, 0);
        const int score = game_.Score();
        const bool record = score > best_;
        if (record) {
            best_ = score;
            Settings("games", true).SetInt("odd_best", best_);
        }
        ShowScore();
        PlaySound(record ? Lang::Sounds::OGG_SUCCESS : Lang::Sounds::OGG_VIBRATION);
        After(1200, [this, score, record]() {
            char text[72];
            snprintf(text, sizeof(text), "You found %d%s\n\nTap to play again", score,
                     record ? " - new best!" : "");
            ShowBanner(banner_, text);
        });
    }
};

}  // namespace

GamesApp::GamesApp(std::function<void()> on_exit) : on_exit_(std::move(on_exit)) {
    screen_ = MakeScreen();
    BuildMenu();
}

GamesApp::~GamesApp() = default;

void GamesApp::BuildMenu() {
    menu_ = lv_obj_create(screen_);
    PlainBox(menu_);
    lv_obj_set_size(menu_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(menu_, LV_OPA_TRANSP, 0);

    lv_obj_t* bar = MakeTopBar(menu_, MATERIAL_SYMBOLS_HOME, [this]() { on_exit_(); });
    lv_obj_align(MakeTextLabel(bar, "Games"), LV_ALIGN_LEFT_MID, kTopBarTitleLeft, 0);

    struct Entry {
        const char* icon;
        const char* name;
        uint32_t color;
        std::function<void()> open;
    };
    const Entry entries[] = {
        {MATERIAL_SYMBOLS_STAR, "Tic-Tac-Toe", 0x1565C0, [this]() { Open<TicTacToeView>(); }},
        {MATERIAL_SYMBOLS_FAVORITE, "Memory", 0xAD1457, [this]() { Open<MemoryView>(); }},
        {MATERIAL_SYMBOLS_NAVIGATION, "Snake", 0x2E7D32, [this]() { Open<SnakeView>(); }},
        {MATERIAL_SYMBOLS_REPEAT, "Copy Me", 0xEF6C00, [this]() { Open<CopyView>(); }},
        {MATERIAL_SYMBOLS_ROBOT_2, "Bop the Bot", 0x00838F, [this]() { Open<BopView>(); }},
        {MATERIAL_SYMBOLS_CALCULATE, "Slide Puzzle", 0x6A1B9A, [this]() { Open<SlideView>(); }},
        {MATERIAL_SYMBOLS_KEYBOARD_DOUBLE_ARROW_DOWN, "Four in a Row", 0x283593, [this]() { Open<FourView>(); }},
        {MATERIAL_SYMBOLS_EDIT_SQUARE, "Quick Sums", 0xD84315, [this]() { Open<SumsView>(); }},
        {MATERIAL_SYMBOLS_STAR, "Catch Stars", 0x558B2F, [this]() { Open<CatchView>(); }},
        {MATERIAL_SYMBOLS_KEYBOARD_ARROW_UP, "Robot Hop", 0x0277BD, [this]() { Open<HopView>(); }},
        {MATERIAL_SYMBOLS_SEARCH, "Odd One Out", 0x6D4C41, [this]() { Open<OddOneView>(); }},
    };
    // Two columns of tiles in a list that scrolls under the top bar.
    lv_obj_t* list = lv_obj_create(menu_);
    PlainBox(list);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_bottom(list, 8, 0);
    lv_obj_set_pos(list, 0, kTopBar);
    lv_obj_set_size(list, lv_pct(100), LV_VER_RES - kTopBar);

    const int w = 148, h = 56, gap = 8, top = 4;
    int index = 0;
    for (const auto& entry : entries) {
        lv_obj_t* tile = lv_button_create(list);
        lv_obj_set_size(tile, w, h);
        lv_obj_set_pos(tile, gap + (index % 2) * (w + gap), top + (index / 2) * (h + 4));
        lv_obj_set_style_radius(tile, 14, 0);
        lv_obj_set_style_shadow_width(tile, 0, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(tile, lv_color_hex(entry.color), 0);
        lv_obj_align(MakeIconLabel(tile, entry.icon), LV_ALIGN_LEFT_MID, 3, 0);
        // Long names wrap onto a second line.
        lv_obj_t* name = MakeTextLabel(tile, entry.name);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(name, w - 38);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 35, 0);
        BindClick(tile, entry.open);
        ++index;
    }
}

template <typename Game>
void GamesApp::Open() {
    lv_obj_add_flag(menu_, LV_OBJ_FLAG_HIDDEN);
    game_ = std::make_unique<Game>(screen_, [this]() {
        // This runs inside the back button's own click event, and closing the
        // game deletes that button, so finish the event first.
        lv_async_call([](void* self) { static_cast<GamesApp*>(self)->CloseGame(); }, this);
    });
}

void GamesApp::CloseGame() {
    game_.reset();
    lv_obj_remove_flag(menu_, LV_OBJ_FLAG_HIDDEN);
}

void GamesApp::OnHide() { CloseGame(); }

}  // namespace apps
