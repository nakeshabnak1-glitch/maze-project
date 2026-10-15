#ifndef MAZE_SOLVER_H
#define MAZE_SOLVER_H

#include "maze_types.h"

enum class SolveAlgorithm
{
    BFS,
    DFS_Recursive,
    DFS_Iterative
};

// 由 B 实现：函数内部检查当前坐标、目标边界和墙壁。
bool canMove(const MazeGrid& maze, Point from, Direction direction);

/**
 * @brief 迷宫求解
 * @param maze 输入迷宫（来自generateMaze）
 * @param start 起点
 * @param end 终点
 * @param algo BFS / DFS递归 / DFS非递归
 * @param outPath 输出：最终路径坐标序列
 * @param outVisitedOrder 输出：访问节点顺序，给GUI动画回放用
 * @param outParentOf 输出：outParentOf[i] 是 outVisitedOrder[i] 的父节点（即"是从哪个格子走过来的"），与 outVisitedOrder 一一对应，起点没有父节点，固定为 {-1,-1}
 *
 * @throws std::invalid_argument 迷宫为空、各行长度不一致（不是矩形）、
 *         或 start / end 超出迷宫范围时抛出；抛出前三个输出参数已被清空。
 * @note 终点不可达时不抛异常：outPath 为空，outVisitedOrder 为实际访问过的格子。
 * @note DFS_Recursive 与 DFS_Iterative 的访问顺序完全一致（都按 上→右→下→左 尝试），
 *       区别只在于前者用函数调用栈、后者用堆上的显式栈，便于做公平的性能对比。
 */
void solveMaze(const MazeGrid& maze,
               Point start,
               Point end,
               SolveAlgorithm algo,
               std::vector<Point>& outPath,
               std::vector<Point>& outVisitedOrder,
               std::vector<Point>& outParentOf);

#endif
