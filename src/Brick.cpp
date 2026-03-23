#include "../include/Brick.h"
#include "../include/Ball.h"

Brick::Brick(float x, float y, float width, float height, Color color, int score) {
    this->rect = {x, y, width, height};
    this->alive = true;
    this->color = color;
    this->score = score; // 初始化分值
}

std::vector<Brick> CreateBricks(int screenWidth) {
    std::vector<Brick> bricks;
    const int brickRows = 5;
    const int brickCols = 10;
    const int brickWidth = 70;
    const int brickHeight = 25;
    const int brickPadding = 5;
    const int startX = (screenWidth - (brickCols*(brickWidth+brickPadding))) / 2;
    const int startY = 50;

    // ===== 重新定义砖块类型：红(10分)、橙(20分)、金(50分) =====
    // 第1行（最上方）：金色砖块（50分）
    for (int x = 0; x < brickCols; x++) {
        bricks.emplace_back(
            startX + x*(brickWidth+brickPadding),
            startY + 0*(brickHeight+brickPadding), // 第1行（最顶）
            brickWidth, brickHeight,
            GOLD, 50 // 金色=50分
        );
    }

    // 第2行：橙色砖块（20分）
    for (int x = 0; x < brickCols; x++) {
        bricks.emplace_back(
            startX + x*(brickWidth+brickPadding),
            startY + 1*(brickHeight+brickPadding), // 第2行
            brickWidth, brickHeight,
            ORANGE, 20 // 橙色=20分
        );
    }

    // 第3-5行（最下面3行）：红色砖块（10分）
    for (int y = 2; y < brickRows; y++) { // y=2/3/4 → 第3/4/5行
        for (int x = 0; x < brickCols; x++) {
            bricks.emplace_back(
                startX + x*(brickWidth+brickPadding),
                startY + y*(brickHeight+brickPadding),
                brickWidth, brickHeight,
                RED, 10 // 红色=10分
            );
        }
    }
    return bricks;
}

// 碰撞函数新增 score 引用，击碎时加分
bool CheckBallBrickCollision(Ball& ball, std::vector<Brick>& bricks, int& score) {
    for (auto& brick : bricks) {
        if (brick.alive && CheckCollisionCircleRec(ball.pos, ball.radius, brick.rect)) {
            brick.alive = false;
            score += brick.score; // 累加对应分值
            ball.vel.y *= -1;
            return true;
        }
    }
    return false;
}