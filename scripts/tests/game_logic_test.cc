// Host test for main/apps/game_logic.cc; built and run by test_game_logic.py.
#include <cstdio>
#include <random>
#include <set>
#include <vector>

#include "game_logic.h"

using namespace games;

static int failures = 0;
#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAILED line %d: %s\n", __LINE__, #cond);       \
            ++failures;                                            \
        }                                                          \
    } while (0)

static void TestTicTacToe() {
    using Mark = TicTacToe::Mark;
    std::mt19937 engine(1);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    Random never_blunder = [] { return 99u; };

    TicTacToe game;
    game.Reset();
    CHECK(!game.IsOver());
    CHECK(game.Play(0, Mark::kPlayer));
    CHECK(!game.Play(0, Mark::kComputer));  // taken
    CHECK(!game.Play(9, Mark::kComputer));  // off the board

    // Takes its own win ahead of blocking.
    game.Reset();
    game.Play(0, Mark::kPlayer);
    game.Play(1, Mark::kPlayer);
    game.Play(3, Mark::kComputer);
    game.Play(4, Mark::kComputer);
    CHECK(game.ChooseComputerMove(never_blunder, 0) == 5);

    // Blocks, unless it blunders.
    game.Reset();
    game.Play(0, Mark::kPlayer);
    game.Play(1, Mark::kPlayer);
    CHECK(game.ChooseComputerMove(never_blunder, 0) == 2);
    CHECK(game.ChooseComputerMove([] { return 0u; }, 100) != 2);

    game.Reset();
    game.Play(2, Mark::kPlayer);
    game.Play(4, Mark::kPlayer);
    game.Play(6, Mark::kPlayer);
    CHECK(game.Winner() == Mark::kPlayer);
    CHECK(game.IsOver());
    CHECK((game.WinningLine() == std::array<int, 3>{2, 4, 6}));
    CHECK(!game.Play(0, Mark::kComputer));  // over

    // Random games always finish, on legal moves.
    for (int round = 0; round < 500; ++round) {
        game.Reset();
        Mark turn = round % 2 ? Mark::kPlayer : Mark::kComputer;
        int plays = 0;
        while (!game.IsOver()) {
            int move = turn == Mark::kComputer ? game.ChooseComputerMove(random, 25) : random() % 9;
            if (game.Play(move, turn)) {
                turn = turn == Mark::kPlayer ? Mark::kComputer : Mark::kPlayer;
                ++plays;
            }
        }
        CHECK(plays <= 9);
    }
}

static void TestMemory() {
    std::mt19937 engine(2);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    MemoryGame game;
    game.Deal(random);

    int counts[MemoryGame::kPairs] = {};
    for (int i = 0; i < MemoryGame::kCards; ++i) ++counts[game.Face(i)];
    for (int count : counts) CHECK(count == 2);

    // Play perfectly from known faces.
    int partner0 = -1;
    for (int i = 1; i < MemoryGame::kCards; ++i) {
        if (game.Face(i) == game.Face(0)) partner0 = i;
    }
    int other = -1;
    for (int i = 1; i < MemoryGame::kCards; ++i) {
        if (game.Face(i) != game.Face(0)) { other = i; break; }
    }
    CHECK(game.FlipCard(0) == MemoryGame::Flip::kFirst);
    CHECK(game.FlipCard(0) == MemoryGame::Flip::kIgnored);
    CHECK(game.FlipCard(other) == MemoryGame::Flip::kMismatch);
    CHECK(game.FlipCard(partner0) == MemoryGame::Flip::kIgnored);  // mismatch still showing
    auto hidden = game.HideMismatch();
    CHECK(hidden[0] == 0 && hidden[1] == other);
    CHECK(!game.IsFaceUp(0) && !game.IsFaceUp(other));
    CHECK(game.Moves() == 1);

    for (int face = 0; face < MemoryGame::kPairs; ++face) {
        int a = -1;
        for (int i = 0; i < MemoryGame::kCards; ++i) {
            if (game.Face(i) != face) continue;
            if (a < 0) {
                a = i;
                CHECK(game.FlipCard(i) == MemoryGame::Flip::kFirst);
            } else {
                CHECK(game.FlipCard(i) == MemoryGame::Flip::kMatch);
            }
        }
    }
    CHECK(game.IsWon());
    CHECK(game.Moves() == 1 + MemoryGame::kPairs);
}

