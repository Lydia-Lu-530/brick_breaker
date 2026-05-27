#include "../include/Game.h"
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include "raymath.h"

using json = nlohmann::json;
json config;

// ----------- 新增：屏幕震动 + 飘字 + 音效 -----------
Camera2D gameCamera = {0};
float shakeTime = 0.0f;
float shakeMagnitude = 0.0f;

struct ScorePopup {
    Vector2 pos;
    int value;
    float timer;
    float alpha;
};
std::vector<ScorePopup> scorePopups;

Sound brickHitSound;
// --------------------------------------------------------

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
    , m_currentState(GameState::INSTRUCTIONS) // 改这里：启动先弹窗
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

    m_ballAttached = true;   // 一开始球粘在板子上

    // ---- 新增：相机、音频、震动初始化 ----
    InitAudioDevice(); // 开启音频
    brickHitSound = LoadSound("hit.wav"); // 音效文件放exe同目录
    gameCamera.target = {0, 0};
    gameCamera.offset = {0, 0};
    gameCamera.rotation = 0.0f;
    gameCamera.zoom = 1.0f;
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

        // 恢复砖块状态
        if (!m_savedBrickStates.empty() && m_savedBrickStates.size() == m_bricks.size()) {
            for (size_t i = 0; i < m_bricks.size(); i++) {
                m_bricks[i].alive = m_savedBrickStates[i];
            }
            m_savedBrickStates.clear();
        }

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

        // ==============================
        // 【已加好】恢复砖块的存活状态
        // ==============================
        if (!m_savedBrickStates.empty() && m_savedBrickStates.size() == m_bricks.size()) {
            for (size_t i = 0; i < m_bricks.size(); i++){
                m_bricks[i].alive = m_savedBrickStates[i];
            }
            m_savedBrickStates.clear();
        }

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

        // 恢复砖块状态
        if (!m_savedBrickStates.empty() && m_savedBrickStates.size() == m_bricks.size()) {
            for (size_t i = 0; i < m_bricks.size(); i++) {
                m_bricks[i].alive = m_savedBrickStates[i];
            }
            m_savedBrickStates.clear();
        }

        return false;
    }
}
//读档
bool Game::LoadSaveGame() {
    std::ifstream file("savegame.json");
    if (!file.is_open()) {
        m_hasSave = false;
        return false;
    }

    try {
        json j;
        file >> j;
        file.close();

        m_score = j["score"];
        m_lives = j["lives"];
        m_currentLevel = j["level"];
        m_hasSave = true;

        // 👇 新增：把砖块状态也读到成员变量里
        if (j.contains("bricks")) {
            m_savedBrickStates = j["bricks"].get<std::vector<bool>>();
        }

        return true;
    } catch (...) {
        m_hasSave = false;
        return false;
    }
}

