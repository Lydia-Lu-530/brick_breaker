#include "../include/Paddle.h"
#include "raylib.h"
#include "raymath.h"

Paddle::Paddle(float x, float y, float width, float height, float speed) {
    this->pos = {x, y};
    this->width = width;
    this->height = height;
    this->speed = speed;
}

void Paddle::Update(int screenWidth, float deltaTime) {
    if (IsKeyDown(KEY_LEFT)) pos.x -= speed * deltaTime;
    if (IsKeyDown(KEY_RIGHT)) pos.x += speed * deltaTime;
    pos.x = Clamp(pos.x, width/2, screenWidth - width/2);
}

Rectangle Paddle::GetRect() const {
    return { pos.x - width/2, pos.y - height/2, width, height };
}

void Paddle::SetWidth(float newWidth) {
    width = newWidth;
}