static void TestSnake() {
    using Step = SnakeGame::Step;
    using Dir = SnakeGame::Direction;
    Random first_free = [] { return 0u; };

    SnakeGame game(8, 6);
    game.Reset(first_free);
    CHECK(game.Body().size() == 3);
    CHECK(game.Body().front().x == 4 && game.Body().front().y == 3);
    CHECK(game.Food().x == 0 && game.Food().y == 0);

    CHECK(game.Advance(first_free) == Step::kMoved);
    CHECK(game.Body().size() == 3);
    CHECK(game.Body().front().x == 5);

    // Straight ahead and in-line taps change nothing; it cannot reverse.
    game.TurnToward(-3, 0);
    CHECK(game.Heading() == Dir::kRight);
    game.TurnToward(0, -2);
    CHECK(game.Heading() == Dir::kUp);
    game.TurnToward(0, 2);  // Not yet stepped: still judged against moving right
    CHECK(game.Heading() == Dir::kDown);
    game.TurnToward(0, -2);
    CHECK(game.Advance(first_free) == Step::kMoved);
    game.TurnToward(0, 5);  // Moving up: vertical taps cannot reverse it
    CHECK(game.Heading() == Dir::kUp);

    // Walls kill.
    while (true) {
        Step step = game.Advance(first_free);
        if (step == Step::kDied) break;
        CHECK(game.Body().front().y >= 0);
    }
    CHECK(game.Body().front().y == 0);

    // Eating grows it and scores.
    SnakeGame eater(8, 6);
    Random before_head = [] { return 27u; };  // Index among free cells, ahead of the head
    eater.Reset(before_head);
    SnakeGame::Cell food = eater.Food();
    CHECK(food.y == 3 && food.x > 4);
    Step step = Step::kMoved;
    for (int i = 0; i < 8 && step == Step::kMoved; ++i) step = eater.Advance(first_free);
    CHECK(step == Step::kAte);
    CHECK(eater.Score() == 1);
    CHECK(eater.Body().size() == 4);
    CHECK(eater.Body().front() == food);

    // Following its own tail is allowed: the tail moves away that step. The
    // snake that just ate is 4 long, so its third turn lands on the tail's cell.
    const SnakeGame::Cell tail_cell = eater.Body().back();
    eater.TurnToward(0, 1);
    CHECK(eater.Advance(first_free) == Step::kMoved);
    eater.TurnToward(-1, 0);
    CHECK(eater.Advance(first_free) == Step::kMoved);
    const SnakeGame::Cell about_to_vacate = eater.Body().back();
    CHECK(!(about_to_vacate == tail_cell));
    eater.TurnToward(0, -1);
    CHECK(eater.Advance(first_free) == Step::kMoved);
    CHECK(eater.Body().front() == about_to_vacate);

    // A long random game keeps food off the body and the body legal.
    std::mt19937 engine(3);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    for (int round = 0; round < 200; ++round) {
        SnakeGame g(6, 5);
        g.Reset(random);
        for (int i = 0; i < 300; ++i) {
            auto head = g.Body().front();
            auto f = g.Food();
            g.TurnToward(f.x - head.x, f.y - head.y);
            Step s = g.Advance(random);
            if (s == Step::kDied || s == Step::kWon) break;
            std::set<std::pair<int, int>> seen;
            for (const auto& c : g.Body()) {
                CHECK(c.x >= 0 && c.x < 6 && c.y >= 0 && c.y < 5);
                CHECK(seen.insert({c.x, c.y}).second);
                CHECK(!(c == g.Food()));
            }
        }
    }

    // Filling the board is a win, not a crash.
    SnakeGame tiny(4, 1);
    tiny.Reset(first_free);  // Body at x=2,1,0; the only free cell is x=3
    CHECK(tiny.Food().x == 3);
    CHECK(tiny.Advance(first_free) == Step::kWon);
    CHECK(tiny.Score() == 1);
}

