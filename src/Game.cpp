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
    , m_menuOption(0)      // 初始化菜单选项
    , m_selectedLevel(1)   // 初始化关卡选择
{
    m_score = 0;
    m_lives = 3;
    m_currentLevel = 1;
    m_hasSave = false;
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

    // 1. 先尝试读档
    bool loadSuccess = LoadSaveGame();
    if (loadSuccess) {
        // 直接加载存档，不弹窗
        TraceLog(LOG_INFO, "检测到存档，自动继续上次游戏：关卡%d，分数%d，生命%d",
                 m_currentLevel, m_score, m_lives);
        LoadLevelFromJSON(m_levelFiles[m_currentLevel - 1]);
        // 删掉旧存档，防止每次启动都自动加载
        std::remove("savegame.json");
    } else {
        // 无存档，直接开始第一关
        m_score = 0;
        m_lives = 3;
        m_currentLevel = 1;
        LoadLevelFromJSON(m_levelFiles[0]);
    }

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

    // 初始化粒子对象池（全部设为未激活）
    for (int i = 0; i < MAX_PARTICLES; i++) {
        m_particlePool[i].active = false;
        m_particlePool[i].life = 0;
    }

    InitBrickGrid();
}

void Game::Run() {
    while (!WindowShouldClose()) {
        HandleStateTransition();
        Update();
        Draw();
    }
}

bool Game::LoadLevelFromJSON(const std::string& filename) {
    m_bricks.clear();

    std::ifstream file(filename);
    if (!file.is_open()) {
        TraceLog(LOG_WARNING, "无法打开关卡文件：%s，使用默认布局", filename.c_str());
        // 默认布局（10列×5行，保留原红橙金规则）
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 10; x++) {
                Color color = RED;
                int score = 10;
                if (y == 0) { color = GOLD; score = 50; }    // 第1行金色50分
                else if (y == 1) { color = ORANGE; score = 20; } // 第2行橙色20分
                m_bricks.push_back(Brick(
                    (float)x * 60,
                    (float)y * 20,
                    60, 20,
                    color, score
                ));
            }
        }
        InitBrickGrid();
        return false;
    }

    try {
        json j;
        file >> j;

        int width = j["width"];
        int height = j["height"];
        float brickW = j["brickWidth"];
        float brickH = j["brickHeight"];
        auto layout = j["layout"];

        // 计算起始X坐标（居中）
        float startX = (m_screenWidth - (width * brickW)) / 2;
        float startY = 50; // 顶部间距

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (layout[y][x] == 1) {
                    // 根据行数定义颜色和分数
                    Color brickColor = RED;
                    int brickScore = 10;
                    if (y == 0) { // 第1行：金色50分
                        brickColor = GOLD;
                        brickScore = 50;
                    } else if (y == 1 || y == 5 || y == 7) { // 第2/6/7行：橙色20分（适配level2/3）
                        brickColor = ORANGE;
                        brickScore = 20;
                    } // 其余行：红色10分（默认）

                    m_bricks.push_back(Brick(
                        startX + (float)x * brickW,
                        startY + (float)y * brickH,
                        brickW, brickH,
                        brickColor, brickScore
                    ));
                }
            }
        }
        InitBrickGrid();
        return true;
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "JSON解析错误：%s，使用默认布局", e.what());
        // 解析失败时用默认红橙金布局
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 10; x++) {
                Color color = RED;
                int score = 10;
                if (y == 0) { color = GOLD; score = 50; }
                else if (y == 1) { color = ORANGE; score = 20; }
                m_bricks.push_back(Brick(
                    (float)x * 60,
                    (float)y * 20,
                    60, 20,
                    color, score
                ));
            }
        }
        InitBrickGrid();
        return false;
    }
}

bool Game::LoadSaveGame() {
    std::ifstream file("savegame.json");
    if (!file.is_open()) {
        m_hasSave = false;
        return false;
    }

    try {
        json j;
        file >> j;
        m_score = j["score"];
        m_lives = j["lives"];
        m_currentLevel = j["level"];
        m_hasSave = true;
        return true;
    } catch (...) {
        m_hasSave = false;
        return false;
    }
}

void Game::SaveGame() {
    json j = {
        {"score", m_score},
        {"lives", m_lives},
        {"level", m_currentLevel}
    };
    std::ofstream file("savegame.json");
    if (file.is_open()) {
        file << j.dump(4);
    }
}

