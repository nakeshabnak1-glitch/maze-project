#ifndef MAZE_TYPES_H
#define MAZE_TYPES_H

#include <vector>

// 公共坐标：row 为行号，col 为列号，均从 0 开始。
struct Point
{
    int row;
    int col;
};

enum class Direction
{
    Up,
    Right,
    Down,
    Left
};

// true 表示有墙；格子初始四面均有墙。
struct Cell
{
    bool wallTop{true};
    bool wallRight{true};
    bool wallBottom{true};
    bool wallLeft{true};
};

// maze[row][col]：height 行、width 列。
using MazeGrid = std::vector<std::vector<Cell>>;

// 从 from 格子向 direction 拆墙，同时打开相邻格子的对应墙。
struct WallBreak
{
    Point from;
    Direction direction;
};

#endif
