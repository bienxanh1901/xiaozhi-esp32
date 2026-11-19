#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>

// Rules for the built-in games, with no display or platform dependencies so
// they can be tested on the host (scripts/tests/test_game_logic.py).
namespace games {

using Random = std::function<uint32_t()>;

class TicTacToe {
public:
    enum class Mark : uint8_t { kEmpty, kPlayer, kComputer };
    static constexpr int kCells = 9;

    void Reset();
    // False if the game is over or the cell is taken.
    bool Play(int cell, Mark mark);
    Mark At(int cell) const { return cells_[cell]; }
    Mark Winner() const;
    bool IsOver() const;
    // The three winning cells. Only meaningful when there is a winner.
    std::array<int, 3> WinningLine() const;

    // Takes a win, blocks a loss, then prefers the centre. With probability
    // `blunder_percent` it misses the block, so that young players can win.
    int ChooseComputerMove(const Random& random, int blunder_percent) const;

private:
    std::array<Mark, kCells> cells_{};
    int CompletingMove(Mark mark) const;
};

class MemoryGame {
public:
    static constexpr int kPairs = 6;
    static constexpr int kCards = kPairs * 2;
    enum class Flip { kIgnored, kFirst, kMatch, kMismatch };

    void Deal(const Random& random);
    // While a mismatch is showing, flips are ignored until HideMismatch().
    Flip FlipCard(int card);
    // Turns the mismatched pair back over and returns which cards they were.
    std::array<int, 2> HideMismatch();

    int Face(int card) const { return faces_[card]; }
    bool IsFaceUp(int card) const { return face_up_[card]; }
    int Moves() const { return moves_; }
    bool IsWon() const { return matched_ == kPairs; }

private:
    std::array<int, kCards> faces_{};
    std::array<bool, kCards> face_up_{};
    int first_ = -1;
    int second_ = -1;
    int moves_ = 0;
    int matched_ = 0;
};

class SnakeGame {
public:
    struct Cell {
        int x;
        int y;
        bool operator==(const Cell& other) const { return x == other.x && y == other.y; }
    };
    enum class Direction { kUp, kDown, kLeft, kRight };
    enum class Step { kMoved, kAte, kDied, kWon };

    SnakeGame(int cols, int rows) : cols_(cols), rows_(rows) {}

    void Reset(const Random& random);
    Step Advance(const Random& random);
    // Turns towards a point relative to the head: a snake moving sideways goes
    // up or down, one moving vertically goes left or right. Turning back on
    // itself is impossible, and a point exactly in line changes nothing.
    void TurnToward(int dx, int dy);

    const std::deque<Cell>& Body() const { return body_; }  // Head first
    Cell Food() const { return food_; }
    int Score() const { return score_; }
    Direction Heading() const { return next_; }

private:
    int cols_;
    int rows_;
    std::deque<Cell> body_;
    Cell food_{0, 0};
    Direction heading_ = Direction::kRight;  // Direction of the last step
    Direction next_ = Direction::kRight;
    int score_ = 0;

    bool PlaceFood(const Random& random);
    bool OnBody(const Cell& cell) const;
};

// Repeat a sequence of pads that grows by one each round.
class CopyGame {
public:
    static constexpr int kPads = 4;
    static constexpr int kMaxLength = 64;
    enum class Result { kIgnored, kRight, kRoundDone, kWrong, kWon };

    void Reset(const Random& random);
    Result Press(int pad);
    // Adds a step once the round is done; otherwise does nothing.
    void NextRound(const Random& random);

    int Length() const { return length_; }
    int Step(int index) const { return steps_[index]; }
    int RoundsDone() const { return rounds_done_; }

private:
    std::array<uint8_t, kMaxLength> steps_{};
    int length_ = 0;
    int position_ = 0;
    int rounds_done_ = 0;
    bool playing_ = false;
};

// Robots pop up on a grid and duck away again; bop as many as you can in time.
class BopGame {
public:
    static constexpr int kCells = 9;
    static constexpr int kRoundMs = 30000;

    void Reset();
    void Advance(int ms, const Random& random);
    // True if a robot was up there. It goes down and scores.
    bool Bop(int cell);

    bool IsUp(int cell) const { return up_ms_[cell] > 0; }
    int Score() const { return score_; }
    int TimeLeftMs() const { return time_left_ms_; }
    bool IsOver() const { return time_left_ms_ <= 0; }

private:
    std::array<int, kCells> up_ms_{};  // Time each robot has left; 0 is down
    int time_left_ms_ = 0;
    int next_spawn_ms_ = 0;
    int score_ = 0;

    void Spawn(const Random& random);
};

// Numbered tiles on a 3x3 board with one gap; put them back in order.
class SlidePuzzle {
public:
    static constexpr int kCells = 9;

    void Shuffle(const Random& random);
    // Moves the tile at `cell` into the gap. False if it isn't next to the gap,
    // or the puzzle is already solved.
    bool Slide(int cell);

    int At(int cell) const { return tiles_[cell]; }  // 1 to 8, or 0 for the gap
    int Moves() const { return moves_; }
    bool IsSolved() const;

private:
    std::array<int, kCells> tiles_{};
    int gap_ = kCells - 1;
    int moves_ = 0;
};

// Drop discs into a 7x6 board; four in a line wins. Row 0 is the bottom.
class FourInARow {
public:
    enum class Mark : uint8_t { kEmpty, kPlayer, kComputer };
    static constexpr int kCols = 7;
    static constexpr int kRows = 6;
    static constexpr int kLine = 4;
    struct Cell {
        int col;
        int row;
    };

