#include "../include/Game.h"
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
json config; // 全局配置对象

// 加载配置文件
void Game::LoadConfig() {
    std::ifstream file("config.json");
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open config.json");
        return;
    }
    file >> config;
    file.close();
}

// 构造函数：初始化窗口和游戏
Game::Game(int screenWidth, int screenHeight, const char* title)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_title(title)
    , m_ball(nullptr)
    , m_paddle(nullptr)
    , m_lives(3)
    , m_score(0)
    , m_currentState(GameState::MENU) // 初始状态：菜单
{
    // 加载 JSON 配置
    LoadConfig();

    InitWindow(m_screenWidth, m_screenHeight, m_title);
    SetTargetFPS(60);
    Init(); // 初始化游戏对象
}

// 析构函数：释放资源
Game::~Game() {
    delete m_ball;
    delete m_paddle;
    CloseWindow();
}

// 初始化游戏对象
void Game::Init() {
    // 从配置读取参数
    float ballRadius = config["ball"]["radius"];
    float ballSpeedX = config["ball"]["speed_x"];
    float ballSpeedY = config["ball"]["speed_y"];

    float paddleWidth = config["paddle"]["width"];
    float paddleHeight = config["paddle"]["height"];
    float paddleSpeed = config["paddle"]["speed"];
    float paddleOffsetY = config["paddle"]["offset_y"];

    int initialLives = config["game"]["initial_lives"];

    // 初始化小球
    m_ball = new Ball(
        m_screenWidth / 2.0f,
        m_screenHeight / 2.0f,
        ballRadius,
        ballSpeedX,
        ballSpeedY
    );

    // 初始化挡板
    m_paddle = new Paddle(
        m_screenWidth / 2.0f,
        m_screenHeight - paddleOffsetY,
        paddleWidth,
        paddleHeight,
        paddleSpeed
    );

    // 初始化砖块（请替换为你自己的 CreateBricks 实现）
    m_bricks = CreateBricks(m_screenWidth);

    // 初始生命
    m_lives = initialLives;
}

// 游戏主循环
void Game::Run() {
    while (!WindowShouldClose()) {
        HandleStateTransition(); // 先处理状态转换
        Update();                // 再更新游戏逻辑
        Draw();                  // 最后绘制
    }
}

// 状态转换处理（核心状态机逻辑）
void Game::HandleStateTransition() {
    switch (m_currentState) {
        case GameState::MENU:
            if (IsKeyPressed(KEY_SPACE)) {
                m_currentState = GameState::PLAYING;
            }
            if (IsKeyPressed(KEY_L)) {
                m_currentState = GameState::LEADERBOARD;
            }
            break;

        case GameState::PLAYING:
            if (IsKeyPressed(KEY_P)) {
                m_currentState = GameState::PAUSED;
            } else if (m_lives <= 0) {
                m_currentState = GameState::GAMEOVER;
            } else if (CheckAllBricksDestroyed()) {
                m_currentState = GameState::VICTORY;
            }
            break;

        case GameState::PAUSED:
            if (IsKeyPressed(KEY_P)) {
                m_currentState = GameState::PLAYING;
            }
            break;

        case GameState::GAMEOVER:
        case GameState::VICTORY:
            if (IsKeyPressed(KEY_SPACE)) {
                Reset();
                m_currentState = GameState::MENU;
            }
            break;

        case GameState::LEADERBOARD:
            if (IsKeyPressed(KEY_L)) {
                m_currentState = GameState::MENU;
            }
            break;
    }
}

// 更新游戏逻辑（仅 PLAYING 状态执行）
void Game::Update() {
    if (m_currentState != GameState::PLAYING) {
        return;
    }

    // 更新小球和挡板
    m_ball->Update();
    m_paddle->Update(m_screenWidth);
    m_ball->CheckBoundaryCollision(m_screenWidth, m_screenHeight);

    // 小球掉落扣命
    if (m_ball->pos.y + m_ball->radius >= m_screenHeight) {
        m_lives--;
        if (m_lives > 0) {
            m_ball->Reset(m_screenWidth, m_screenHeight);
        }
    }

    // 球与挡板碰撞
    if (CheckCollisionCircleRec(m_ball->pos, m_ball->radius, m_paddle->GetRect()) && m_ball->vel.y > 0) {
        m_ball->vel.y *= -1;
        m_ball->vel.x = (m_ball->pos.x - m_paddle->pos.x) * 0.1f;
    }

    // 球与砖块碰撞（加分）
    CheckBallBrickCollision(*m_ball, m_bricks, m_score);
}

