#include "../include/Game.h"
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <memory>

using json = nlohmann::json;
json config;

void Game::LoadConfig() {
    // 加载主配置
    std::ifstream file("config.json");
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open config.json");
        return;
    }
    file >> config;
    file.close();

    // 加载道具配置表（独立文件，和新建的 powerup_config.json 对应）
    std::ifstream powerUpFile("powerup_config.json");
    if (!powerUpFile.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open powerup_config.json");
        return;
    }
    powerUpFile >> powerUpConfig;
    powerUpFile.close();
}

Game::Game(int screenWidth, int screenHeight, const char* title)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_title(title)
    , m_ball(nullptr)
    , m_paddle(nullptr)
    , m_lives(3)
    , m_score(0)
    , m_currentState(GameState::MENU)
    , m_rng(std::random_device{}())
    , m_paddleExtendTimer(0.0f)
    , m_ballSlowTimer(0.0f)
{
    LoadConfig();
    InitWindow(m_screenWidth, m_screenHeight, m_title);
    SetTargetFPS(60);
    Init();
}

Game::~Game() {
    delete m_ball;
    delete m_paddle;
    CloseWindow();
}

void Game::Init() {
    float ballRadius = config["ball"]["radius"];
    float ballSpeedX = config["ball"]["speed_x"];
    float ballSpeedY = config["ball"]["speed_y"];

    float paddleWidth = config["paddle"]["width"];
    float paddleHeight = config["paddle"]["height"];
    float paddleSpeed = config["paddle"]["speed"];
    float paddleOffsetY = config["paddle"]["offset_y"];
    int initialLives = config["game"]["initial_lives"];

    // 初始化原始参数
    m_paddleOriginalWidth = paddleWidth;
    m_ballOriginalSpeedX = ballSpeedX;
    m_ballOriginalSpeedY = ballSpeedY;

    m_ball = new Ball(
        m_screenWidth / 2.0f,
        m_screenHeight / 2.0f,
        ballRadius,
        ballSpeedX,
        ballSpeedY
    );

    m_paddle = new Paddle(
        m_screenWidth / 2.0f,
        m_screenHeight - paddleOffsetY,
        paddleWidth,
        paddleHeight,
        paddleSpeed
    );

    m_bricks = CreateBricks(m_screenWidth);
    m_lives = initialLives;

    m_powerUps.clear();
    m_extraBalls.clear();
    m_particles.clear();

    InitBrickGrid();
}

void Game::Run() {
    while (!WindowShouldClose()) {
        HandleStateTransition();
        Update();
        Draw();
    }
}

void Game::HandleStateTransition() {
    switch (m_currentState) {
        case GameState::MENU:
            if (IsKeyPressed(KEY_SPACE)) m_currentState = GameState::PLAYING;
            if (IsKeyPressed(KEY_L)) m_currentState = GameState::LEADERBOARD;
            break;
        case GameState::PLAYING:
            if (IsKeyPressed(KEY_P)) m_currentState = GameState::PAUSED;
            else if (m_lives <= 0) m_currentState = GameState::GAMEOVER;
            else if (CheckAllBricksDestroyed()) m_currentState = GameState::VICTORY;
            break;
        case GameState::PAUSED:
            if (IsKeyPressed(KEY_P)) m_currentState = GameState::PLAYING;
            break;
        case GameState::GAMEOVER:
        case GameState::VICTORY:
            if (IsKeyPressed(KEY_SPACE)) {
                Reset();
                m_currentState = GameState::MENU;
            }
            break;
        case GameState::LEADERBOARD:
            if (IsKeyPressed(KEY_L)) m_currentState = GameState::MENU;
            break;
    }
}

// 生成砖块破碎粒子
void Game::SpawnBrickParticles(Vector2 pos, Color color) {
    std::uniform_real_distribution<float> angleDist(0, 2 * PI);
    std::uniform_real_distribution<float> speedDist(50, 150);
    std::uniform_real_distribution<float> lifeDist(0.5f, 1.0f);

    for (int i = 0; i < 8; i++) {
        float angle = angleDist(m_rng);
        float speed = speedDist(m_rng);
        Particle p;
        p.pos = pos;
        p.vel = {cosf(angle) * speed, sinf(angle) * speed};
        p.color = color;
        p.life = lifeDist(m_rng);
        p.maxLife = p.life;
        m_particles.push_back(p);
    }
}