    void Reset();
    // The row the disc landed in, or -1 if the column is full, off the board or
    // the game is over.
    int Drop(int col, Mark mark);
    Mark At(int col, int row) const { return cells_[col * kRows + row]; }
    Mark Winner() const;
    bool IsOver() const;
    // The four winning cells. Only meaningful when there is a winner.
    std::array<Cell, kLine> WinningLine() const;

    // Takes a win, blocks a loss, avoids handing over a win, then prefers the
    // middle. With probability `blunder_percent` it misses the block.
    int ChooseComputerMove(const Random& random, int blunder_percent) const;

private:
    std::array<Mark, kCols * kRows> cells_{};
    int filled_ = 0;

    int RowFor(int col) const;  // Where a disc would land, or -1 if full
    bool FindLine(std::array<Cell, kLine>* line) const;
    int CompletingMove(Mark mark) const;
};

// Ten sums with three answers each. Only a right first try scores.
class SumsGame {
public:
    static constexpr int kQuestions = 10;
    static constexpr int kChoices = 3;
    enum class Result { kIgnored, kWrong, kRight };

    void Start(const Random& random);
    Result Answer(int choice);
    // Moves on after a right answer. False when the round is over.
    bool Next(const Random& random);

    int A() const { return a_; }
    int B() const { return b_; }
    bool IsPlus() const { return plus_; }
    int Total() const { return plus_ ? a_ + b_ : a_ - b_; }
    int Choice(int index) const { return choices_[index]; }
    bool Tried(int index) const { return tried_[index]; }
    bool Answered() const { return answered_; }
    int Question() const { return question_; }  // 0-based, kQuestions when done
    int Score() const { return score_; }
    bool IsOver() const { return question_ >= kQuestions; }

private:
    int a_ = 0;
    int b_ = 0;
    bool plus_ = true;
    std::array<int, kChoices> choices_{};
    std::array<bool, kChoices> tried_{};
    bool answered_ = false;
    bool missed_ = false;
    int question_ = 0;
    int score_ = 0;

    void Ask(const Random& random);
};

// Move a basket to catch falling stars and dodge the red marks. A red mark
// caught or a star dropped costs a life. It gets faster as the score grows.
class CatchGame {
public:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 180;
    static constexpr int kBasketWidth = 72;
    static constexpr int kBasketTop = kHeight - 22;
    static constexpr int kItemSize = 30;
    static constexpr int kMaxItems = 6;
    static constexpr int kStartLives = 3;

    struct Item {
        bool active = false;
        bool bad = false;
        int x = 0;       // Left edge
        int y_milli = 0;  // Top edge, in thousandths of a pixel
    };
    struct Events {
        int caught = 0;
        int lost = 0;
    };

    void Reset();
    Events Advance(int ms, const Random& random);
    void MoveBasket(int center_x);

    const Item& At(int index) const { return items_[index]; }
    int BasketLeft() const { return basket_left_; }
    int Score() const { return score_; }
    int Lives() const { return lives_; }
    bool IsOver() const { return lives_ <= 0; }

private:
    std::array<Item, kMaxItems> items_{};
    int basket_left_ = 0;
    int next_spawn_ms_ = 0;
    int score_ = 0;
    int lives_ = kStartLives;

    void Spawn(const Random& random);
};

// A robot runs along the ground; tap to jump over the blocks.
class HopGame {
public:
    static constexpr int kWidth = 320;
    static constexpr int kRobotX = 40;     // Left edge
    static constexpr int kRobotSize = 30;
    static constexpr int kBlockWidth = 24;
    static constexpr int kMaxBlocks = 4;

    struct Block {
        bool active = false;
        bool passed = false;
        int x_milli = 0;  // Left edge, in thousandths of a pixel
        int height = 0;
    };

    void Reset(const Random& random);
    // A jump pressed just before landing still counts.
    void Jump();
    // False once the robot has hit a block.
    bool Advance(int ms, const Random& random);

    const Block& At(int index) const { return blocks_[index]; }
    int RobotHeight() const { return height_milli_ / 1000; }  // Above the ground
    int Speed() const;                                        // Pixels per second
    int Score() const { return score_; }
    bool IsOver() const { return crashed_; }

private:
    std::array<Block, kMaxBlocks> blocks_{};
    int height_milli_ = 0;
    int velocity_ = 0;  // Pixels per second, up is positive
    int jump_buffer_ms_ = 0;
    int next_block_milli_ = 0;  // Distance until the next block
    int score_ = 0;
    bool crashed_ = false;

    void Spawn(const Random& random);
};

// One tile of the grid is a slightly different color, harder each round. A wrong
// tap ends the game.
class OddOneGame {
public:
    static constexpr int kCols = 4;
    static constexpr int kRows = 3;
    static constexpr int kCells = kCols * kRows;

    void Reset(const Random& random);
    // True if `cell` was the odd one, which starts the next round.
    bool Tap(int cell, const Random& random);

    int Odd() const { return odd_; }
    uint32_t BaseColor() const { return base_; }
    uint32_t OddColor() const { return odd_color_; }
    int Score() const { return score_; }
    bool IsOver() const { return over_; }

private:
    int odd_ = 0;
    uint32_t base_ = 0;
    uint32_t odd_color_ = 0;
    int score_ = 0;
    bool over_ = false;

    void NewRound(const Random& random);
};

}  // namespace games
