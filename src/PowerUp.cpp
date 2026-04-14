#include "../include/PowerUp.h"
#include "../include/Game.h"
#include <cmath>

// 基类构造函数：必须带 PowerUpType 参数，和头文件完全匹配
PowerUp::PowerUp(Vector2 position, PowerUpType t)
    : pos(position), speed(200.0f), active(true), type(t), timer(5.0f), glowAlpha(1.0f) {}

void PowerUp::Update(float deltaTime) {
    if (!active) return;
    pos.y += speed * deltaTime;
    // 光晕淡入淡出动画
    glowAlpha = 0.5f + 0.5f * sinf(GetTime() * 4.0f);
    if (pos.y > 800) active = false;
}

void PowerUp::Draw() {
    if (!active) return;

    // 绘制光晕（粒子特效）
    Color glowColor;
    switch(type) {
        case PowerUpType::PADDLE_EXTEND: glowColor = BLUE; break;
        case PowerUpType::MULTI_BALL: glowColor = GREEN; break;
        case PowerUpType::SLOW_BALL: glowColor = YELLOW; break;
    }
    DrawCircleV(pos, 15, ColorAlpha(glowColor, glowAlpha * 0.3f));

    // 绘制道具本体
    Color mainColor = glowColor;
    switch(type) {
        case PowerUpType::PADDLE_EXTEND:
            DrawTriangle({pos.x, pos.y-10}, {pos.x-10, pos.y+10}, {pos.x+10, pos.y+10}, mainColor);
            break;
        case PowerUpType::MULTI_BALL:
            DrawCircleV(pos, 8, mainColor);
            break;
        case PowerUpType::SLOW_BALL:
            DrawRectangle(pos.x-8, pos.y-8, 16, 16, mainColor);
            break;
    }
}

bool PowerUp::CheckPaddleCollision(Rectangle paddleRect) {
    if (!active) return false;
    return CheckCollisionPointRec(pos, paddleRect);
}

// 工厂方法实现
PowerUp* PowerUp::Create(PowerUpType type, Vector2 pos) {
    switch(type) {
        case PowerUpType::PADDLE_EXTEND: return new PaddleExtendPowerUp(pos);
        case PowerUpType::MULTI_BALL: return new MultiBallPowerUp(pos);
        case PowerUpType::SLOW_BALL: return new SlowBallPowerUp(pos);
        default: return nullptr;
    }
}

// ====================== 子类实现 ======================
// 加长板道具
PaddleExtendPowerUp::PaddleExtendPowerUp(Vector2 pos)
    : PowerUp(pos, PowerUpType::PADDLE_EXTEND) {}

void PaddleExtendPowerUp::Activate(Game& game) {
    game.ExtendPaddle(1.5f, 5.0f);
}

// 多球道具
MultiBallPowerUp::MultiBallPowerUp(Vector2 pos)
    : PowerUp(pos, PowerUpType::MULTI_BALL) {}

void MultiBallPowerUp::Activate(Game& game) {
    game.SpawnMultiBall();
}

// 减速球道具
SlowBallPowerUp::SlowBallPowerUp(Vector2 pos)
    : PowerUp(pos, PowerUpType::SLOW_BALL) {}

void SlowBallPowerUp::Activate(Game& game) {
    game.SlowBall(0.7f, 5.0f);
}