// 绘制所有元素（按状态分支）
void Game::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    switch (m_currentState) {
        case GameState::MENU:{
            // 菜单文字居中绘制
            const char* titleText = "BRICK BREAKER";
            int titleFontSize = 40;
            int titleWidth = MeasureText(titleText, titleFontSize);
            DrawText(titleText, (m_screenWidth - titleWidth)/2, m_screenHeight/2 - 80, titleFontSize, BLUE);

            const char* startText = "Press SPACE to Start";
            int startFontSize = 30;
            int startWidth = MeasureText(startText, startFontSize);
            DrawText(startText, (m_screenWidth - startWidth)/2, m_screenHeight/2, startFontSize, GRAY);

            const char* leaderText = "Press L for Leaderboard";
            int leaderFontSize = 20;
            int leaderWidth = MeasureText(leaderText, leaderFontSize);
            DrawText(leaderText, (m_screenWidth - leaderWidth)/2, m_screenHeight/2 + 50, leaderFontSize, GRAY);
            }break;

        case GameState::PLAYING:
            // 绘制游戏元素
            DrawCircleV(m_ball->pos, m_ball->radius, RED);
            DrawRectangleRec(m_paddle->GetRect(), BLUE);
            for (const auto& brick : m_bricks) {
                if (brick.alive) DrawRectangleRec(brick.rect, brick.color);
            }
            // 绘制UI
            DrawText("Press P to Pause", 10, 10, 20, GRAY);
            DrawText(TextFormat("Lives: %d", m_lives), m_screenWidth - 100, 10, 20, RED);
            DrawText(TextFormat("Score: %d", m_score), m_screenWidth - 100, 40, 20, GOLD);
            break;

        case GameState::PAUSED:{
            // PAUSED 状态示例
            const char* pauseText = "PAUSED";
            int pauseFontSize = 40;
            int pauseWidth = MeasureText(pauseText, pauseFontSize);
            DrawText(pauseText, (m_screenWidth - pauseWidth)/2, m_screenHeight/2, pauseFontSize, ORANGE);

            const char* resumeText = "Press P to Resume";
            int resumeFontSize = 20;
            int resumeWidth = MeasureText(resumeText, resumeFontSize);
            DrawText(resumeText, (m_screenWidth - resumeWidth)/2, m_screenHeight/2 + 50, resumeFontSize, GRAY);
            }break;

        case GameState::GAMEOVER:
            DrawText("GAME OVER", m_screenWidth/2 - 80, m_screenHeight/2 - 40, 40, RED);
            DrawText(TextFormat("Final Score: %d", m_score), m_screenWidth/2 - 100, m_screenHeight/2 + 20, 30, GRAY);
            DrawText("Press SPACE to Menu", m_screenWidth/2 - 140, m_screenHeight/2 + 70, 20, GRAY);
            break;

        case GameState::VICTORY:
            DrawText("VICTORY!", m_screenWidth/2 - 80, m_screenHeight/2 - 40, 40, GREEN);
            DrawText(TextFormat("Final Score: %d", m_score), m_screenWidth/2 - 100, m_screenHeight/2 + 20, 30, GRAY);
            DrawText("Press SPACE to Menu", m_screenWidth/2 - 140, m_screenHeight/2 + 70, 20, GRAY);
            break;

        case GameState::LEADERBOARD:
            DrawText("LEADERBOARD", m_screenWidth/2 - 100, m_screenHeight/2 - 80, 40, BLUE);
            DrawText("1. Player A: 1000", m_screenWidth/2 - 100, m_screenHeight/2, 30, GRAY);
            DrawText("2. Player B: 800", m_screenWidth/2 - 100, m_screenHeight/2 + 40, 30, GRAY);
            DrawText("Press L to Return", m_screenWidth/2 - 100, m_screenHeight/2 + 100, 20, GRAY);
            break;
    }

    EndDrawing();
}

// 检查所有砖块是否被击碎
bool Game::CheckAllBricksDestroyed() {
    for (const auto& brick : m_bricks) {
        if (brick.alive) return false;
    }
    return true;
}

// 重置游戏
void Game::Reset() {
    m_lives = config["game"]["initial_lives"];
    m_score = 0;
    m_bricks.clear();

    // 从配置读取参数
    float brickW = config["brick"]["width"];
    float brickH = config["brick"]["height"];
    float brickSpace = config["brick"]["spacing"];
    int brickCount = config["brick"]["count"];
    int scoreGold = config["game"]["score_gold"];

    // 重新初始化砖块（和 Init() 保持一致）
    for (int i = 0; i < brickCount; ++i) {
        m_bricks.emplace_back(
            60.0f + i * (brickW + brickSpace),
            60.0f,
            brickW, brickH,
            GOLD, scoreGold
        );
    }

    m_ball->Reset(m_screenWidth, m_screenHeight);
}