#include "../include/Ball.h"
#include "../include/Paddle.h"
#include "../include/Brick.h"
#include "raylib.h"
#include <vector>
#include <cstdio>  // 新增：包含 sprintf 所需头文件
#include <string> // 确保包含字符串头文件

// 新增：检查是否所有砖块都被打碎
bool AreAllBricksDestroyed(const std::vector<Brick>& bricks) {
    for (const auto& brick : bricks) {
        if (brick.alive) { // 只要有1个砖块存活，就返回false
            return false;
        }
    }
    return true; // 所有砖块都被打碎，返回true
}

int main() {
    // 窗口配置
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "2D 打砖块游戏");
    SetTargetFPS(60);

     // ===== 新增：生命值配置 =====
    int lives = 3;          // 初始3条命
    const int maxLives = 3; // 最大生命值
    int score = 0;          //分数变量

    // 初始化游戏对象
    Ball ball(
        screenWidth/2, screenHeight/2, // 初始位置
        10.0f,                         // 半径
        4.0f, -4.0f                    // 初始速度
    );

    Paddle paddle(
        screenWidth/2, screenHeight - 50, // 初始位置
        120.0f, 20.0f,                    // 宽高
        6.0f                              // 移动速度
    );

    std::vector<Brick> bricks = CreateBricks(screenWidth);

    // 游戏主循环
    while (!WindowShouldClose()) {
        // 新增：生命值为0则不更新逻辑
        if (lives <= 0) {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("GAME OVER!", screenWidth/2 - 80, screenHeight/2, 40, RED);
            // 失败时显示最终分数
            std::string finalScore = "Final Score: " + std::to_string(score);
            DrawText(finalScore.c_str(), screenWidth/2 - 80, screenHeight/2 + 50, 20, GRAY);
        EndDrawing();
        continue; // 跳过后续更新逻辑
        }
        
        // ===== 新增：检测是否所有砖块都被打碎 =====
        bool allBricksDestroyed = AreAllBricksDestroyed(bricks);
        if (allBricksDestroyed) {
            BeginDrawing();
                ClearBackground(RAYWHITE);
                // 胜利提示（居中显示）
                DrawText("CONGRATULATIONS!", screenWidth/2 - 140, screenHeight/2 - 40, 40, GREEN);
                DrawText("YOU WIN!", screenWidth/2 - 80, screenHeight/2 + 20, 40, GREEN);
                // 胜利时显示最终分数
                std::string finalScore = "Final Score: " + std::to_string(score);
                DrawText(finalScore.c_str(), screenWidth/2 - 80, screenHeight/2 + 80, 20, GREEN);
            EndDrawing();
            continue; // 跳过后续游戏逻辑，锁定胜利画面
        }

        // 1. 更新逻辑
        ball.Update();
        paddle.Update(screenWidth);

        // 边界碰撞检测
        ball.CheckBoundaryCollision(screenWidth, screenHeight);

        // 球掉出下边界 → 重置
        if (ball.pos.y + ball.radius >= screenHeight) {
        lives--; // 扣1条命
        if (lives > 0) ball.Reset(screenWidth, screenHeight); // 还有命就重置球
        // 没命了则退出游戏（或加游戏结束逻辑）
        }

        // 球与挡板碰撞
        if (CheckCollisionCircleRec(ball.pos, ball.radius, paddle.GetRect()) && ball.vel.y > 0) {
            ball.vel.y *= -1;
            // 碰撞位置微调水平速度（增加可玩性）
            ball.vel.x = (ball.pos.x - paddle.pos.x) * 0.1f;
        }

        // 球与砖块碰撞
        CheckBallBrickCollision(ball, bricks, score);

        // 2. 绘制渲染
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // 绘制游戏元素
            DrawCircleV(ball.pos, ball.radius, RED);          // 球
            DrawRectangleRec(paddle.GetRect(), BLUE);         // 挡板
            for (const auto& brick : bricks) {                // 砖块
                if (brick.alive) DrawRectangleRec(brick.rect, brick.color);
            }
            
            DrawText("Left/Right to Move Paddle", 10, 10, 20, GRAY);
            DrawText("Ball Drop = Reset Position", 10, 40, 20, GRAY);
        
            // ===== 新增：绘制生命值 =====
            char lifeText[20];
            sprintf(lifeText, "Lives: %d", lives); // 拼接生命值文本
            DrawText(lifeText, screenWidth - 120, 10, 20, RED); // 右上角显示

            // ===== 新增：游戏结束逻辑（可选）=====
            if (lives <= 0) {
                DrawText("GAME OVER!", screenWidth/2 - 150, screenHeight/2, 40, RED);
                // 结束后按ESC退出，或点击窗口关闭
            }

            // ===== 新增：分数显示（右上角，生命值下方）=====
            std::string scoreText = "Score: " + std::to_string(score);
            DrawText(scoreText.c_str(), screenWidth - 120, 40, 20, ORANGE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}