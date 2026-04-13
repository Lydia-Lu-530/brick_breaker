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

// 游戏状态枚举
enum class GameState {
    MENU,        // 菜单
    PLAYING,     // 游戏中
    PAUSED,      // 暂停
    GAMEOVER,    // 失败
    VICTORY,     // 胜利
    LEADERBOARD  // 排行榜
};

class Game {
public:
    // 构造函数：初始化窗口、游戏对象
    Game(int screenWidth, int screenHeight, const char* title);
    // 析构函数：释放资源
    ~Game();

    // 游戏主循环
    void Run();

private:
    // 加载配置文件
    void LoadConfig();
    // 初始化所有游戏对象
    void Init();
    // 处理状态转换
    void HandleStateTransition();
    // 更新游戏逻辑（按状态分支）
    void Update();
    // 绘制所有元素（按状态分支）
    void Draw();
    // 检查所有砖块是否被击碎
    bool CheckAllBricksDestroyed();
    // 重置游戏
    void Reset();

    // 配置（只保留一次）
    nlohmann::json config;

    // 窗口参数
    int m_screenWidth;
    int m_screenHeight;
    const char* m_title;

    // 游戏对象
    Ball* m_ball;
    Paddle* m_paddle;
    std::vector<Brick> m_bricks;

    // 游戏状态与数据
    int m_lives;
    int m_score;
    GameState m_currentState; // 核心状态机变量

    std::vector<PowerUp> m_powerUps;
};

#endif // GAME_H