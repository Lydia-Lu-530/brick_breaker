#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

// 挡板的结构体
struct Paddle {
    Vector2 pos;     // 位置（中心）
    float width;     // 宽度
    float height;    // 高度
    float speed;     // 移动速度

    // 构造函数
    Paddle(float x, float y, float width, float height, float speed);

    // 更新挡板位置（键盘控制）
    // Paddle.h
    void Update(int screenWidth, float deltaTime);

    // 获取挡板的碰撞矩形
    Rectangle GetRect() const;

    void SetWidth(float newWidth); // 确认参数是 float 类型
};

#endif // PADDLE_H