static void TestCopy() {
    using Result = CopyGame::Result;
    std::mt19937 engine(3);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };

    CopyGame game;
    CHECK(game.Press(0) == Result::kIgnored);  // Not started
    game.Reset(random);
    CHECK(game.Length() == 1);
    CHECK(game.RoundsDone() == 0);

    // Each round keeps the earlier steps and adds one.
    for (int round = 1; round <= 10; ++round) {
        std::vector<int> before;
        for (int i = 0; i < game.Length(); ++i) {
            CHECK(game.Step(i) >= 0 && game.Step(i) < CopyGame::kPads);
            before.push_back(game.Step(i));
        }
        for (int i = 0; i < game.Length() - 1; ++i) {
            CHECK(game.Press(game.Step(i)) == Result::kRight);
        }
        CHECK(game.Press(game.Step(game.Length() - 1)) == Result::kRoundDone);
        CHECK(game.RoundsDone() == round);
        CHECK(game.Press(0) == Result::kIgnored);  // Between rounds
        game.NextRound(random);
        game.NextRound(random);  // Twice must not skip ahead: the round has restarted
        CHECK(game.Length() == round + 1);
        for (size_t i = 0; i < before.size(); ++i) CHECK(game.Step(i) == before[i]);
    }

    // A wrong pad ends the game, even part way through a round.
    CHECK(game.Press(game.Step(0)) == Result::kRight);
    const int rounds = game.RoundsDone();
    const int length = game.Length();
    CHECK(game.Press((game.Step(1) + 1) % CopyGame::kPads) == Result::kWrong);
    CHECK(game.Press(game.Step(1)) == Result::kIgnored);
    game.NextRound(random);
    CHECK(game.Length() == length);
    CHECK(game.RoundsDone() == rounds);

    // The longest sequence is a win, not an overflow.
    game.Reset(random);
    Result last = Result::kIgnored;
    for (int round = 1; round <= CopyGame::kMaxLength; ++round) {
        for (int i = 0; i < game.Length(); ++i) last = game.Press(game.Step(i));
        if (round < CopyGame::kMaxLength) {
            CHECK(last == Result::kRoundDone);
            game.NextRound(random);
        }
    }
    CHECK(last == Result::kWon);
    CHECK(game.RoundsDone() == CopyGame::kMaxLength);
    game.NextRound(random);
    CHECK(game.Length() == CopyGame::kMaxLength);
    CHECK(game.Press(0) == Result::kIgnored);
}

static int RobotsUp(const BopGame& game) {
    int up = 0;
    for (int i = 0; i < BopGame::kCells; ++i) up += game.IsUp(i);
    return up;
}

static void TestBop() {
    std::mt19937 engine(4);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };

    BopGame game;
    game.Reset();
    CHECK(!game.IsOver());
    CHECK(game.TimeLeftMs() == BopGame::kRoundMs);
    CHECK(RobotsUp(game) == 0);
    CHECK(!game.Bop(0));  // Nothing there
    CHECK(!game.Bop(-1));
    CHECK(!game.Bop(BopGame::kCells));
    CHECK(game.Score() == 0);

    // The first robot appears straight away, scores once, and goes down.
    game.Advance(100, random);
    CHECK(RobotsUp(game) == 1);
    int cell = 0;
    while (!game.IsUp(cell)) ++cell;
    CHECK(game.Bop(cell));
    CHECK(!game.IsUp(cell));
    CHECK(!game.Bop(cell));
    CHECK(game.Score() == 1);

    // A robot left alone ducks away without scoring.
    game.Reset();
    game.Advance(100, random);
    cell = 0;
    while (!game.IsUp(cell)) ++cell;
    int waited = 0;
    while (game.IsUp(cell) && waited < 5000) {
        game.Advance(100, random);
        waited += 100;
    }
    CHECK(waited >= 1000 && waited <= 1600);
    CHECK(game.Score() == 0);

    // A round: playable throughout, faster at the end than at the start.
    game.Reset();
    int spawned_first = 0, spawned_last = 0, elapsed = 0;
    std::array<bool, BopGame::kCells> was_up{};
    while (!game.IsOver()) {
        game.Advance(100, random);
        elapsed += 100;
        for (int i = 0; i < BopGame::kCells; ++i) {
            if (game.IsUp(i) && !was_up[i]) {
                if (elapsed <= 10000) ++spawned_first;
                if (elapsed > 20000) ++spawned_last;
            }
            was_up[i] = game.IsUp(i);
        }
        CHECK(RobotsUp(game) <= 4);
    }
    CHECK(elapsed == BopGame::kRoundMs);
    CHECK(spawned_first >= 8);
    CHECK(spawned_last > spawned_first);

    // When time is up everything stops.
    CHECK(RobotsUp(game) == 0);
    CHECK(game.TimeLeftMs() == 0);
    game.Advance(100, random);
    CHECK(RobotsUp(game) == 0);
    CHECK(!game.Bop(0));

    // A long step can't take the clock below zero.
    game.Reset();
    game.Advance(BopGame::kRoundMs + 5000, random);
    CHECK(game.IsOver());
    CHECK(game.TimeLeftMs() == 0);
}