void Game::Update() {
    if (m_currentState != GameState::PLAYING) return;

    float dt = GetFrameTime();

    // 更新小球和挡板
    m_ball->Update();
    m_paddle->Update(m_screenWidth, dt);
    m_ball->CheckBoundaryCollision(m_screenWidth, m_screenHeight);

    // 小球掉落扣命
    if (m_ball->pos.y + m_ball->radius >= m_screenHeight) {
        m_lives--;
        if (m_lives > 0) {
            m_ball->Reset(m_screenWidth, m_screenHeight);
            m_extraBalls.clear();
        }
    }

    // 球与挡板碰撞
    if (CheckCollisionCircleRec(m_ball->pos, m_ball->radius, m_paddle->GetRect()) && m_ball->vel.y > 0) {
        m_ball->vel.y *= -1;
        m_ball->vel.x = (m_ball->pos.x - m_paddle->pos.x) * 0.1f;
    }

    // 球与砖块碰撞（加分 + 粒子特效）初版
    /*
    for (auto& brick : m_bricks) {
        if (!brick.alive) continue;
        if (CheckCollisionCircleRec(m_ball->pos, m_ball->radius, brick.rect)) {
            // 小球反弹
            if (m_ball->vel.y < 0) m_ball->vel.y *= -1;
            brick.alive = false;
            m_score += brick.score;
            // 生成砖块破碎粒子
            SpawnBrickParticles({brick.rect.x + brick.rect.width/2, brick.rect.y + brick.rect.height/2}, brick.color);
        }
    }
    */
    // ======================
// 优化版：网格碰撞检测
// ======================
int ballCol = m_ball->pos.x / (m_screenWidth / GRID_COLS);
int ballRow = m_ball->pos.y / (m_screenHeight / GRID_ROWS);
ballCol = std::clamp(ballCol, 0, GRID_COLS - 1);
ballRow = std::clamp(ballRow, 0, GRID_ROWS - 1);

bool hitBrick = false;
for (int dc = -1; dc <= 1; ++dc) {
    for (int dr = -1; dr <= 1; ++dr) {
        int c = ballCol + dc;
        int r = ballRow + dr;
        if (c < 0 || c >= GRID_COLS || r < 0 || r >= GRID_ROWS) continue;

        for (Brick* brick : m_brickGrid[c][r]) {
            if (!brick->alive) continue;
            if (CheckCollisionCircleRec(m_ball->pos, m_ball->radius, brick->rect)) {
                m_ball->vel.y *= -1;
                brick->alive = false;
                m_score += brick->score;
                SpawnBrickParticles({ brick->rect.x + brick->rect.width / 2, brick->rect.y + brick->rect.height / 2 }, brick->color);
                hitBrick = true;
                break;
            }
        }
        if (hitBrick) break;
    }
}

    // ====================== 随机道具生成（从JSON读取概率） ======================
    static std::vector<bool> spawned(m_bricks.size(), false);
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> typeDist(0, 2);

    float dropChance = powerUpConfig["drop_chance"];
    for (size_t i = 0; i < m_bricks.size(); ++i) {
        if (!m_bricks[i].alive && !spawned[i] && chanceDist(m_rng) < dropChance) {
            spawned[i] = true;
            Vector2 pos = {
                m_bricks[i].rect.x + m_bricks[i].rect.width / 2,
                m_bricks[i].rect.y
            };
            PowerUpType type = static_cast<PowerUpType>(typeDist(m_rng));
            m_powerUps.emplace_back(PowerUp::Create(type, pos));
        }
    }

    // ====================== 道具效果倒计时 ======================
    if (m_paddleExtendTimer > 0) {
        m_paddleExtendTimer -= dt;
        if (m_paddleExtendTimer <= 0) ResetPaddleSize();
    }
    if (m_ballSlowTimer > 0) {
        m_ballSlowTimer -= dt;
        if (m_ballSlowTimer <= 0) ResetBallSpeed();
    }

    // ====================== 更新道具 ======================
    for (auto it = m_powerUps.begin(); it != m_powerUps.end();) {
        (*it)->Update(dt);
        bool hit = (*it)->CheckPaddleCollision(m_paddle->GetRect());

        if (hit) {
            (*it)->Activate(*this);
            it = m_powerUps.erase(it);
        } else if (!(*it)->active) {
            it = m_powerUps.erase(it);
        } else {
            ++it;
        }
    }

    // ====================== 更新额外球 ======================
    /*
    for (auto& ball : m_extraBalls) {
        ball.Update();
        ball.CheckBoundaryCollision(m_screenWidth, m_screenHeight);
        if (CheckCollisionCircleRec(ball.pos, ball.radius, m_paddle->GetRect()) && ball.vel.y > 0) {
            ball.vel.y *= -1;
            ball.vel.x = (ball.pos.x - m_paddle->pos.x) * 0.1f;
        }
        // 额外球与砖块碰撞
        for (auto& brick : m_bricks) {
            if (!brick.alive) continue;
            if (CheckCollisionCircleRec(ball.pos, ball.radius, brick.rect)) {
                if (ball.vel.y < 0) ball.vel.y *= -1;
                brick.alive = false;
                m_score += brick.score;
                SpawnBrickParticles({brick.rect.x + brick.rect.width/2, brick.rect.y + brick.rect.height/2}, brick.color);
            }
        }
        if (ball.pos.y + ball.radius >= m_screenHeight) ball.alive = false;
    }
    */
// 额外球优化碰撞
for (auto& ball : m_extraBalls) {
    if (!ball.alive) continue;

    // ====================== 【必须加！让球移动】 ======================
    ball.Update();
    ball.CheckBoundaryCollision(m_screenWidth, m_screenHeight);
    // ================================================================

    // ========== 补上：额外球 碰挡板反弹 ==========
    if (CheckCollisionCircleRec(ball.pos, ball.radius, m_paddle->GetRect()))
    {
        ball.vel.y *= -1;
        ball.vel.x = (ball.pos.x - m_paddle->pos.x) * 0.1f;
    }

    int bCol = ball.pos.x / (m_screenWidth / GRID_COLS);
    int bRow = ball.pos.y / (m_screenHeight / GRID_ROWS);
    bCol = std::clamp(bCol, 0, GRID_COLS - 1);
    bRow = std::clamp(bRow, 0, GRID_ROWS - 1);

    bool hit = false;
    for (int dc = -1; dc <= 1; ++dc) {
        for (int dr = -1; dr <= 1; ++dr) {
            int c = bCol + dc;
            int r = bRow + dr;
            if (c < 0 || c >= GRID_COLS || r < 0 || r >= GRID_ROWS) continue;

            for (Brick* brick : m_brickGrid[c][r]) {
                if (!brick->alive) continue;
                if (CheckCollisionCircleRec(ball.pos, ball.radius, brick->rect)) {
                    ball.vel.y *= -1;
                    brick->alive = false;
                    m_score += brick->score;
                    SpawnBrickParticles({ brick->rect.x + brick->rect.width / 2, brick->rect.y + brick->rect.height / 2 }, brick->color);
                    hit = true;
                    break;
                }
            }
            if (hit) break;
        }
    }

    // 额外球出界 → 死亡
    if (ball.pos.y + ball.radius >= m_screenHeight || ball.pos.y - ball.radius <= 0) {
        ball.alive = false;
    }
}

// 移除死亡的额外球
m_extraBalls.erase(
    std::remove_if(m_extraBalls.begin(), m_extraBalls.end(),
        [](const Ball& ball) {
            return !ball.alive;
        }
    ),
    m_extraBalls.end()
);

    // ====================== 更新粒子 ======================
    for (auto it = m_particles.begin(); it != m_particles.end();) {
        it->pos.x += it->vel.x * dt;
        it->pos.y += it->vel.y * dt;
        it->vel.y += 200 * dt; // 重力
        it->life -= dt;
        if (it->life <= 0) {
            it = m_particles.erase(it);
        } else {
            ++it;
        }
    }

    // ====================== 多球生命值处理 ======================
    bool allBallsDropped = (m_ball->GetPosition().y + m_ball->GetRadius() >= m_screenHeight);
    for (auto& ball : m_extraBalls) {
        if (ball.GetPosition().y + ball.GetRadius() < m_screenHeight) {
            allBallsDropped = false;
            break;
        }
    }

    if (allBallsDropped) {
        m_lives--;
        if (m_lives > 0) {
            m_ball->Reset(m_screenWidth, m_screenHeight);
            m_extraBalls.clear();
        } else {
            m_currentState = GameState::GAMEOVER;
        }
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    //检测帧率
    DrawText(TextFormat("FPS: %.1f", 1.0f / GetFrameTime()), 10, 30, 20, BLUE);

    switch (m_currentState) {
        case GameState::MENU: {
            const char* t = "BRICK BREAKER";
            int w = MeasureText(t, 40);
            DrawText(t, (m_screenWidth-w)/2, m_screenHeight/2-80, 40, BLUE);
            const char* s = "Press SPACE to Start";
            w = MeasureText(s,30);
            DrawText(s,(m_screenWidth-w)/2,m_screenHeight/2,30,GRAY);
            const char* l = "Press L for Leaderboard";
            w = MeasureText(l,20);
            DrawText(l,(m_screenWidth-w)/2,m_screenHeight/2+50,20,GRAY);
            break;
        }
        case GameState::PLAYING: {
            // 绘制粒子
            for (auto& p : m_particles) {
                float alpha = p.life / p.maxLife;
                DrawCircleV(p.pos, 3, ColorAlpha(p.color, alpha));
            }

            DrawCircleV(m_ball->pos, m_ball->radius, RED);
            DrawRectangleRec(m_paddle->GetRect(), BLUE);
            for (auto& b : m_bricks) if (b.alive) DrawRectangleRec(b.rect, b.color);

            // 绘制额外球
            for (auto& ball : m_extraBalls) {
                if (ball.alive) DrawCircleV(ball.pos, ball.radius, RED);
            }

            // 绘制道具
            for (auto& pu : m_powerUps) {
                pu->Draw();
            }

            // UI
            DrawText("Press P to Pause",10,10,20,GRAY);
            DrawText(TextFormat("Lives: %d",m_lives),m_screenWidth-100,10,20,RED);
            DrawText(TextFormat("Score: %d",m_score),m_screenWidth-100,40,20,GOLD);

            // 道具状态显示
            if (m_paddleExtendTimer > 0) {
                DrawText(TextFormat("PADDLE+: %.1fs", m_paddleExtendTimer), 10, 40, 20, BLUE);
            }
            if (m_ballSlowTimer > 0) {
                DrawText(TextFormat("SLOW: %.1fs", m_ballSlowTimer), 10, 70, 20, YELLOW);
            }
            if (!m_extraBalls.empty()) {
                DrawText("MULTI BALL ACTIVE", 10, 100, 20, GREEN);
            }
            break;
        }
        case GameState::PAUSED: {
            const char* t = "PAUSED";
            int w = MeasureText(t,40);
            DrawText(t,(m_screenWidth-w)/2,m_screenHeight/2,40,ORANGE);
            const char* s = "Press P to Resume";
            w = MeasureText(s,20);
            DrawText(s,(m_screenWidth-w)/2,m_screenHeight/2+50,20,GRAY);
            break;
        }
        case GameState::GAMEOVER:
            DrawText("GAME OVER",m_screenWidth/2-80,m_screenHeight/2-40,40,RED);
            DrawText(TextFormat("Final Score: %d",m_score),m_screenWidth/2-100,m_screenHeight/2+20,30,GRAY);
            DrawText("Press SPACE to Menu",m_screenWidth/2-140,m_screenHeight/2+70,20,GRAY);
            break;
        case GameState::VICTORY:
            DrawText("VICTORY!",m_screenWidth/2-80,m_screenHeight/2-40,40,GREEN);
            DrawText(TextFormat("Final Score: %d",m_score),m_screenWidth/2-100,m_screenHeight/2+20,30,GRAY);
            DrawText("Press SPACE to Menu",m_screenWidth/2-140,m_screenHeight/2+70,20,GRAY);
            break;
        case GameState::LEADERBOARD:
            DrawText("LEADERBOARD",m_screenWidth/2-100,m_screenHeight/2-80,40,BLUE);
            DrawText("1. Player A: 1000",m_screenWidth/2-100,m_screenHeight/2,30,GRAY);
            DrawText("2. Player B: 800",m_screenWidth/2-100,m_screenHeight/2+40,30,GRAY);
            DrawText("Press L to Return",m_screenWidth/2-100,m_screenHeight/2+100,20,GRAY);
            break;
    }
    EndDrawing();
}

bool Game::CheckAllBricksDestroyed() {
    for (auto& b : m_bricks) if (b.alive) return false;
    return true;
}

void Game::Reset() {
    m_lives = config["game"]["initial_lives"];
    m_score = 0;
    m_bricks.clear();
    m_powerUps.clear();
    m_extraBalls.clear();
    m_particles.clear();
    m_paddleExtendTimer = 0;
    m_ballSlowTimer = 0;

    float bw = config["brick"]["width"];
    float bh = config["brick"]["height"];
    float sp = config["brick"]["spacing"];
    int cnt = config["brick"]["count"];
    int sc = config["game"]["score_gold"];

    for (int i=0;i<cnt;++i) {
        m_bricks.emplace_back(60+i*(bw+sp),60,bw,bh,GOLD,sc);
    }

    m_ball->Reset(m_screenWidth,m_screenHeight);
    ResetPaddleSize();
    ResetBallSpeed();

    InitBrickGrid(); 
}

// 道具效果实现
void Game::ExtendPaddle(float scale, float duration) {
    m_paddle->SetWidth(m_paddleOriginalWidth * scale);
    m_paddleExtendTimer = duration;
}

void Game::ResetPaddleSize() {
    m_paddle->SetWidth(m_paddleOriginalWidth);
}

void Game::SpawnMultiBall() {
    if (m_extraBalls.empty()) {
        Ball newBall = *m_ball;
        newBall.vel.x *= -1;
        m_extraBalls.push_back(newBall);
    }
}

void Game::SlowBall(float scale, float duration) {
    m_ball->vel.x *= scale;
    m_ball->vel.y *= scale;
    for (auto& ball : m_extraBalls) {
        ball.vel.x *= scale;
        ball.vel.y *= scale;
    }
    m_ballSlowTimer = duration;
}

void Game::ResetBallSpeed() {
    m_ball->vel.x = m_ballOriginalSpeedX;
    m_ball->vel.y = m_ballOriginalSpeedY;
    for (auto& ball : m_extraBalls) {
        ball.vel.x = m_ballOriginalSpeedX;
        ball.vel.y = m_ballOriginalSpeedY;
    }
}

Rectangle Game::GetPaddleRect() {
    return m_paddle->GetRect();
}

int Game::GetScreenHeight() {
    return m_screenHeight;
}

void Game::InitBrickGrid() {
    // 清空网格
    for (int c = 0; c < Game::GRID_COLS; c++)
        for (int r = 0; r < Game::GRID_ROWS; r++)
            m_brickGrid[c][r].clear();

    // 把砖块按位置放进对应网格
    for (auto& brick : m_bricks) {
        int col = brick.rect.x / (m_screenWidth / GRID_COLS);
        int row = brick.rect.y / (m_screenHeight / GRID_ROWS);

        col = std::clamp(col, 0, GRID_COLS - 1);
        row = std::clamp(row, 0, GRID_ROWS - 1);

        m_brickGrid[col][row].push_back(&brick);
    }
}