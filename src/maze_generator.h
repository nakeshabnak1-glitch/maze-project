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


// 指定种子的兼容重载；原三参数入口仍随机生成完美迷宫。
// 相同尺寸、算法、seed、loopRate 在同一标准库实现下得到相同结果。
// loopRate 必须为 [0,1] 内有限数；额外拆除 floor(loopRate * 剩余内部墙数) 面墙。
// 0 保持完美迷宫，1 打开所有内部墙；单行/单列没有剩余内部墙。
MazeGrid generateMaze(int width, int height, GenAlgorithm algo,
                      unsigned seed, double loopRate = 0.0);

// 原四参数入口保留；记录先包含生成树的 N-1 步，再包含额外拆墙。
// 参数错误或生成异常时 outSteps 为空。seed=0 也是有效的固定种子。
MazeGrid generateMazeWithSteps(int width, int height, GenAlgorithm algo,
                               std::vector<WallBreak>& outSteps,
                               unsigned seed, double loopRate = 0.0);
#endif
