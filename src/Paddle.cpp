#include "../include/Paddle.h"
#include "raylib.h"
#include "raymath.h" 

// 挡板的构造函数
Paddle::Paddle(float x, float y, float width, float height, float speed) {
    this->pos = {x, y};
    this->width = width;
    this->height = height;
    this->speed = speed;
}

// 更新挡板位置（键盘控制）
void Paddle::Update(int screenWidth) {
    if (IsKeyDown(KEY_LEFT)) pos.x -= speed;
    if (IsKeyDown(KEY_RIGHT)) pos.x += speed;
    // 限制挡板在屏幕内
    pos.x = Clamp(pos.x, width/2, (float)screenWidth - width/2);
}

// 获取挡板的碰撞矩形
Rectangle Paddle::GetRect() const {
    return {
        pos.x - width/2,
        pos.y - height/2,
        width,
        height
    };
}