// Only half of all arrangements of a 3x3 board can be solved: those with an even
// number of out-of-order pairs of tiles.
static bool Solvable(const SlidePuzzle& puzzle) {
    int inversions = 0;
    for (int a = 0; a < SlidePuzzle::kCells; ++a) {
        for (int b = a + 1; b < SlidePuzzle::kCells; ++b) {
            if (puzzle.At(a) != 0 && puzzle.At(b) != 0 && puzzle.At(a) > puzzle.At(b)) ++inversions;
        }
    }
    return inversions % 2 == 0;
}

static void TestSlide() {
    std::mt19937 engine(5);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };

    SlidePuzzle puzzle;
    for (int game = 0; game < 300; ++game) {
        puzzle.Shuffle(random);
        CHECK(!puzzle.IsSolved());
        CHECK(puzzle.Moves() == 0);
        CHECK(Solvable(puzzle));
        std::set<int> tiles;
        for (int i = 0; i < SlidePuzzle::kCells; ++i) tiles.insert(puzzle.At(i));
        CHECK(tiles.size() == SlidePuzzle::kCells);
        CHECK(*tiles.begin() == 0 && *tiles.rbegin() == 8);

        // Exactly the neighbours of the gap can move, with no wrapping between rows.
        int gap = 0;
        while (puzzle.At(gap) != 0) ++gap;
        for (int cell = -1; cell <= SlidePuzzle::kCells; ++cell) {
            SlidePuzzle copy = puzzle;
            const bool beside = cell >= 0 && cell < SlidePuzzle::kCells &&
                                ((cell == gap - 3) || (cell == gap + 3) ||
                                 (cell == gap - 1 && gap % 3 != 0) || (cell == gap + 1 && gap % 3 != 2));
            CHECK(copy.Slide(cell) == beside);
            if (beside) {
                CHECK(copy.At(gap) == puzzle.At(cell));
                CHECK(copy.At(cell) == 0);
                CHECK(copy.Moves() == 1);
            } else {
                CHECK(copy.Moves() == 0);
            }
        }
    }

    // A random walk with a legal move at every step stays solvable. Slide only
    // reports solved at the real end state, and a solved board stops moving.
    for (int solved = 0; solved < 3;) {
        puzzle.Shuffle(random);
        for (int step = 0; step < 200000 && !puzzle.IsSolved(); ++step) puzzle.Slide(random() % 9);
        if (!puzzle.IsSolved()) continue;
        ++solved;
        for (int i = 0; i < 8; ++i) CHECK(puzzle.At(i) == i + 1);
        CHECK(puzzle.At(8) == 0);
        const int moves = puzzle.Moves();
        CHECK(!puzzle.Slide(5));
        CHECK(!puzzle.Slide(7));
        CHECK(puzzle.Moves() == moves);
    }
}

