#ifndef POWER_UP_H
#define POWER_UP_H

#include "raylib.h"
#include <vector>

// 前向声明 Game 类，避免循环依赖
class Game;

enum class PowerUpType {
    PADDLE_EXTEND,    // 加长板
    MULTI_BALL,       // 多球
    SLOW_BALL         // 减速球
};

class PowerUp {
public:
    Vector2 pos;
    float speed;
    bool active;
    PowerUpType type;
    float timer;        // 效果持续时间
    float glowAlpha;    // 光晕透明度（粒子特效）

    // 构造函数：必须带 PowerUpType 参数
    PowerUp(Vector2 position, PowerUpType t);
    virtual ~PowerUp() = default;

    virtual void Update(float deltaTime);
    virtual void Draw();
    virtual bool CheckPaddleCollision(Rectangle paddleRect);
    // 纯虚方法：子类必须实现 Activate
    virtual void Activate(Game& game) = 0;

    // 工厂方法：创建对应类型的道具
    static PowerUp* Create(PowerUpType type, Vector2 pos);
};

// 加长板道具
class PaddleExtendPowerUp : public PowerUp {
public:
    PaddleExtendPowerUp(Vector2 pos);
    void Activate(Game& game) override;
};

// 多球道具
class MultiBallPowerUp : public PowerUp {
public:
    MultiBallPowerUp(Vector2 pos);
    void Activate(Game& game) override;
};

// 减速球道具
class SlowBallPowerUp : public PowerUp {
public:
    SlowBallPowerUp(Vector2 pos);
    void Activate(Game& game) override;
};

#endif