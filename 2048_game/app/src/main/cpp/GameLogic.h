#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <vector>
#include <random>

enum class MoveDirection {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

class GameLogic {
public:
    GameLogic();
    void reset();
    bool move(MoveDirection direction);
    void addRandomTile();
    const std::vector<std::vector<int>>& getGrid() const { return grid_; }
    int getScore() const { return score_; }
    bool isGameOver() const;

private:
    std::vector<std::vector<int>> grid_;
    int score_;
    std::mt19937 gen_;

    bool slideAndMerge(std::vector<int>& row);
};

#endif // GAME_LOGIC_H