static void TestFourInARow() {
    using Mark = FourInARow::Mark;
    std::mt19937 engine(11);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    Random never_blunder = [] { return 99u; };

    FourInARow game;
    game.Reset();
    CHECK(!game.IsOver());
    CHECK(game.Drop(-1, Mark::kPlayer) == -1);
    CHECK(game.Drop(7, Mark::kPlayer) == -1);
    // Discs stack up, and a full column refuses more.
    for (int row = 0; row < FourInARow::kRows; ++row) {
        CHECK(game.Drop(0, row % 2 == 0 ? Mark::kPlayer : Mark::kComputer) == row);
    }
    CHECK(game.Drop(0, Mark::kPlayer) == -1);
    CHECK(game.Winner() == Mark::kEmpty);

    // Each direction wins, and reports its line.
    game.Reset();
    for (int col = 2; col < 6; ++col) game.Drop(col, Mark::kPlayer);
    CHECK(game.Winner() == Mark::kPlayer && game.IsOver());
    CHECK(game.Drop(0, Mark::kComputer) == -1);  // Over
    const auto line = game.WinningLine();
    for (int i = 0; i < FourInARow::kLine; ++i) {
        CHECK(line[i].col == 2 + i && line[i].row == 0);
    }
    game.Reset();
    for (int i = 0; i < 4; ++i) game.Drop(3, Mark::kComputer);
    CHECK(game.Winner() == Mark::kComputer);
    // A rising diagonal, and a falling one.
    for (int flip = 0; flip < 2; ++flip) {
        game.Reset();
        for (int i = 0; i < 4; ++i) {
            const int col = flip == 0 ? i : 6 - i;
            for (int under = 0; under < i; ++under) game.Drop(col, Mark::kComputer);
            game.Drop(col, Mark::kPlayer);
        }
        CHECK(game.Winner() == Mark::kPlayer);
    }

    // Takes a win ahead of a block.
    game.Reset();
    for (int i = 0; i < 3; ++i) {
        game.Drop(0, Mark::kPlayer);
        game.Drop(1, Mark::kComputer);
    }
    CHECK(game.ChooseComputerMove(never_blunder, 0) == 1);
    // Blocks when it cannot win.
    game.Reset();
    for (int i = 0; i < 3; ++i) game.Drop(4, Mark::kPlayer);
    CHECK(game.ChooseComputerMove(never_blunder, 0) == 4);
    // Does not play under a square the player would win on, though it is the
    // middle column it otherwise prefers.
    game.Reset();
    game.Drop(0, Mark::kComputer);
    game.Drop(1, Mark::kPlayer);
    game.Drop(2, Mark::kComputer);
    for (int col = 0; col < 3; ++col) game.Drop(col, Mark::kPlayer);
    CHECK(game.ChooseComputerMove(never_blunder, 0) != 3);

    // Whole games between random players always end, on a legal move.
    for (int round = 0; round < 300; ++round) {
        game.Reset();
        Mark mark = Mark::kPlayer;
        int moves = 0;
        while (!game.IsOver()) {
            const int col = mark == Mark::kComputer ? game.ChooseComputerMove(random, 25) : static_cast<int>(random() % 7);
            if (game.Drop(col, mark) < 0) {
                CHECK(mark == Mark::kPlayer);  // Only the random player may pick a full column
                continue;
            }
            mark = mark == Mark::kPlayer ? Mark::kComputer : Mark::kPlayer;
            ++moves;
        }
        CHECK(moves <= FourInARow::kCols * FourInARow::kRows);
    }
}

static void TestSums() {
    std::mt19937 engine(12);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    for (int round = 0; round < 200; ++round) {
        SumsGame game;
        game.Start(random);
        int expected = 0;
        int questions = 0;
        do {
            CHECK(game.Question() == questions);
            CHECK(game.Total() >= 0 && game.Total() <= (questions < 5 ? 10 : 20));
            CHECK(game.A() > 0 && game.B() > 0);
            std::set<int> shown;
            int answer = -1;
            for (int i = 0; i < SumsGame::kChoices; ++i) {
                CHECK(game.Choice(i) >= 0);
                CHECK(shown.insert(game.Choice(i)).second);
                if (game.Choice(i) == game.Total()) answer = i;
            }
            CHECK(answer >= 0);
            if (answer < 0) return;
            CHECK(!game.Next(random));  // Not answered yet

            const bool miss = questions % 3 == 0;
            if (miss) {
                const int wrong = (answer + 1) % SumsGame::kChoices;
                CHECK(game.Answer(wrong) == SumsGame::Result::kWrong);
                CHECK(game.Answer(wrong) == SumsGame::Result::kIgnored);
            } else {
                ++expected;
            }
            CHECK(game.Answer(answer) == SumsGame::Result::kRight);
            CHECK(game.Answered());
            CHECK(game.Answer(answer) == SumsGame::Result::kIgnored);
            ++questions;
        } while (game.Next(random));
        CHECK(questions == SumsGame::kQuestions);
        CHECK(game.IsOver() && game.Score() == expected);
    }
}

