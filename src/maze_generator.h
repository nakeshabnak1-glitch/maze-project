#ifndef MAZE_GENERATOR_H
#define MAZE_GENERATOR_H

#include "maze_types.h"

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

/**
 * @brief 返回 height 行、width 列的迷宫，并输出真实拆墙顺序。
 * @param outSteps 替换为本次成功生成的记录，不追加旧记录；异常时为空。
 * @note 与 generateMaze 共用算法。1×1 无记录，其余完美迷宫有 N-1 条记录。
 */
MazeGrid generateMazeWithSteps(int width, int height, GenAlgorithm algo,
                               std::vector<WallBreak>& outSteps);

#endif
