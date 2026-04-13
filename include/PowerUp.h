#ifndef POWER_UP_H
#define POWER_UP_H

#include "raylib.h"

enum class PowerUpType {
    PADDLE_EXTEND
};

class PowerUp {
public:
    Vector2 pos;
    float speed;
    bool active;
    PowerUpType type;

    PowerUp(Vector2 position);

    void Update(float deltaTime);
    void Draw();
    bool CheckPaddleCollision(Rectangle paddleRect);
};

#endif