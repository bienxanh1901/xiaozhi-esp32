#include "game_logic.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace games {

namespace {
constexpr int kLines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                              {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
}  // namespace

void TicTacToe::Reset() { cells_.fill(Mark::kEmpty); }

bool TicTacToe::Play(int cell, Mark mark) {
    if (cell < 0 || cell >= kCells || cells_[cell] != Mark::kEmpty || IsOver()) {
        return false;
    }
    cells_[cell] = mark;
    return true;
}

TicTacToe::Mark TicTacToe::Winner() const {
    for (const auto& line : kLines) {
        Mark mark = cells_[line[0]];
        if (mark != Mark::kEmpty && mark == cells_[line[1]] && mark == cells_[line[2]]) {
            return mark;
        }
    }
    return Mark::kEmpty;
}

bool TicTacToe::IsOver() const {
    return Winner() != Mark::kEmpty ||
           std::none_of(cells_.begin(), cells_.end(), [](Mark m) { return m == Mark::kEmpty; });
}

std::array<int, 3> TicTacToe::WinningLine() const {
    for (const auto& line : kLines) {
        Mark mark = cells_[line[0]];
        if (mark != Mark::kEmpty && mark == cells_[line[1]] && mark == cells_[line[2]]) {
            return {line[0], line[1], line[2]};
        }
    }
    return {-1, -1, -1};
}

int TicTacToe::CompletingMove(Mark mark) const {
    for (const auto& line : kLines) {
        int count = 0;
        int empty = -1;
        for (int cell : line) {
            if (cells_[cell] == mark) {
                ++count;
            } else if (cells_[cell] == Mark::kEmpty) {
                empty = cell;
            }
        }
        if (count == 2 && empty >= 0) {
            return empty;
        }
    }
    return -1;
}

int TicTacToe::ChooseComputerMove(const Random& random, int blunder_percent) const {
    int move = CompletingMove(Mark::kComputer);
    if (move >= 0) {
        return move;
    }
    if (static_cast<int>(random() % 100) >= blunder_percent) {
        move = CompletingMove(Mark::kPlayer);
        if (move >= 0) {
            return move;
        }
    }
    if (cells_[4] == Mark::kEmpty) {
        return 4;
    }
    int empty[kCells];
    int count = 0;
    for (int i = 0; i < kCells; ++i) {
        if (cells_[i] == Mark::kEmpty) {
            empty[count++] = i;
        }
    }
    return count == 0 ? -1 : empty[random() % count];
}

void MemoryGame::Deal(const Random& random) {
    for (int i = 0; i < kCards; ++i) {
        faces_[i] = i / 2;
    }
    for (int i = kCards - 1; i > 0; --i) {
        std::swap(faces_[i], faces_[random() % (i + 1)]);
    }
    face_up_.fill(false);
    first_ = second_ = -1;
    moves_ = matched_ = 0;
}

MemoryGame::Flip MemoryGame::FlipCard(int card) {
    if (card < 0 || card >= kCards || face_up_[card] || second_ >= 0) {
        return Flip::kIgnored;
    }
    face_up_[card] = true;
    if (first_ < 0) {
        first_ = card;
        return Flip::kFirst;
    }
    ++moves_;
    if (faces_[first_] == faces_[card]) {
        first_ = -1;
        ++matched_;
        return Flip::kMatch;
    }
    second_ = card;
    return Flip::kMismatch;
}

std::array<int, 2> MemoryGame::HideMismatch() {
    std::array<int, 2> hidden{first_, second_};
    if (second_ >= 0) {
        face_up_[first_] = false;
        face_up_[second_] = false;
        first_ = second_ = -1;
    }
    return hidden;
}

void SnakeGame::Reset(const Random& random) {
    body_.clear();
    const Cell head{cols_ / 2, rows_ / 2};
    for (int i = 0; i < 3; ++i) {
        body_.push_back({head.x - i, head.y});
    }
    heading_ = next_ = Direction::kRight;
    score_ = 0;
    PlaceFood(random);
}

bool SnakeGame::OnBody(const Cell& cell) const {
    return std::find(body_.begin(), body_.end(), cell) != body_.end();
}

bool SnakeGame::PlaceFood(const Random& random) {
    const int free_cells = cols_ * rows_ - static_cast<int>(body_.size());
    if (free_cells <= 0) {
        return false;
    }
    // Pick the nth free cell, so this stays quick when the board is nearly full.
    int skip = random() % free_cells;
    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < cols_; ++x) {
            if (!OnBody({x, y}) && skip-- == 0) {
                food_ = {x, y};
                return true;
            }
        }
    }
    return false;
}

