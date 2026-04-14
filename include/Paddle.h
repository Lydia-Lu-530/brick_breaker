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
    void Update(int screenWidth, float deltaTime);
    Rectangle GetRect() const;
    void SetWidth(float newWidth);
};

#endif