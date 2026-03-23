#ifndef BALL_H
#define BALL_H

#include "raylib.h"

// 球的结构体
struct Ball {
    Vector2 pos;    // 位置
    Vector2 vel;    // 速度
    float radius;   // 半径

    // 构造函数
    Ball(float x, float y, float radius, float vx, float vy);

    // 更新球的位置
    void Update();

    // 检测球与边界的碰撞
    void CheckBoundaryCollision(int screenWidth, int screenHeight);

    // 重置球的位置和速度
    void Reset(int screenWidth, int screenHeight);
};

#endif // BALL_H