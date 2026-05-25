#include "../include/Paddle.h"
#include "raylib.h"
#include "raymath.h"

// 构造函数：初始化挡板的位置、宽高、移动速度
Paddle::Paddle(float x, float y, float width, float height, float speed) {
    this->pos = {x, y};
    this->width = width;
    this->height = height;
    this->speed = speed;
}
// 更新函数：处理挡板的移动逻辑 + 边界限制
// screenWidth：屏幕宽度  deltaTime：帧间隔时间（保证移动速度平滑）
void Paddle::Update(int screenWidth, float deltaTime) {
    if (IsKeyDown(KEY_LEFT)) pos.x -= speed * deltaTime;
    if (IsKeyDown(KEY_RIGHT)) pos.x += speed * deltaTime;
    pos.x = Clamp(pos.x, width/2, screenWidth - width/2);// 限制挡板范围，防止跑出屏幕左右边界
}
// 获取挡板的矩形区域（用于碰撞检测）
Rectangle Paddle::GetRect() const {
    return { pos.x - width/2, pos.y - height/2, width, height };
}
// 设置挡板宽度（用于道具：延长/缩短挡板效果）
void Paddle::SetWidth(float newWidth) {
    width = newWidth;
}