void Game::HandleStateTransition() {
    switch (m_currentState) {
        case GameState::MENU:
            // ✅ 菜单按键检测（1/2/3 + Enter）
            if (IsKeyPressed(KEY_ONE)) m_menuOption = 0;
            if (IsKeyPressed(KEY_TWO)) m_menuOption = 1;
            if (IsKeyPressed(KEY_THREE)) m_menuOption = 2;

            // 关卡选择时左右切换
            if (m_menuOption == 2) {
                if (IsKeyPressed(KEY_RIGHT)) m_selectedLevel++;
                if (IsKeyPressed(KEY_LEFT)) m_selectedLevel--;
                if (m_selectedLevel < 1) m_selectedLevel = 1;
                if (m_selectedLevel > 3) m_selectedLevel = 3;
            }

            // 按Enter确认选择
            if (IsKeyPressed(KEY_ENTER)) {
                if (m_menuOption == 0) {
                    // 1. 新游戏
                    m_score = 0;
                    m_lives = 3;
                    m_currentLevel = 1;
                    std::remove("savegame.json");
                    LoadLevelFromJSON(m_levelFiles[0]);
                    m_currentState = GameState::PLAYING;
                }
                else if (m_menuOption == 1) {
                    // 2. 继续游戏
                    if (LoadSaveGame()) {
                        LoadLevelFromJSON(m_levelFiles[m_currentLevel - 1]);
                        m_currentState = GameState::PLAYING;
                    }
                }
                else if (m_menuOption == 2) {
                    // 3. 选择关卡
                    m_currentLevel = m_selectedLevel;
                    m_score = 0;
                    m_lives = 3;
                    LoadLevelFromJSON(m_levelFiles[m_currentLevel - 1]);
                    m_currentState = GameState::PLAYING;
                }
            }
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
/*void Game::SpawnBrickParticles(Vector2 pos, Color color) {
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
}*/
//对象池版SpawnBrickParticles 
void Game::SpawnBrickParticles(Vector2 pos, Color color)
{
    // 从对象池找空闲粒子，而不是新建
    for (int i = 0; i < 6; i++) {  // 一次生成6个
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (!m_particlePool[p].active) {
                // 拿到一个空闲粒子
                m_particlePool[p].pos = pos;
                m_particlePool[p].vel = {
                    (float)(rand() % 100 - 50) * 0.02f,
                    (float)(rand() % 100 - 50) * 0.02f
                };
                m_particlePool[p].color = color;
                m_particlePool[p].life = 1.0f;
                m_particlePool[p].maxLife = 1.0f;
                m_particlePool[p].active = true;
                break;
            }
        }
    }
}

void Game::Update() {
    if (m_currentState != GameState::PLAYING) return;
    //✅ 游戏中按E退出（保留在这里）
    if (IsKeyPressed(KEY_E)) {
        SaveGame();
        m_currentState = GameState::MENU;
        return;
    }
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
/*
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
*/  
    // ====================== 【对象池优化版】更新粒子 ======================
for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!m_particlePool[i].active) continue;

    // 保持你原来的物理效果：速度 + 重力
    m_particlePool[i].pos.x += m_particlePool[i].vel.x * dt;
    m_particlePool[i].pos.y += m_particlePool[i].vel.y * dt;
    m_particlePool[i].vel.y += 200 * dt; // 重力完全保留！

    // 生命周期递减
    m_particlePool[i].life -= dt;

    // 生命周期结束 -> 不删除，只是回收
    if (m_particlePool[i].life <= 0) {
        m_particlePool[i].active = false;
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

     // 检测当前关卡是否通关（所有砖块死亡）
    bool allBricksCleared = true;
for (auto& brick : m_bricks)
{
    if (brick.alive)
    {
        allBricksCleared = false;
        break;
    }
}

if (allBricksCleared)
{
    m_currentLevel++;

    if ((size_t)m_currentLevel > m_levelFiles.size())
    {
        TraceLog(LOG_INFO, "恭喜！全部关卡通关！");
        Close();
        return;
    }

    LoadLevelFromJSON(m_levelFiles[m_currentLevel - 1]);

    m_ball->pos = { m_screenWidth / 2.0f, m_screenHeight - 50.0f };
    m_ball->vel = { 0, -300 };
    m_extraBalls.clear();
}
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // 帧率显示（固定位置，不遮挡）
    DrawText(TextFormat("FPS: %.1f", 1.0f / GetFrameTime()), 10, 10, 20, BLUE);

    switch (m_currentState) {
        case GameState::MENU: {
            // 标题
            const char* t = "BRICK BREAKER";
            int w = MeasureText(t, 40);
            DrawText(t, (m_screenWidth-w)/2, 120, 40, BLUE);

            // 菜单选项（干净不重叠）
            const char* options[] = {
                "1. New Game",
                "2. Continue",
                "3. Select Level"
            };
            for (int i = 0; i < 3; i++) {
                int textY = 220 + i * 45;
                DrawText(options[i],
                    (m_screenWidth - MeasureText(options[i], 25))/2,
                    textY,
                    25,
                    (m_menuOption == i) ? GREEN : DARKGRAY);
            }

            // 选择关卡提示
            if (m_menuOption == 2) {
                char levelText[32];
                sprintf(levelText, "Level: %d (<-/-> to change)", m_selectedLevel);
                DrawText(levelText,
                    (m_screenWidth - MeasureText(levelText, 20))/2,
                    380,
                    20,
                    GRAY);
            }

            // 操作提示
            DrawText("USE 1/2/3 + ENTER to confirm",
                (m_screenWidth - MeasureText("USE 1/2/3 + ENTER to confirm", 20))/2,
                450,
                20,
                GRAY);
            DrawText("Press L for Leaderboard",
                (m_screenWidth - MeasureText("Press L for Leaderboard", 20))/2,
                500,
                20,
                GRAY);
            break;
        }

        case GameState::PLAYING: {
            // 粒子
            for (int i = 0; i < MAX_PARTICLES; i++) {
                if (!m_particlePool[i].active) continue;
                float alpha = m_particlePool[i].life / m_particlePool[i].maxLife;
                DrawCircleV(m_particlePool[i].pos, 3, ColorAlpha(m_particlePool[i].color, alpha));
            }

            // 游戏物体
            DrawCircleV(m_ball->pos, m_ball->radius, RED);
            DrawRectangleRec(m_paddle->GetRect(), BLUE);
            // 修改后（增加白色边框）
            for (auto& b : m_bricks) {
                if (b.alive) {
                    DrawRectangleRec(b.rect, b.color); // 绘制砖块主体
                    // 绘制1像素白色边框（向内缩1像素避免覆盖）
                    DrawRectangleLinesEx(b.rect, 1, WHITE);
                }
            }
            for (auto& ball : m_extraBalls) if (ball.alive) DrawCircleV(ball.pos, ball.radius, RED);
            for (auto& pu : m_powerUps) pu->Draw();

            // ========== 左侧 UI（完全不重叠） ==========
            DrawText("Press P = Pause", 10, 40, 20, DARKGRAY);
            DrawText("Press E = Save & Exit", 10, 70, 20, DARKGRAY);
            DrawText(TextFormat("Level: %d", m_currentLevel), 10, 100, 20, BLUE);

            // 道具状态
            if (m_paddleExtendTimer > 0)
                DrawText(TextFormat("Paddle+: %.1fs", m_paddleExtendTimer), 10, 130, 20, BLUE);
            if (m_ballSlowTimer > 0)
                DrawText(TextFormat("Slow: %.1fs", m_ballSlowTimer), 10, 160, 20, YELLOW);
            if (!m_extraBalls.empty())
                DrawText("Multi Ball Active", 10, 190, 20, GREEN);

            // ========== 右侧 UI ==========
            DrawText(TextFormat("Lives: %d", m_lives), m_screenWidth - 120, 10, 20, RED);
            DrawText(TextFormat("Score: %d", m_score), m_screenWidth - 120, 40, 20, YELLOW);
            break;
        }

        case GameState::PAUSED: {
            DrawText("PAUSED", (m_screenWidth-MeasureText("PAUSED",40))/2, 200, 40, ORANGE);
            DrawText("Press P to Resume", (m_screenWidth-MeasureText("Press P to Resume",20))/2, 270, 20, GRAY);
            break;
        }

        case GameState::GAMEOVER:
            DrawText("GAME OVER", (m_screenWidth-MeasureText("GAME OVER",40))/2, 200, 40, RED);
            DrawText(TextFormat("Score: %d", m_score), (m_screenWidth-MeasureText(TextFormat("Score: %d", m_score),30))/2, 270, 30, DARKGRAY);
            DrawText("Press SPACE to Menu", (m_screenWidth-MeasureText("Press SPACE to Menu",20))/2, 330, 20, GRAY);
            break;

        case GameState::VICTORY:
            DrawText("VICTORY!", (m_screenWidth-MeasureText("VICTORY!",40))/2, 200, 40, GREEN);
            DrawText(TextFormat("Score: %d", m_score), (m_screenWidth-MeasureText(TextFormat("Score: %d", m_score),30))/2, 270, 30, DARKGRAY);
            DrawText("Press SPACE to Menu", (m_screenWidth-MeasureText("Press SPACE to Menu",20))/2, 330, 20, GRAY);
            break;

        case GameState::LEADERBOARD:
            DrawText("LEADERBOARD", (m_screenWidth-MeasureText("LEADERBOARD",40))/2, 120, 40, BLUE);
            DrawText("1. Player: 1000", (m_screenWidth-MeasureText("1. Player: 1000",30))/2, 220, 30, DARKGRAY);
            DrawText("2. Player: 800", (m_screenWidth-MeasureText("2. Player: 800",30))/2, 260, 30, DARKGRAY);
            DrawText("Press L to Return", (m_screenWidth-MeasureText("Press L to Return",20))/2, 350, 20, GRAY);
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

void Game::Close()
{
    // 退出时自动保存游戏进度
    SaveGame();
}