static void TestCatch() {
    std::mt19937 engine(13);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };

    // Standing still with stars falling elsewhere loses all the lives.
    CatchGame game;
    game.Reset();
    CHECK(game.Lives() == CatchGame::kStartLives && !game.IsOver());
    game.MoveBasket(-500);
    CHECK(game.BasketLeft() == 0);
    game.MoveBasket(5000);
    CHECK(game.BasketLeft() == CatchGame::kWidth - CatchGame::kBasketWidth);

    Random stars_left = [] { return 50u; };  // x = 50, never a red mark (50 % 100 >= 25)
    game.Reset();
    game.MoveBasket(5000);
    for (int t = 0; t < 60000 && !game.IsOver(); t += 40) game.Advance(40, stars_left);
    CHECK(game.IsOver() && game.Score() == 0);
    const auto after = game.Advance(40, stars_left);
    CHECK(after.caught == 0 && after.lost == 0);  // Stops when over

    // A player who follows the lowest star scores well, though the red marks
    // that land in the basket end every game. Items stay inside the field.
    int total = 0;
    for (int round = 0; round < 8; ++round) {
        game.Reset();
        int caught = 0;
        for (int t = 0; t < 300000 && !game.IsOver(); t += 40) {
            const CatchGame::Item* target = nullptr;
            for (int i = 0; i < CatchGame::kMaxItems; ++i) {
                const auto& item = game.At(i);
                if (!item.active) continue;
                CHECK(item.x >= 0 && item.x + CatchGame::kItemSize <= CatchGame::kWidth);
                // A star already below the rim is lost; do not chase it.
                const bool above_rim = item.y_milli / 1000 + CatchGame::kItemSize <= CatchGame::kBasketTop;
                if (!item.bad && above_rim && (target == nullptr || item.y_milli > target->y_milli)) {
                    target = &item;
                }
            }
            if (target != nullptr) game.MoveBasket(target->x + CatchGame::kItemSize / 2);
            caught += game.Advance(40, random).caught;
        }
        CHECK(caught == game.Score());
        CHECK(game.IsOver());
        total += caught;
    }
    CHECK(total > 80);
}

static void TestHop() {
    std::mt19937 engine(14);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };

    // Without jumping the first block ends it.
    HopGame game;
    game.Reset(random);
    int t = 0;
    while (game.Advance(33, random) && t < 60000) t += 33;
    CHECK(game.IsOver() && game.Score() == 0);
    CHECK(!game.Advance(33, random));

    // Jumping at the right moment always clears the blocks, at every speed: the
    // gaps and the jump leave room for a player who times it.
    for (int round = 0; round < 20; ++round) {
        game.Reset(random);
        int ms = 0;
        int peak = 0;
        while (game.Score() < 60 && !game.IsOver() && ms < 600000) {
            for (int i = 0; i < HopGame::kMaxBlocks; ++i) {
                const auto& block = game.At(i);
                if (!block.active || block.passed) continue;
                const int ahead = block.x_milli / 1000 - (HopGame::kRobotX + HopGame::kRobotSize);
                if (ahead >= 0 && ahead < game.Speed() * 15 / 100) game.Jump();
            }
            game.Advance(33, random);
            if (game.RobotHeight() > peak) peak = game.RobotHeight();
            CHECK(game.RobotHeight() >= 0);
            ms += 33;
        }
        CHECK(!game.IsOver());
        CHECK(game.Score() >= 60);
        CHECK(peak > 44);  // Higher than the tallest block
        CHECK(game.Speed() <= 300);
    }

    // A jump pressed just before landing still happens; one long before does not.
    game.Reset(random);
    game.Jump();
    game.Advance(33, random);
    CHECK(game.RobotHeight() > 0);
    while (game.RobotHeight() > 5 && !game.IsOver()) game.Advance(33, random);
    game.Jump();
    for (int i = 0; i < 4 && !game.IsOver(); ++i) game.Advance(33, random);
    CHECK(game.IsOver() || game.RobotHeight() > 0);
}

static void TestOddOne() {
    std::mt19937 engine(15);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    OddOneGame game;
    game.Reset(random);
    int last_gap = 1000;
    for (int round = 0; round < 40; ++round) {
        CHECK(game.Odd() >= 0 && game.Odd() < OddOneGame::kCells);
        CHECK(game.BaseColor() != game.OddColor());
        CHECK(game.OddColor() <= 0xFFFFFF);
        const int gap = static_cast<int>((game.OddColor() >> 16) - (game.BaseColor() >> 16));
        CHECK(gap >= 20 && gap <= last_gap);  // Gets harder, stays visible
        last_gap = gap;
        CHECK(game.Score() == round);
        CHECK(game.Tap(game.Odd(), random));
    }
    CHECK(!game.Tap(-1, random) && !game.IsOver());
    CHECK(!game.Tap(OddOneGame::kCells, random) && !game.IsOver());
    const int score = game.Score();
    CHECK(!game.Tap((game.Odd() + 1) % OddOneGame::kCells, random));
    CHECK(game.IsOver() && game.Score() == score);
    CHECK(!game.Tap(game.Odd(), random));  // Over
    game.Reset(random);
    CHECK(!game.IsOver() && game.Score() == 0);
}

