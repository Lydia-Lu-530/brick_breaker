#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include "PowerUp.h"
#include <random>
#include <memory>
#include <mutex>
#include <future>
#include <chrono>
#include <thread>

// 粒子结构体（砖块破碎特效）
struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
};

enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY,
    LEADERBOARD
};

class Game {
public:
    Game(int screenWidth, int screenHeight, const char* title);
    ~Game();
    void Run();

    // 道具效果接口
    void ExtendPaddle(float scale, float duration);
    void SpawnMultiBall();
    void SlowBall(float scale, float duration);
    void ResetPaddleSize();
    void ResetBallSpeed();
    Rectangle GetPaddleRect();
    int GetScreenHeight();

private:
    void LoadConfig();
    void Init();
    void HandleStateTransition();
    void Update();
    void Draw();
    bool CheckAllBricksDestroyed();
    void Reset();
    void SpawnBrickParticles(Vector2 pos, Color color); // 粒子生成函数声明

    nlohmann::json config;
    nlohmann::json powerUpConfig; // 道具JSON配置

    int m_screenWidth;
    int m_screenHeight;
    const char* m_title;

    Ball* m_ball;
    Paddle* m_paddle;
    std::vector<Brick> m_bricks;
    std::vector<std::unique_ptr<PowerUp>> m_powerUps;
    std::vector<Ball> m_extraBalls;  // 额外球容器
    std::vector<Particle> m_particles; // 粒子容器

    int m_lives;
    int m_score;
    GameState m_currentState;

    std::mt19937 m_rng; // 随机数生成器

    // 道具状态变量
    float m_paddleOriginalWidth;
    float m_ballOriginalSpeedX, m_ballOriginalSpeedY;
    float m_paddleExtendTimer;
    float m_ballSlowTimer;

     // 异步加载相关
    std::mutex m_loadingMutex;          // 互斥锁保护共享状态
    std::future<void> m_loadingFuture;  // 后台加载任务句柄
    bool m_isLoading = false;           // 加载状态标志
    bool m_loadingComplete = false;     // 加载完成标志
    Color m_originalBrickColor = RED;   // 砖块原始颜色
    void SimulateAsyncLoad();
};

#endif