SnakeGame::Step SnakeGame::Advance(const Random& random) {
    heading_ = next_;
    Cell head = body_.front();
    switch (heading_) {
        case Direction::kUp: --head.y; break;
        case Direction::kDown: ++head.y; break;
        case Direction::kLeft: --head.x; break;
        case Direction::kRight: ++head.x; break;
    }
    if (head.x < 0 || head.y < 0 || head.x >= cols_ || head.y >= rows_) {
        return Step::kDied;
    }
    const bool eating = head == food_;
    // The tail moves out of the way this step unless the snake is growing.
    if (!eating) {
        body_.pop_back();
    }
    if (OnBody(head)) {
        return Step::kDied;
    }
    body_.push_front(head);
    if (!eating) {
        return Step::kMoved;
    }
    ++score_;
    return PlaceFood(random) ? Step::kAte : Step::kWon;
}

void SnakeGame::TurnToward(int dx, int dy) {
    // Judged against the last step taken, so two quick taps cannot reverse it.
    if (heading_ == Direction::kLeft || heading_ == Direction::kRight) {
        if (dy != 0) {
            next_ = dy < 0 ? Direction::kUp : Direction::kDown;
        }
    } else if (dx != 0) {
        next_ = dx < 0 ? Direction::kLeft : Direction::kRight;
    }
}

void CopyGame::Reset(const Random& random) {
    length_ = 0;
    position_ = 0;
    rounds_done_ = 0;
    playing_ = true;
    steps_[length_++] = random() % kPads;
}

CopyGame::Result CopyGame::Press(int pad) {
    if (!playing_ || position_ >= length_) {
        return Result::kIgnored;
    }
    if (pad != steps_[position_]) {
        playing_ = false;
        return Result::kWrong;
    }
    if (++position_ < length_) {
        return Result::kRight;
    }
    ++rounds_done_;
    if (length_ == kMaxLength) {
        playing_ = false;
        return Result::kWon;
    }
    return Result::kRoundDone;
}

void CopyGame::NextRound(const Random& random) {
    if (!playing_ || position_ != length_) {
        return;
    }
    steps_[length_++] = random() % kPads;
    position_ = 0;
}

void BopGame::Reset() {
    up_ms_.fill(0);
    time_left_ms_ = kRoundMs;
    next_spawn_ms_ = 0;
    score_ = 0;
}

void BopGame::Spawn(const Random& random) {
    std::array<int, kCells> free_cells;
    int count = 0;
    for (int i = 0; i < kCells; ++i) {
        if (up_ms_[i] == 0) free_cells[count++] = i;
    }
    if (count == 0) {
        return;
    }
    // Robots come faster and stay up for less time as the round goes on.
    const int elapsed = kRoundMs - time_left_ms_;
    up_ms_[free_cells[random() % count]] = 1500 - 700 * elapsed / kRoundMs;
    next_spawn_ms_ = 900 - 450 * elapsed / kRoundMs;
}

void BopGame::Advance(int ms, const Random& random) {
    if (IsOver()) {
        return;
    }
    time_left_ms_ = std::max(0, time_left_ms_ - ms);
    if (IsOver()) {
        up_ms_.fill(0);
        return;
    }
    for (int& up : up_ms_) {
        up = std::max(0, up - ms);
    }
    next_spawn_ms_ -= ms;
    if (next_spawn_ms_ <= 0) {
        Spawn(random);
    }
}

bool BopGame::Bop(int cell) {
    if (cell < 0 || cell >= kCells || up_ms_[cell] == 0) {
        return false;
    }
    up_ms_[cell] = 0;
    ++score_;
    return true;
}

namespace {
bool NextToEachOther(int a, int b) {
    return std::abs(a / 3 - b / 3) + std::abs(a % 3 - b % 3) == 1;
}
}  // namespace

