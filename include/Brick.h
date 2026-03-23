#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"
#include <vector>
#include "Ball.h"  // 必须加！让编译器认识 Ball 类型

struct Brick {
    Rectangle rect;   // 碰撞矩形
    bool alive;       // 是否存活
    Color color;      // 颜色
    int score;        // 新增：击碎该砖块的得分


    Brick(float x, float y, float width, float height, Color color, int score);
};

// 正确写法：Ball& 后面加空格，参数名 ball
std::vector<Brick> CreateBricks(int screenWidth);
bool CheckBallBrickCollision(Ball& ball, std::vector<Brick>& bricks, int& score);

#endif // BRICK_H