static void TestLightsOut() {
    std::mt19937 engine(21);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    LightsOut game;
    for (int level = 1; level <= 15; ++level) {
        game.NewPuzzle(level, random);
        CHECK(game.Level() == level && game.Moves() == 0 && !game.IsSolved());
        // Pressing a cell twice puts it back.
        LightsOut copy = game;
        CHECK(copy.Press(7) && copy.Press(7));
        for (int c = 0; c < LightsOut::kCells; ++c)
            CHECK(copy.On(c) == game.On(c));
        // Solvable: "chase the lights" down, then try the 2^5 top-row presses.
        bool solved = false;
        for (int top = 0; top < 32 && !solved; ++top) {
            LightsOut trial = game;
            for (int col = 0; col < LightsOut::kSize; ++col) {
                if (top & (1 << col))
                    trial.Press(col);
            }
            for (int row = 1; row < LightsOut::kSize; ++row) {
                for (int col = 0; col < LightsOut::kSize; ++col) {
                    if (trial.On((row - 1) * LightsOut::kSize + col))
                        trial.Press(row * LightsOut::kSize + col);
                }
            }
            solved = trial.IsSolved();
            if (solved)
                CHECK(!trial.Press(0));  // Nothing to do once solved
        }
        CHECK(solved);
    }
    CHECK(!game.Press(-1) && !game.Press(LightsOut::kCells));
    // A middle press flips five lights, a corner three.
    game.NewPuzzle(1, random);
    LightsOut before = game;
    game.Press(12);
    int flipped = 0;
    for (int c = 0; c < LightsOut::kCells; ++c)
        flipped += game.On(c) != before.On(c);
    CHECK(flipped == 5);
    before = game;
    game.Press(0);
    flipped = 0;
    for (int c = 0; c < LightsOut::kCells; ++c)
        flipped += game.On(c) != before.On(c);
    CHECK(flipped == 3 || game.IsSolved());
}

