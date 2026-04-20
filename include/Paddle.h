#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

struct Paddle {
    Vector2 pos;
    float width;
    float height;
    float speed;
    Rectangle rect;

    Paddle(float x, float y, float width, float height, float speed);

    // 更新挡板位置（键盘控制）
    // Paddle.h
    void Update(int screenWidth, float deltaTime);

    // 获取挡板的碰撞矩形
    Rectangle GetRect() const;

    void SetWidth(float newWidth); // 确认参数是 float 类型
};

#endif