void SlidePuzzle::Shuffle(const Random& random) {
    // Only legal moves from the solved board, so it can always be solved.
    do {
        for (int i = 0; i < kCells; ++i) {
            tiles_[i] = (i + 1) % kCells;
        }
        gap_ = kCells - 1;
        int previous = -1;
        for (int step = 0; step < 80; ++step) {
            std::array<int, 4> options;
            int count = 0;
            for (int cell = 0; cell < kCells; ++cell) {
                if (NextToEachOther(cell, gap_) && cell != previous) options[count++] = cell;
            }
            const int cell = options[random() % count];
            std::swap(tiles_[cell], tiles_[gap_]);
            previous = gap_;
            gap_ = cell;
        }
    } while (IsSolved());
    moves_ = 0;
}

bool SlidePuzzle::Slide(int cell) {
    if (cell < 0 || cell >= kCells || IsSolved() || !NextToEachOther(cell, gap_)) {
        return false;
    }
    std::swap(tiles_[cell], tiles_[gap_]);
    gap_ = cell;
    ++moves_;
    return true;
}

bool SlidePuzzle::IsSolved() const {
    for (int i = 0; i < kCells; ++i) {
        if (tiles_[i] != (i + 1) % kCells) return false;
    }
    return true;
}

// ── FourInARow ─────────────────────────────────────────────────────────────

void FourInARow::Reset() {
    cells_.fill(Mark::kEmpty);
    filled_ = 0;
}

int FourInARow::RowFor(int col) const {
    if (col < 0 || col >= kCols) return -1;
    for (int row = 0; row < kRows; ++row) {
        if (At(col, row) == Mark::kEmpty) return row;
    }
    return -1;
}

int FourInARow::Drop(int col, Mark mark) {
    if (IsOver() || mark == Mark::kEmpty) return -1;
    const int row = RowFor(col);
    if (row < 0) return -1;
    cells_[col * kRows + row] = mark;
    ++filled_;
    return row;
}

bool FourInARow::FindLine(std::array<Cell, kLine>* line) const {
    static constexpr int kDirections[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int col = 0; col < kCols; ++col) {
        for (int row = 0; row < kRows; ++row) {
            const Mark mark = At(col, row);
            if (mark == Mark::kEmpty) continue;
            for (const auto& direction : kDirections) {
                const int end_col = col + direction[0] * (kLine - 1);
                const int end_row = row + direction[1] * (kLine - 1);
                if (end_col < 0 || end_col >= kCols || end_row < 0 || end_row >= kRows) continue;
                bool same = true;
                for (int i = 1; i < kLine && same; ++i) {
                    same = At(col + direction[0] * i, row + direction[1] * i) == mark;
                }
                if (!same) continue;
                if (line != nullptr) {
                    for (int i = 0; i < kLine; ++i) {
                        (*line)[i] = {col + direction[0] * i, row + direction[1] * i};
                    }
                }
                return true;
            }
        }
    }
    return false;
}

FourInARow::Mark FourInARow::Winner() const {
    std::array<Cell, kLine> line{};
    return FindLine(&line) ? At(line[0].col, line[0].row) : Mark::kEmpty;
}

bool FourInARow::IsOver() const { return filled_ == kCols * kRows || FindLine(nullptr); }

std::array<FourInARow::Cell, FourInARow::kLine> FourInARow::WinningLine() const {
    std::array<Cell, kLine> line{};
    FindLine(&line);
    return line;
}

int FourInARow::CompletingMove(Mark mark) const {
    for (int col = 0; col < kCols; ++col) {
        FourInARow copy = *this;
        if (copy.Drop(col, mark) >= 0 && copy.Winner() == mark) return col;
    }
    return -1;
}

int FourInARow::ChooseComputerMove(const Random& random, int blunder_percent) const {
    const int win = CompletingMove(Mark::kComputer);
    if (win >= 0) return win;
    if (static_cast<int>(random() % 100) >= blunder_percent) {
        const int block = CompletingMove(Mark::kPlayer);
        if (block >= 0) return block;
    }
    // Rate the open columns: the middle is worth most, and a move that lets the
    // player win straight away on top of it is worth least.
    int best = -1;
    int best_score = 0;
    for (int col = 0; col < kCols; ++col) {
        if (RowFor(col) < 0) continue;
        FourInARow copy = *this;
        copy.Drop(col, Mark::kComputer);
        int score = 100 - 10 * std::abs(col - kCols / 2) + static_cast<int>(random() % 15);
        if (copy.CompletingMove(Mark::kPlayer) >= 0) score -= 1000;
        if (best < 0 || score > best_score) {
            best = col;
            best_score = score;
        }
    }
    return best;
}

