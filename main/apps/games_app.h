#pragma once

#include <functional>
#include <memory>

#include <lvgl.h>

namespace apps {

class GameView;

/**
 * The Games app: a scrolling menu and eleven touch games (Tic-Tac-Toe, Memory,
 * Snake, Copy Me, Bop the Bot, Slide Puzzle, Four in a Row, Quick Sums, Catch
 * Stars, Robot Hop, Odd One Out).
 * The rules live in game_logic.*; this is the screen side. Each game is built
 * when opened and freed when closed, so it costs nothing while not played.
 * Everything runs in the LVGL context.
 */
class GamesApp {
public:
    explicit GamesApp(std::function<void()> on_exit);
    ~GamesApp();

    lv_obj_t* screen() const { return screen_; }
    // Called when the app closes: ends the game being played.
    void OnHide();

private:
    std::function<void()> on_exit_;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* menu_ = nullptr;
    std::unique_ptr<GameView> game_;

    void BuildMenu();
    template <typename Game>
    void Open();
    void CloseGame();
};

}  // namespace apps
