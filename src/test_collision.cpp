// src/test_collision.cpp
#include <gtest/gtest.h>
#include "../include/Ball.h"   // 引入你的Ball头文件
#include "../include/Brick.h"  // 引入你的Brick头文件
#include <vector>

// 测试1：小球撞到存活砖块 → 返回true，分数增加，砖块破碎
TEST(CollisionTest, BallHitBrick) {
    // 👉 关键修改：用你的带参构造创建Ball对象
    // 参数：x, y, radius, vx, vy（速度不影响碰撞检测，随便填0即可）
    Ball ball(100.0f, 100.0f, 10.0f, 0.0f, 0.0f);

    // 创建金色砖块（分值5）
    Brick brick(90.0f, 90.0f, 70.0f, 25.0f, GOLD, 5);
    brick.alive = true;

    std::vector<Brick> bricks = {brick};
    int score = 0;

    // 调用碰撞函数
    bool result = CheckBallBrickCollision(ball, bricks, score);

    // 验证结果
    EXPECT_TRUE(result);                  // 碰撞函数应返回true
    EXPECT_EQ(score, 5);                  // 分数应+5
    EXPECT_FALSE(bricks[0].alive);        // 砖块应标记为破碎
}

// 测试2：小球没撞到砖块 → 返回false，分数/状态不变
TEST(CollisionTest, BallNotHitBrick) {
    // 小球位置远离砖块（200,200）
    Ball ball(200.0f, 200.0f, 10.0f, 0.0f, 0.0f);

    Brick brick(90.0f, 90.0f, 70.0f, 25.0f, RED, 1);
    brick.alive = true;

    std::vector<Brick> bricks = {brick};
    int score = 0;

    bool result = CheckBallBrickCollision(ball, bricks, score);

    EXPECT_FALSE(result);                 // 无碰撞→返回false
    EXPECT_EQ(score, 0);                  // 分数不变
    EXPECT_TRUE(bricks[0].alive);         // 砖块仍存活
}

// 测试3：小球撞到已破碎的砖块 → 返回false，无变化
TEST(CollisionTest, BallHitBrokenBrick) {
    Ball ball(100.0f, 100.0f, 10.0f, 0.0f, 0.0f);

    Brick brick(90.0f, 90.0f, 70.0f, 25.0f, ORANGE, 2);
    brick.alive = false;  // 初始已破碎

    std::vector<Brick> bricks = {brick};
    int score = 0;

    bool result = CheckBallBrickCollision(ball, bricks, score);

    EXPECT_FALSE(result);                 // 无有效碰撞→返回false
    EXPECT_EQ(score, 0);                  // 分数不变
    EXPECT_FALSE(bricks[0].alive);        // 砖块仍为破碎状态
}

// 主函数：运行所有测试
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}