// ── SumsGame ───────────────────────────────────────────────────────────────

void SumsGame::Start(const Random& random) {
    question_ = 0;
    score_ = 0;
    Ask(random);
}

void SumsGame::Ask(const Random& random) {
    // Totals up to 10 for the first half of the round, then up to 20.
    const int limit = question_ < kQuestions / 2 ? 10 : 20;
    const int previous_a = a_;
    const int previous_b = b_;
    const bool previous_plus = plus_;
    do {
        plus_ = random() % 2 == 0;
        const int top = 2 + static_cast<int>(random() % (limit - 1));  // 2 to limit
        const int part = 1 + static_cast<int>(random() % (top - 1));   // 1 to top - 1
        a_ = plus_ ? part : top;
        b_ = plus_ ? top - part : part;
    } while (a_ == previous_a && b_ == previous_b && plus_ == previous_plus);
    answered_ = false;
    missed_ = false;

    const int total = Total();
    const int answer_at = static_cast<int>(random() % kChoices);
    for (int i = 0; i < kChoices; ++i) {
        tried_[i] = false;
        if (i == answer_at) {
            choices_[i] = total;
            continue;
        }
        // A near miss, never negative, not the answer or another button.
        int pick;
        bool taken;
        do {
            const int offset = 1 + static_cast<int>(random() % 3);
            pick = random() % 2 == 0 ? total + offset : total - offset;
            taken = pick < 0 || pick == total;
            for (int j = 0; j < i; ++j) taken = taken || choices_[j] == pick;
        } while (taken);
        choices_[i] = pick;
    }
}

SumsGame::Result SumsGame::Answer(int choice) {
    if (IsOver() || answered_ || choice < 0 || choice >= kChoices || tried_[choice]) {
        return Result::kIgnored;
    }
    tried_[choice] = true;
    if (choices_[choice] != Total()) {
        missed_ = true;
        return Result::kWrong;
    }
    if (!missed_) ++score_;
    answered_ = true;
    return Result::kRight;
}

bool SumsGame::Next(const Random& random) {
    if (IsOver() || !answered_) return false;
    ++question_;
    if (IsOver()) return false;
    Ask(random);
    return true;
}

// ── CatchGame ──────────────────────────────────────────────────────────────

void CatchGame::Reset() {
    items_ = {};
    basket_left_ = (kWidth - kBasketWidth) / 2;
    score_ = 0;
    lives_ = kStartLives;
    next_spawn_ms_ = 500;
}

void CatchGame::MoveBasket(int center_x) {
    basket_left_ = std::clamp(center_x - kBasketWidth / 2, 0, kWidth - kBasketWidth);
}

void CatchGame::Spawn(const Random& random) {
    for (auto& item : items_) {
        if (item.active) continue;
        item.active = true;
        item.bad = random() % 100 < 25;
        item.x = static_cast<int>(random() % (kWidth - kItemSize));
        item.y_milli = 0;
        return;
    }
}

CatchGame::Events CatchGame::Advance(int ms, const Random& random) {
    Events events;
    if (IsOver()) return events;
    const int speed = std::min(70 + score_ * 4, 240);  // Pixels per second
    for (auto& item : items_) {
        if (!item.active) continue;
        const int previous_bottom = item.y_milli / 1000 + kItemSize;
        item.y_milli += speed * ms;
        const int top = item.y_milli / 1000;
        const int bottom = top + kItemSize;
        // Only as it reaches the rim: an item that fell past the side stays lost.
        const bool at_rim = previous_bottom <= kBasketTop && bottom >= kBasketTop;
        const bool over_basket = item.x + kItemSize > basket_left_ + 4 &&
                                 item.x < basket_left_ + kBasketWidth - 4;
        if (at_rim && over_basket) {
            item.active = false;
            if (item.bad) {
                --lives_;
                ++events.lost;
            } else {
                ++score_;
                ++events.caught;
            }
        } else if (top >= kHeight) {
            item.active = false;
            if (!item.bad) {
                --lives_;
                ++events.lost;
            }
        }
    }
    next_spawn_ms_ -= ms;
    if (next_spawn_ms_ <= 0) {
        Spawn(random);
        next_spawn_ms_ = std::max(1100 - score_ * 15, 500);
    }
    return events;
}

