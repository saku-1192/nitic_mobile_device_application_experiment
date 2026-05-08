#include "GameLogic.h"
#include <algorithm>
#include <ctime>

GameLogic::GameLogic() : grid_(4, std::vector<int>(4, 0)), score_(0), gen_(std::time(nullptr)) {
    reset();
}

void GameLogic::reset() {
    for (auto& row : grid_) {
        std::fill(row.begin(), row.end(), 0);
    }
    score_ = 0;

    // --- テスト用に好きな数字を配置 ---
    grid_[0][0] = 2;
    grid_[0][1] = 4;
    grid_[0][2] = 8;
    grid_[0][3] = 16;
    grid_[1][0] = 32;
    grid_[1][1] = 64;
    grid_[1][2] = 128;
    grid_[1][3] = 256;
    grid_[2][0] = 512;
    grid_[2][1] = 1024;
    grid_[2][2] = 2048;
    grid_[2][3] = 4096; // 4096なども表示できるかテスト
    // ----------------------------
}

void GameLogic::addRandomTile() {
    std::vector<std::pair<int, int>> emptyCells;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (grid_[r][c] == 0) {
                emptyCells.push_back({r, c});
            }
        }
    }

    if (!emptyCells.empty()) {
        std::uniform_int_distribution<> dis(0, emptyCells.size() - 1);
        auto [r, c] = emptyCells[dis(gen_)];
        std::uniform_int_distribution<> valDis(0, 9);
        grid_[r][c] = (valDis(gen_) == 0) ? 4 : 2;
    }
}

bool GameLogic::slideAndMerge(std::vector<int>& row) {
    bool changed = false;
    std::vector<int> newRow;
    for (int val : row) {
        if (val != 0) newRow.push_back(val);
    }

    for (size_t i = 0; i + 1 < newRow.size(); ++i) {
        if (newRow[i] == newRow[i + 1]) {
            newRow[i] *= 2;
            score_ += newRow[i];
            newRow.erase(newRow.begin() + i + 1);
            changed = true;
        }
    }

    while (newRow.size() < 4) {
        newRow.push_back(0);
    }

    if (newRow != row) {
        changed = true;
        row = newRow;
    }
    return changed;
}

bool GameLogic::move(MoveDirection direction) {
    bool changed = false;
    if (direction == MoveDirection::LEFT || direction == MoveDirection::RIGHT) {
        for (int r = 0; r < 4; ++r) {
            std::vector<int> row = grid_[r];
            if (direction == MoveDirection::RIGHT) std::reverse(row.begin(), row.end());
            if (slideAndMerge(row)) changed = true;
            if (direction == MoveDirection::RIGHT) std::reverse(row.begin(), row.end());
            grid_[r] = row;
        }
    } else {
        for (int c = 0; c < 4; ++c) {
            std::vector<int> col(4);
            for (int r = 0; r < 4; ++r) col[r] = grid_[r][c];
            if (direction == MoveDirection::DOWN) std::reverse(col.begin(), col.end());
            if (slideAndMerge(col)) changed = true;
            if (direction == MoveDirection::DOWN) std::reverse(col.begin(), col.end());
            for (int r = 0; r < 4; ++r) grid_[r][c] = col[r];
        }
    }

    if (changed) {
        addRandomTile();
    }
    return changed;
}

bool GameLogic::isGameOver() const {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (grid_[r][c] == 0) return false;
            if (r + 1 < 4 && grid_[r][c] == grid_[r + 1][c]) return false;
            if (c + 1 < 4 && grid_[r][c] == grid_[r][c + 1]) return false;
        }
    }
    return true;
}
