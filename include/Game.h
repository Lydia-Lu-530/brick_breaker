#ifndef GAME_H
#define GAME_H

#include "json.hpp"
using json = nlohmann::json;
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

// 粒子结构体（砖块破碎特效）
struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
};
// 状态机
enum class GameState {
    MENU,
    INSTRUCTIONS, // 新增：操作说明弹窗
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
    // 网格初始化
    void InitBrickGrid();
    void Close();// 退出时自动保存 + 释放资源

private:
    void LoadConfig();// 加载游戏配置文件（如config.json
    void Init();// 游戏初始化：创建球、挡板、初始化变量、加载第一关
    void HandleStateTransition();// 处理游戏状态切换（菜单→游戏→暂停→胜利→失败）
    void Update();// 游戏逻辑更新：物理运动、碰撞检测、道具、粒子、倒计时
    void Draw();// 游戏画面绘制：渲染所有物体、UI、菜单、文字
    bool CheckAllBricksDestroyed();// 检查所有砖块是否被销毁（用于判断是否通关）
    void Reset();// 重置游戏：分数、生命、关卡、道具状态全部恢复默认
    void SpawnBrickParticles(Vector2 pos, Color color); // 粒子生成函数声明
    void Unload(); // 释放音效、音频设备等资源

    nlohmann::json config;//创建一个空的 JSON 对象，用来存放、读取、写入游戏的配置 / 数据
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

    // 碰撞检测网格参数
    static const int GRID_COLS = 10;
    static const int GRID_ROWS = 6;
    std::vector<Brick*> m_brickGrid[GRID_COLS][GRID_ROWS];

    // ==========================
    // 粒子对象池（性能优化）
    // ==========================
    struct PoolParticle {
        Vector2 pos;
        Vector2 vel;
        Color color;
        float life;
        float maxLife;
        bool active;
    };

    static const int MAX_PARTICLES = 300;  // 最多同时300个粒子
    PoolParticle m_particlePool[MAX_PARTICLES]; // 对象池（预分配）


    // 关卡相关
    int m_currentLevel = 1;
    std::vector<std::string> m_levelFiles = {
        "levels/level1.json",
        "levels/level2.json",
        "levels/level3.json"
    };

    // 存档相关
    bool m_hasSave = false;

    // 必须的函数声明
    bool LoadLevelFromJSON(const std::string& filename);
    bool LoadSaveGame();
    void SaveGame();

    // 菜单选择相关
    int m_menuOption = 0;  // 0:新游戏 1:继续游戏 2:选择关卡
    int m_selectedLevel = 1;

    bool m_ballAttached;   // 球是否粘在挡板上

    std::vector<bool> m_savedBrickStates;
};

#endif