// ── HopGame ────────────────────────────────────────────────────────────────

namespace {
constexpr int kJumpSpeed = 520;   // Pixels per second
constexpr int kGravity = 1500;    // Pixels per second per second
constexpr int kJumpBufferMs = 120;
constexpr int kHitMargin = 4;     // The robot's corners do not count
}  // namespace

int HopGame::Speed() const { return std::min(130 + score_ * 5, 300); }

void HopGame::Reset(const Random& random) {
    blocks_ = {};
    height_milli_ = 0;
    velocity_ = 0;
    jump_buffer_ms_ = 0;
    score_ = 0;
    crashed_ = false;
    next_block_milli_ = 0;
    Spawn(random);
}

void HopGame::Jump() {
    if (!crashed_) jump_buffer_ms_ = kJumpBufferMs;
}

void HopGame::Spawn(const Random& random) {
    for (auto& block : blocks_) {
        if (block.active) continue;
        block.active = true;
        block.passed = false;
        block.x_milli = kWidth * 1000;
        block.height = 20 + static_cast<int>(random() % 25);  // 20 to 44, under the jump
        break;
    }
    // Far enough apart to land between blocks: a jump lasts about 0.7 s.
    const int gap = Speed() * 3 / 4 + kBlockWidth + 40 + static_cast<int>(random() % 160);
    next_block_milli_ = gap * 1000;
}

bool HopGame::Advance(int ms, const Random& random) {
    if (crashed_) return false;

    if (jump_buffer_ms_ > 0) {
        if (height_milli_ == 0) {
            velocity_ = kJumpSpeed;
            jump_buffer_ms_ = 0;
        } else {
            jump_buffer_ms_ = std::max(jump_buffer_ms_ - ms, 0);
        }
    }
    if (height_milli_ > 0 || velocity_ > 0) {
        height_milli_ += velocity_ * ms;
        velocity_ -= kGravity * ms / 1000;
        if (height_milli_ <= 0) {
            height_milli_ = 0;
            velocity_ = 0;
        }
    }

    const int moved = Speed() * ms;
    const int robot_left = kRobotX + kHitMargin;
    const int robot_right = kRobotX + kRobotSize - kHitMargin;
    for (auto& block : blocks_) {
        if (!block.active) continue;
        block.x_milli -= moved;
        const int left = block.x_milli / 1000;
        if (left < robot_right && left + kBlockWidth > robot_left &&
            RobotHeight() + kHitMargin < block.height) {
            crashed_ = true;
            return false;
        }
        if (!block.passed && left + kBlockWidth <= robot_left) {
            block.passed = true;
            ++score_;
        }
        if (left + kBlockWidth < 0) block.active = false;
    }

    next_block_milli_ -= moved;
    if (next_block_milli_ <= 0) Spawn(random);
    return true;
}

// ── OddOneGame ─────────────────────────────────────────────────────────────

void OddOneGame::Reset(const Random& random) {
    score_ = 0;
    over_ = false;
    NewRound(random);
}

void OddOneGame::NewRound(const Random& random) {
    odd_ = static_cast<int>(random() % kCells);
    // Channels stay low enough that the odd tile can always be lighter.
    const int r = 40 + static_cast<int>(random() % 136);
    const int g = 40 + static_cast<int>(random() % 136);
    const int b = 40 + static_cast<int>(random() % 136);
    const int delta = std::max(70 - score_ * 5, 20);
    base_ = static_cast<uint32_t>(r << 16 | g << 8 | b);
    odd_color_ = static_cast<uint32_t>((r + delta) << 16 | (g + delta) << 8 | (b + delta));
}

