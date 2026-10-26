// 性能测试：生成算法(Kruskal / Prim)与求解算法(BFS / DFS递归 / DFS非递归)的耗时统计
// 用法：perf_test.exe [输出csv路径]    默认输出到当前目录的 perf_results.csv
// 注意：必须用 Release 模式编译，Debug 数据没有参考价值。
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "maze_generator.h"
#include "maze_solver.h"
#include "maze_types.h"

using Clock = std::chrono::steady_clock;

static double elapsedUs(Clock::time_point t0, Clock::time_point t1) {
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

static const char* genName(GenAlgorithm a) {
    return a == GenAlgorithm::Kruskal ? "Kruskal" : "Prim";
}

static const char* solveName(SolveAlgorithm a) {
    switch (a) {
    case SolveAlgorithm::BFS: return "BFS";
    case SolveAlgorithm::DFS_Recursive: return "DFS_Recursive";
    default: return "DFS_Iterative";
    }
}

int main(int argc, char** argv) {
    const std::string csvPath = (argc > 1) ? argv[1] : "perf_results.csv";

    // 迷宫边长梯度（正方形）
    const std::vector<int> sizes = {10, 25, 50, 100, 200, 300, 500};
    // 每个规模重复生成多少个不同的迷宫
    const int trials = 5;
    // 递归 DFS 超过这个边长可能栈溢出崩溃，所以只在小规模测（与 GUI 的限制一致）
    const int recursiveMaxSide = 50;

    std::ofstream csv(csvPath);
    if (!csv) {
        std::cerr << "无法打开输出文件: " << csvPath << "\n";
        return 1;
    }
    csv << "phase,algo,width,height,trial,time_us,visited,path_len\n";

    std::uint64_t sink = 0; // 防止编译器把"结果没用到"的计算优化掉

    for (int side : sizes) {
        std::cout << "测试规模 " << side << "x" << side << " ..." << std::endl;
        for (int trial = 0; trial < trials; ++trial) {
            MazeGrid forSolve;

            // ---- 生成：Kruskal 与 Prim 各计时一次 ----
            for (GenAlgorithm g : {GenAlgorithm::Kruskal, GenAlgorithm::Prim}) {
                auto t0 = Clock::now();
                MazeGrid maze = generateMaze(side, side, g);
                auto t1 = Clock::now();
                sink += maze.size();
                csv << "gen," << genName(g) << "," << side << "," << side << ","
                    << trial << "," << elapsedUs(t0, t1) << ",,\n";
                if (g == GenAlgorithm::Kruskal) forSolve = std::move(maze);
            }

            // ---- 求解：在同一个 Kruskal 迷宫上，左上角 -> 右下角 ----
            Point start{0, 0};
            Point end{side - 1, side - 1};
            for (SolveAlgorithm s : {SolveAlgorithm::BFS, SolveAlgorithm::DFS_Iterative,
                                     SolveAlgorithm::DFS_Recursive}) {
                if (s == SolveAlgorithm::DFS_Recursive && side > recursiveMaxSide) continue;

                std::vector<Point> path, visited, parent;
                auto t0 = Clock::now();
                solveMaze(forSolve, start, end, s, path, visited, parent);
                auto t1 = Clock::now();
                sink += path.size() + visited.size();
                csv << "solve," << solveName(s) << "," << side << "," << side << ","
                    << trial << "," << elapsedUs(t0, t1) << ","
                    << visited.size() << "," << path.size() << "\n";
            }
        }
    }

    csv.close();
    std::cout << "完成。结果已保存到: "
              << std::filesystem::absolute(csvPath).string() << "\n";
    std::cout << "(校验值 " << sink << "，无需关注)\n";
    return 0;
}