//存档
void Game::SaveGame() {
    json j;
    j["score"] = m_score;
    j["lives"] = m_lives;
    j["level"] = m_currentLevel;

    // 👇 新增：保存所有砖块的存活状态
    std::vector<bool> brickStates;
    for (auto& brick : m_bricks) {
        brickStates.push_back(brick.alive);
    }
    j["bricks"] = brickStates;

    std::ofstream file("savegame.json");
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

void Game::HandleStateTransition() {
    switch (m_currentState) {
        case GameState::INSTRUCTIONS:
            // 按任意键进入菜单
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                m_currentState = GameState::MENU;
            }
            break;
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
                    // 👇 强制重置 + 粘板
                    m_ball->Reset(m_screenWidth, m_screenHeight);
                    m_ballAttached = true;
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
                    // 👇 强制重置球 + 粘在板子上
                    m_ball->Reset(m_screenWidth, m_screenHeight);
                    m_ballAttached = true;
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


void StartScreenShake(float magnitude, float duration)
{
    shakeMagnitude = magnitude;
    shakeTime = duration;
}

void UpdateScreenShake()
{
    if (shakeTime > 0.0f)
    {
        gameCamera.offset.x = GetRandomValue(-shakeMagnitude, shakeMagnitude);
        gameCamera.offset.y = GetRandomValue(-shakeMagnitude, shakeMagnitude);
        shakeTime -= GetFrameTime();
    }
    else
    {
        gameCamera.offset = {0, 0};
        shakeMagnitude = 0.0f;
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

    // 更新挡板
    m_paddle->Update(m_screenWidth, dt);

    // ==========================
    // 【粘在挡板上 + 自由移动 + 空格发射】
    // ==========================
    if (m_ballAttached)
    {
        // 球永远跟着挡板中心
        m_ball->pos.x = m_paddle->pos.x;
        m_ball->pos.y = m_paddle->pos.y - m_ball->radius - 5.0f;

        // 按空格发射
        if (IsKeyPressed(KEY_SPACE))
        {
            float angle = 0.0f;
             // 改成 A / D 键控制角度，不影响挡板移动！
            if (IsKeyDown(KEY_A)) angle = -35.0f;   // A = 向左斜射
            if (IsKeyDown(KEY_D)) angle = 35.0f;    // D = 向右斜射

            float rad = angle * DEG2RAD;
            m_ball->vel.x = sinf(rad) * m_ballOriginalSpeedX;
            m_ball->vel.y = -cosf(rad) * fabsf(m_ballOriginalSpeedY);

            m_ballAttached = false;
        }
    }
    else
    {
        // 正常移动
        m_ball->Update();
        m_ball->CheckBoundaryCollision(m_screenWidth, m_screenHeight);

    }

    // 球与挡板碰撞
    if (CheckCollisionCircleRec(m_ball->pos, m_ball->radius, m_paddle->GetRect()) && m_ball->vel.y > 0) {
        m_ball->vel.y *= -1;
        m_ball->vel.x = (m_ball->pos.x - m_paddle->pos.x) * 0.1f;
    }

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
                // ---- 新增：音效 + 震动 + 飘字 ----
                PlaySound(brickHitSound);
                StartScreenShake(5.0f, 0.1f); // 震动幅度、时长

                // 分数飘字
                scorePopups.push_back({
                { brick->rect.x + brick->rect.width/2, brick->rect.y },
                brick->score,
                0.6f,
                1.0f
                });
                // ------------------------------------
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

                    // ✅ 额外球也加音效、震动、飘字
                    PlaySound(brickHitSound);
                    StartScreenShake(3.0f, 0.07f);

                    scorePopups.push_back({
                        { brick->rect.x + brick->rect.width/2, brick->rect.y },
                        brick->score,
                        0.6f,
                        1.0f
                    });

                    SpawnBrickParticles({
                    brick->rect.x + brick->rect.width / 2,
                        brick->rect.y + brick->rect.height / 2
                    }, brick->color);

                    hit = true;
                    break;
                }
            }
            if (hit) break;
        }
    }

    // 额外球出界 → 死亡
    if (ball.pos.y + ball.radius >= m_screenHeight) {
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

    // ====================== 多球生命值处理（修复版） ======================
    bool mainBallDropped = (m_ball->pos.y + m_ball->radius >= m_screenHeight);
    bool anyExtraAlive = false;

    for (auto& ball : m_extraBalls) {
        if (ball.alive) {
            anyExtraAlive = true;
            break;
        }
    }

    // 只有【主球掉了 + 没有任何额外球活着】才扣命
    if (mainBallDropped && !anyExtraAlive)
    {
        m_lives--;
        if (m_lives > 0)
        {
            m_ball->Reset(m_screenWidth, m_screenHeight);
            m_extraBalls.clear();
            m_ballAttached = true;   // ✅ 重新粘板
        }
        else
        {
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
            TraceLog(LOG_INFO, "Congratulations！");
            Close();
            return;
        }

        LoadLevelFromJSON(m_levelFiles[m_currentLevel - 1]);

        m_ballAttached = true;  // ✅ 通关后粘住
        m_extraBalls.clear();
    }
    // ---- 新增：更新屏幕震动 ----
    UpdateScreenShake();
    // ---- 新增：更新分数飘字 ----
    for (size_t i = 0; i < scorePopups.size(); )
    {
        ScorePopup& pop = scorePopups[i];
        pop.pos.y -= 60 * dt; // 向上飘
        pop.alpha -= 1.0f / pop.timer * dt; // 渐变消失
        pop.timer -= dt;

        if (pop.timer <= 0.0f || pop.alpha <= 0.0f)
        {
            scorePopups.erase(scorePopups.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    // ---- 新增：开启相机（震动在这里生效） ----
    BeginMode2D(gameCamera);
    
    // 帧率显示（固定位置，不遮挡）
    DrawText(TextFormat("FPS: %.1f", 1.0f / GetFrameTime()), 10, 10, 20, BLUE);
    //状态切换
    switch (m_currentState) {
        case GameState::INSTRUCTIONS: {
            ClearBackground(RAYWHITE);

            // 半透明黑色遮罩
            DrawRectangle(0, 0, m_screenWidth, m_screenHeight, Fade(BLACK, 0.8f));

            // 白色弹窗背景
            int boxW = 520;
            int boxH = 360;
            int boxX = (m_screenWidth - boxW) / 2;
            int boxY = (m_screenHeight - boxH) / 2;
            DrawRectangle(boxX, boxY, boxW, boxH, RAYWHITE);
            DrawRectangleLinesEx({(float)boxX, (float)boxY, (float)boxW, (float)boxH}, 3, BLUE);

            // 标题
            DrawText("HOW TO PLAY", boxX + 140, boxY + 30, 32, BLUE);

            // 操作说明（英文）
            DrawText("A / D         - Aim ball before launch", boxX + 40, boxY + 90, 22, DARKGRAY);
            DrawText("LEFT/RIGHT    - Move paddle", boxX + 40, boxY + 125, 22, DARKGRAY);
            DrawText("SPACE        - Launch ball / Confirm", boxX + 40, boxY + 160, 22, DARKGRAY);
            DrawText("P            - Pause game", boxX + 40, boxY + 195, 22, DARKGRAY);
            DrawText("E            - Save & Exit to menu", boxX + 40, boxY + 230, 22, DARKGRAY);
            DrawText("L            - Leaderboard", boxX + 40, boxY + 265, 22, DARKGRAY);

            // 底部提示
            DrawText("PRESS ENTER OR SPACE TO CONTINUE", boxX + 50, boxY + 310, 20, GREEN);
            break;
        }
        //菜单
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
        //游戏中
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

            // ========== 左侧 UI ==========
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
        //暂停
        case GameState::PAUSED: {
            DrawText("PAUSED", (m_screenWidth-MeasureText("PAUSED",40))/2, 200, 40, ORANGE);
            DrawText("Press P to Resume", (m_screenWidth-MeasureText("Press P to Resume",20))/2, 270, 20, GRAY);
            break;
        }
        //游戏结束
        case GameState::GAMEOVER:
            DrawText("GAME OVER", (m_screenWidth-MeasureText("GAME OVER",40))/2, 200, 40, RED);
            DrawText(TextFormat("Score: %d", m_score), (m_screenWidth-MeasureText(TextFormat("Score: %d", m_score),30))/2, 270, 30, DARKGRAY);
            DrawText("Press SPACE to Menu", (m_screenWidth-MeasureText("Press SPACE to Menu",20))/2, 330, 20, GRAY);
            break;
        //成功通关
        case GameState::VICTORY:
            DrawText("VICTORY!", (m_screenWidth-MeasureText("VICTORY!",40))/2, 200, 40, GREEN);
            DrawText(TextFormat("Score: %d", m_score), (m_screenWidth-MeasureText(TextFormat("Score: %d", m_score),30))/2, 270, 30, DARKGRAY);
            DrawText("Press SPACE to Menu", (m_screenWidth-MeasureText("Press SPACE to Menu",20))/2, 330, 20, GRAY);
            break;
        //排行榜
        case GameState::LEADERBOARD:
            DrawText("LEADERBOARD", (m_screenWidth-MeasureText("LEADERBOARD",40))/2, 120, 40, BLUE);
            DrawText("1. Player: 1000", (m_screenWidth-MeasureText("1. Player: 1000",30))/2, 220, 30, DARKGRAY);
            DrawText("2. Player: 800", (m_screenWidth-MeasureText("2. Player: 800",30))/2, 260, 30, DARKGRAY);
            DrawText("Press L to Return", (m_screenWidth-MeasureText("Press L to Return",20))/2, 350, 20, GRAY);
            break;
    }
    EndMode2D(); // 结束相机

    // ---- 新增：绘制分数飘字（在UI层，不跟着震） ----
    for (const auto& pop : scorePopups)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d", pop.value);
        DrawText(buf, pop.pos.x, pop.pos.y, 20, Fade(YELLOW, pop.alpha));
    }

    EndDrawing();
}
// 检查所有砖块是否被销毁（用于判断是否通关）
bool Game::CheckAllBricksDestroyed() {
    for (auto& b : m_bricks) if (b.alive) return false;
    return true;
}
// 重置游戏：分数、生命、关卡、道具状态全部恢复默认
void Game::Reset() {
    m_lives = config["game"]["initial_lives"];// 从配置文件读取初始生命值并恢复
    m_score = 0;// 分数清零
    // 清空所有游戏对象容器
    m_bricks.clear();// 清空砖块
    m_powerUps.clear();// 清空道具
    m_extraBalls.clear();// 清空额外球
    m_particles.clear();// 清空粒子特效
    // 重置所有道具计时器（关闭所有道具效果）
    m_paddleExtendTimer = 0;// 挡板延长效果归零
    m_ballSlowTimer = 0;// 球减速效果归零
    // 从JSON配置读取砖块参数
    float bw = config["brick"]["width"];// 砖块宽度
    float bh = config["brick"]["height"];// 砖块高度
    float sp = config["brick"]["spacing"];// 砖块间距
    int cnt = config["brick"]["count"];// 砖块数量
    int sc = config["game"]["score_gold"];// 金色砖块分数
    // 创建一排金色初始砖块
    for (int i=0;i<cnt;++i) {
        m_bricks.emplace_back(60+i*(bw+sp),60,bw,bh,GOLD,sc);
    }

    m_ball->Reset(m_screenWidth,m_screenHeight);// 重置球的位置与状态
    ResetPaddleSize();// 重置挡板尺寸
    ResetBallSpeed();// 重置球的移动速度

    InitBrickGrid();// 初始化砖块网格（用于碰撞检测优化）
}

//  ==================== 道具效果实现 ====================
// 功能：延长挡板长度
// 参数：scale-缩放比例  duration-效果持续时间
void Game::ExtendPaddle(float scale, float duration) {
    // 设置挡板宽度为原始宽度的scale倍
    m_paddle->SetWidth(m_paddleOriginalWidth * scale);
    // 设置道具效果计时器
    m_paddleExtendTimer = duration;
}

// 功能：重置挡板为默认大小（道具效果结束时调用）
void Game::ResetPaddleSize() {
    m_paddle->SetWidth(m_paddleOriginalWidth);
}

// 功能：生成额外的球（多球道具）
void Game::SpawnMultiBall() {
    // 限制：同一时间最多 1 个额外球（防止太多）
    int aliveExtra = 0;
    for (auto& b : m_extraBalls) {
        if (b.alive) aliveExtra++;
    }

    // 只有【没有存活的额外球】时才生成
    if (aliveExtra == 0) {
        Ball newBall = *m_ball;
        newBall.vel.x *= -1;
        newBall.alive = true;
        m_extraBalls.push_back(newBall);
    }
}

// 功能：减慢所有球的速度（减速道具）
// 参数：scale-速度缩放比例（小于1减速） duration-持续时间
void Game::SlowBall(float scale, float duration) {
    // 减慢主球速度
    m_ball->vel.x *= scale;
    m_ball->vel.y *= scale;
    // 遍历并减慢所有额外球的速度
    for (auto& ball : m_extraBalls) {
        ball.vel.x *= scale;
        ball.vel.y *= scale;
    }
    // 设置减速效果计时器
    m_ballSlowTimer = duration;
}

// 功能：重置所有球为原始速度（道具效果结束）
void Game::ResetBallSpeed() {
    // -------- 恢复主球速度 --------
    float len = sqrtf(m_ball->vel.x * m_ball->vel.x + m_ball->vel.y * m_ball->vel.y);
    if (len > 0.001f) {
        // 算出当前方向（单位向量）
        float dirX = m_ball->vel.x / len;
        float dirY = m_ball->vel.y / len;
        
        // 保持方向，只改大小
        m_ball->vel.x = dirX * m_ballOriginalSpeedX;
        m_ball->vel.y = dirY * fabsf(m_ballOriginalSpeedY);
    }

    // -------- 恢复所有额外球速度（同样保持方向） --------
    for (auto& ball : m_extraBalls) {
        float blen = sqrtf(ball.vel.x * ball.vel.x + ball.vel.y * ball.vel.y);
        if (blen > 0.001f) {
            float dirX = ball.vel.x / blen;
            float dirY = ball.vel.y / blen;
            
            ball.vel.x = dirX * m_ballOriginalSpeedX;
            ball.vel.y = dirY * fabsf(m_ballOriginalSpeedY);
        }
    }
}

// 功能：获取挡板的矩形区域（供外部碰撞检测使用）
Rectangle Game::GetPaddleRect() {
    return m_paddle->GetRect();
}

// 功能：获取屏幕高度（工具函数）
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

void Game::Unload()
{
    UnloadSound(brickHitSound);
    CloseAudioDevice();
}

void Game::Close()
{
    // 退出时自动保存游戏进度
    SaveGame();
    Unload();  // 👈 加这一行
}