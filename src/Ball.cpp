#include "../include/Ball.h"

// 球的构造函数
Ball::Ball(float x, float y, float radius, float vx, float vy) {
    this->pos = {x, y};
    this->radius = radius;
    this->vel = {vx, vy};
}

// 更新球的位置
void Ball::Update() {
    pos.x += vel.x;
    pos.y += vel.y;
}

// 检测球与边界的碰撞
void Ball::CheckBoundaryCollision(int screenWidth, int screenHeight) {
    // 左右边界反弹
    if (pos.x - radius <= 0 || pos.x + radius >= screenWidth) {
        vel.x *= -1;
    }
    // 上边界反弹
    if (pos.y - radius <= 0) {
        vel.y *= -1;
    }
}

// 重置球的位置和速度
void Ball::Reset(int screenWidth, int screenHeight) {
    pos = { (float)screenWidth/2, (float)screenHeight/2 };
    vel = {4.0f, -4.0f};
}