static void TestMerge() {
    using Dir = MergeGame::Direction;
    // Never a 4 (needs % 10 == 0); new tiles go into the last empty cells.
    Random twos = [] { return 13u; };
    MergeGame game;
    game.Reset(twos);
    int tiles = 0;
    for (int c = 0; c < MergeGame::kCells; ++c)
        tiles += game.At(c) != 0;
    CHECK(tiles == 2 && game.Score() == 0);

    // 2 2 2 2 -> 4 4 (each tile merges once), score 8, plus one new tile.
    game.SetTiles({2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(game.Move(Dir::kLeft, twos));
    CHECK(game.At(0) == 4 && game.At(1) == 4 && game.At(2) == 0 && game.At(3) == 0);
    CHECK(game.Score() == 8);
    tiles = 0;
    for (int c = 0; c < MergeGame::kCells; ++c)
        tiles += game.At(c) != 0;
    CHECK(tiles == 3);

    // 4 0 4 8 right -> 0 0 8 8 (the 8 made this move does not merge again).
    game.SetTiles({4, 0, 4, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(game.Move(Dir::kRight, twos));
    CHECK(game.At(2) == 8 && game.At(3) == 8);

    // Columns: up and down.
    game.SetTiles({2, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0});
    CHECK(game.Move(Dir::kUp, twos));
    CHECK(game.At(0) == 4 && game.At(4) == 4);
    game.SetTiles({2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0});
    CHECK(game.Move(Dir::kDown, twos));
    CHECK(game.At(12) == 4 && game.At(0) == 0);

    // Nothing moves: no new tile.
    game.SetTiles({2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(!game.Move(Dir::kLeft, twos));
    CHECK(game.At(2) == 0 && game.At(4) == 0);

    // Full board without equal neighbours is over; one pair keeps it going.
    game.SetTiles({2, 4, 2, 4, 4, 2, 4, 2, 2, 4, 2, 4, 4, 2, 4, 2});
    CHECK(game.IsOver() && game.Largest() == 4);
    game.SetTiles({2, 4, 2, 4, 4, 2, 4, 2, 2, 4, 2, 4, 4, 2, 4, 4});
    CHECK(!game.IsOver());

    // A long random game stays sane.
    std::mt19937 engine(22);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    game.Reset(random);
    int moves = 0;
    while (!game.IsOver() && moves < 5000) {
        game.Move(static_cast<Dir>(engine() % 4), random);
        ++moves;
    }
    CHECK(game.IsOver());
    CHECK(game.Largest() >= 32 && game.Score() > 0);
}

static void TestCount() {
    std::mt19937 engine(23);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    CountGame game;
    game.Start(random);
    int last = 0;
    for (int q = 0; q < CountGame::kQuestions; ++q) {
        CHECK(game.Question() == q);
        const int max = q < CountGame::kQuestions / 2 ? 5 : CountGame::kMaxCount;
        CHECK(game.Count() >= 1 && game.Count() <= max);
        CHECK(game.Count() != last);
        last = game.Count();
        CHECK(game.Kind() >= 0 && game.Kind() < CountGame::kKinds);
        std::set<int> slots;
        for (int i = 0; i < game.Count(); ++i) {
            CHECK(game.Slot(i) >= 0 && game.Slot(i) < CountGame::kSlots);
            slots.insert(game.Slot(i));
        }
        CHECK(static_cast<int>(slots.size()) == game.Count());
        std::set<int> choices;
        int right = -1;
        for (int i = 0; i < CountGame::kChoices; ++i) {
            CHECK(game.Choice(i) >= 1);
            choices.insert(game.Choice(i));
            if (game.Choice(i) == game.Count())
                right = i;
        }
        CHECK(choices.size() == CountGame::kChoices && right >= 0);
        if (q % 2 == 1)
            CHECK(game.Answer((right + 1) % CountGame::kChoices) == CountGame::Result::kWrong);
        CHECK(game.Answer(right) == CountGame::Result::kRight);
        CHECK(game.Answer(right) == CountGame::Result::kIgnored);
        CHECK(game.Next(random) == (q < CountGame::kQuestions - 1));
    }
    CHECK(game.IsOver() && game.Score() == CountGame::kQuestions / 2);
}

static void TestBalloon() {
    std::mt19937 engine(24);
    Random random = [&]() { return static_cast<uint32_t>(engine()); };
    BalloonGame game;
    game.Reset();
    CHECK(game.Lives() == BalloonGame::kStartLives && game.Score() == 0);
    CHECK(game.Tap(100, 100) == -1);

    // Pop every balloon as soon as it is fully in view: nothing escapes.
    int escaped = 0;
    for (int t = 0; t < 60000; t += 40) {
        escaped += game.Advance(40, random);
        for (int i = 0; i < BalloonGame::kMaxBalloons; ++i) {
            const auto& b = game.At(i);
            if (b.active && b.y_milli / 1000 < BalloonGame::kHeight - BalloonGame::kSize) {
                CHECK(game.Tap(b.x + BalloonGame::kSize / 2,
                               b.y_milli / 1000 + BalloonGame::kSize / 2) == i);
            }
        }
    }
    CHECK(escaped == 0 && game.Lives() == BalloonGame::kStartLives);
    CHECK(game.Score() > 40);
    CHECK(game.Speed() > 45 && game.Speed() <= 160);

    // Ignore them all: three escape and the game ends.
    game.Reset();
    int ms = 0;
    while (!game.IsOver() && ms < 120000) {
        game.Advance(40, random);
        ms += 40;
    }
    CHECK(game.IsOver() && game.Lives() == 0 && game.Score() == 0);
    CHECK(game.Advance(40, random) == 0);
    CHECK(game.Tap(0, 0) == -1);
}

int main() {
    TestTicTacToe();
    TestMemory();
    TestSnake();
    TestCopy();
    TestBop();
    TestSlide();
    TestFourInARow();
    TestSums();
    TestCatch();
    TestHop();
    TestOddOne();
    TestLightsOut();
    TestMerge();
    TestCount();
    TestBalloon();
    if (failures == 0) printf("OK\n");
    return failures == 0 ? 0 : 1;
}
