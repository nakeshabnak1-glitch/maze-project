#ifndef MAZE_SOLVER_H
#define MAZE_SOLVER_H

#include "maze_generator.h"
#include <vector>

// 坐标点
struct Point
{
    int row;
    int col;
};

enum class SolveAlgorithm
{
    BFS,
    DFS_Recursive,
    DFS_Iterative
};

/**
 * @brief 迷宫求解
 * @param maze 输入迷宫（来自generateMaze）
 * @param start 起点
 * @param end 终点
 * @param algo BFS / DFS递归 / DFS非递归
 * @param outPath 输出：最终路径坐标序列
 * @param outVisitedOrder 输出：访问节点顺序，给GUI动画回放用
 */
void solveMaze(const MazeGrid& maze,
               Point start,
               Point end,
               SolveAlgorithm algo,
               std::vector<Point>& outPath,
               std::vector<Point>& outVisitedOrder);

#endif
