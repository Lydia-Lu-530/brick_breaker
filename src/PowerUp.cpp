#include "../include/PowerUp.h"

PowerUp::PowerUp(Vector2 position)
    : pos(position), speed(200.0f), active(true), type(PowerUpType::PADDLE_EXTEND)
{}

void PowerUp::Update(float deltaTime) {
    if (!active) return;
    pos.y += speed * deltaTime;
    if (pos.y > 800) active = false;
}

void PowerUp::Draw() {
    if (!active) return;
    DrawTriangle(
        {pos.x, pos.y - 10},
        {pos.x - 10, pos.y + 10},
        {pos.x + 10, pos.y + 10},
        BLUE
    );
}

bool PowerUp::CheckPaddleCollision(Rectangle paddleRect) {
    if (!active) return false;
    return CheckCollisionPointRec(pos, paddleRect);
}