bool OddOneGame::Tap(int cell, const Random& random) {
    if (over_ || cell < 0 || cell >= kCells) return false;
    if (cell != odd_) {
        over_ = true;
        return false;
    }
    ++score_;
    NewRound(random);
    return true;
}

// ── Lights Out ────────────────────────────────────────────────────────────

void LightsOut::Flip(int cell) {
    const int row = cell / kSize, col = cell % kSize;
    lights_[cell] = !lights_[cell];
    if (row > 0)
        lights_[cell - kSize] = !lights_[cell - kSize];
    if (row < kSize - 1)
        lights_[cell + kSize] = !lights_[cell + kSize];
    if (col > 0)
        lights_[cell - 1] = !lights_[cell - 1];
    if (col < kSize - 1)
        lights_[cell + 1] = !lights_[cell + 1];
}

void LightsOut::NewPuzzle(int level, const Random& random) {
    level_ = std::max(level, 1);
    moves_ = 0;
    const int taps = std::min(2 + level_, 12);
    // Some sets of taps cancel out; try again until lights are on.
    do {
        lights_.fill(false);
        std::array<bool, kCells> tapped{};
        for (int done = 0; done < taps;) {
            const int cell = static_cast<int>(random() % kCells);
            if (tapped[cell])
                continue;
            tapped[cell] = true;
            Flip(cell);
            ++done;
        }
    } while (IsSolved());
}

bool LightsOut::Press(int cell) {
    if (cell < 0 || cell >= kCells || IsSolved())
        return false;
    Flip(cell);
    ++moves_;
    return true;
}

bool LightsOut::IsSolved() const {
    return std::none_of(lights_.begin(), lights_.end(), [](bool on) { return on; });
}

// ── Number Merge ──────────────────────────────────────────────────────────

void MergeGame::Reset(const Random& random) {
    tiles_.fill(0);
    score_ = 0;
    AddTile(random);
    AddTile(random);
}

void MergeGame::AddTile(const Random& random) {
    int empty = 0;
    for (int value : tiles_)
        empty += value == 0;
    if (empty == 0)
        return;
    int pick = static_cast<int>(random() % empty);
    const int value = random() % 10 == 0 ? 4 : 2;
    for (int& tile : tiles_) {
        if (tile == 0 && pick-- == 0) {
            tile = value;
            return;
        }
    }
}

bool MergeGame::Move(Direction direction, const Random& random) {
    bool moved = false;
    for (int line = 0; line < kSize; ++line) {
        // The cells of this row or column, starting from the side tiles move to.
        int cells[kSize];
        for (int i = 0; i < kSize; ++i) {
            switch (direction) {
                case Direction::kLeft:
                    cells[i] = line * kSize + i;
                    break;
                case Direction::kRight:
                    cells[i] = line * kSize + (kSize - 1 - i);
                    break;
                case Direction::kUp:
                    cells[i] = i * kSize + line;
                    break;
                case Direction::kDown:
                    cells[i] = (kSize - 1 - i) * kSize + line;
                    break;
            }
        }
        int values[kSize] = {};
        int count = 0;
        bool can_merge = false;  // A tile merges once per move
        for (int i = 0; i < kSize; ++i) {
            const int value = tiles_[cells[i]];
            if (value == 0)
                continue;
            if (can_merge && values[count - 1] == value) {
                values[count - 1] *= 2;
                score_ += values[count - 1];
                can_merge = false;
            } else {
                values[count++] = value;
                can_merge = true;
            }
        }
        for (int i = 0; i < kSize; ++i) {
            if (tiles_[cells[i]] != values[i])
                moved = true;
            tiles_[cells[i]] = values[i];
        }
    }
    if (moved)
        AddTile(random);
    return moved;
}

int MergeGame::Largest() const { return *std::max_element(tiles_.begin(), tiles_.end()); }

bool MergeGame::CanMove() const {
    for (int cell = 0; cell < kCells; ++cell) {
        if (tiles_[cell] == 0)
            return true;
        const int col = cell % kSize;
        if (col < kSize - 1 && tiles_[cell] == tiles_[cell + 1])
            return true;
        if (cell + kSize < kCells && tiles_[cell] == tiles_[cell + kSize])
            return true;
    }
    return false;
}

// ── How Many? ─────────────────────────────────────────────────────────────

