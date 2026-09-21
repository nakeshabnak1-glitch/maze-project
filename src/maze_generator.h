#ifndef MAZE_GENERATOR_H
#define MAZE_GENERATOR_H

#include <vector>

/**
 * @brief 迷宫单个格子结构体，记录四个方向墙壁状态
 */
struct Cell
{
    bool wallTop{true};
    bool wallRight{true};
    bool wallBottom{true};
    bool wallLeft{true};
};

/// 迷宫网格类型：二维数组，width × height 的格子
using MazeGrid = std::vector<std::vector<Cell>>;

/// 迷宫生成算法枚举
enum class GenAlgorithm
{
    Kruskal,
    Prim
};

/**
 * @brief 生成完美迷宫
 * @param width 横向格子数量
 * @param height 纵向格子数量
 * @param algo 选择 Kruskal / Prim
 * @return MazeGrid 迷宫网格，统一接口输出，给求解模块使用
 */
MazeGrid generateMaze(int width, int height, GenAlgorithm algo);

#endif