void CountGame::Start(const Random& random) {
    question_ = 0;
    score_ = 0;
    Ask(random);
}

void CountGame::Ask(const Random& random) {
    const int max = question_ < kQuestions / 2 ? 5 : kMaxCount;
    int count = 1 + static_cast<int>(random() % max);
    if (count == count_)
        count = count % max + 1;  // Not the same twice in a row
    count_ = count;
    kind_ = static_cast<int>(random() % kKinds);

    // Distinct slots: shuffle them all and take the first few.
    std::array<int, kSlots> all{};
    for (int i = 0; i < kSlots; ++i)
        all[i] = i;
    for (int i = kSlots - 1; i > 0; --i)
        std::swap(all[i], all[random() % (i + 1)]);
    std::copy(all.begin(), all.begin() + count_, slots_.begin());
    std::sort(slots_.begin(), slots_.begin() + count_);

    // The answer and two near misses, in a random order.
    std::array<int, kChoices> choices{count_, 0, 0};
    for (int i = 1; i < kChoices; ++i) {
        int candidate;
        do {
            const int offset = 1 + static_cast<int>(random() % 2);
            candidate = random() % 2 ? count_ + offset : count_ - offset;
        } while (candidate < 1 || candidate > kMaxCount + 2 ||
                 std::find(choices.begin(), choices.begin() + i, candidate) != choices.begin() + i);
        choices[i] = candidate;
    }
    for (int i = kChoices - 1; i > 0; --i)
        std::swap(choices[i], choices[random() % (i + 1)]);
    choices_ = choices;
    answered_ = false;
    missed_ = false;
}

CountGame::Result CountGame::Answer(int choice) {
    if (choice < 0 || choice >= kChoices || answered_ || IsOver())
        return Result::kIgnored;
    if (choices_[choice] != count_) {
        missed_ = true;
        return Result::kWrong;
    }
    answered_ = true;
    if (!missed_)
        ++score_;
    return Result::kRight;
}

bool CountGame::Next(const Random& random) {
    if (!answered_ || IsOver())
        return false;
    ++question_;
    if (IsOver())
        return false;
    Ask(random);
    return true;
}

// ── Balloon Pop ───────────────────────────────────────────────────────────

void BalloonGame::Reset() {
    balloons_ = {};
    score_ = 0;
    lives_ = kStartLives;
    next_spawn_ms_ = 300;
}

int BalloonGame::Speed() const { return std::min(45 + score_ * 3, 160); }

void BalloonGame::Spawn(const Random& random) {
    for (auto& balloon : balloons_) {
        if (balloon.active)
            continue;
        balloon.active = true;
        balloon.x = static_cast<int>(random() % (kWidth - kSize));
        balloon.y_milli = kHeight * 1000;
        balloon.color = static_cast<int>(random() % kColors);
        return;
    }
}

int BalloonGame::Advance(int ms, const Random& random) {
    if (IsOver())
        return 0;
    int escaped = 0;
    const int step = Speed() * ms;  // Thousandths of a pixel
    for (auto& balloon : balloons_) {
        if (!balloon.active)
            continue;
        balloon.y_milli -= step;
        if (balloon.y_milli < -kSize * 1000) {
            balloon.active = false;
            ++escaped;
        }
    }
    lives_ = std::max(lives_ - escaped, 0);
    next_spawn_ms_ -= ms;
    if (next_spawn_ms_ <= 0 && !IsOver()) {
        Spawn(random);
        const int interval = std::max(1200 - score_ * 25, 450);
        next_spawn_ms_ = interval + static_cast<int>(random() % 300);
    }
    return escaped;
}

int BalloonGame::Tap(int x, int y) {
    if (IsOver())
        return -1;
    constexpr int kSlack = 6;  // Small fingers miss by a little
    for (int i = 0; i < kMaxBalloons; ++i) {
        auto& balloon = balloons_[i];
        if (!balloon.active)
            continue;
        const int top = balloon.y_milli / 1000;
        if (x >= balloon.x - kSlack && x < balloon.x + kSize + kSlack && y >= top - kSlack &&
            y < top + kSize + kSlack) {
            balloon.active = false;
            ++score_;
            return i;
        }
    }
    return -1;
}

}  // namespace games
