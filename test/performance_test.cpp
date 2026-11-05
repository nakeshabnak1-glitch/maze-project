// 性能测试：生成算法(Kruskal / Prim)与求解算法(BFS / BFS递归 / DFS递归 / DFS非递归)的耗时统计
// 用法：perf_test.exe [输出csv路径]    默认输出到当前目录的 perf_results.csv
// 注意：必须用 Release 模式编译，Debug 数据没有参考价值（Debug 下运行会打印警告）。
//
// 测量方法：
//  - 固定随机种子：第 trial 个迷宫使用种子 1000 + trial，Kruskal 与 Prim 共用同一组种子，结果可复现
//  - 每个计时点先空跑一次预热（不计入结果）
//  - 小迷宫单次耗时只有几微秒，所以连续重复 reps 次，CSV 里的 time_us 是单次平均耗时
//  - 求解在 Kruskal 生成的迷宫上进行，起点左上角，终点右下角
//  - 递归版求解(DFS_Recursive、BFS_Recursive)在边长超过 recursiveMaxSide 时跳过，避免栈溢出
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
    case SolveAlgorithm::BFS_Recursive: return "BFS_Recursive";
    case SolveAlgorithm::DFS_Recursive: return "DFS_Recursive";
    default: return "DFS_Iterative";
    }
}

static bool isRecursive(SolveAlgorithm a) {
    return a == SolveAlgorithm::DFS_Recursive || a == SolveAlgorithm::BFS_Recursive;
}

// 迷宫越小，重复次数越多（小迷宫单次耗时只有几微秒，单次计时误差太大）
static int repsFor(int side) {
    long cells = static_cast<long>(side) * side;
    long reps = 100000 / cells;
    return static_cast<int>(std::clamp<long>(reps, 1, 300));
}

int main(int argc, char** argv) {
    const std::string csvPath = (argc > 1) ? argv[1] : "perf_results.csv";

    // 迷宫边长梯度（正方形）
    const std::vector<int> sizes = { 10, 25, 50, 100, 200, 300, 500 };
    // 每个规模使用多少个不同种子的迷宫
    const int trials = 20;
    // 种子 = baseSeed + trial
    const unsigned baseSeed = 1000;
    // 递归版求解超过这个边长可能栈溢出崩溃，所以只在小规模测
    const int recursiveMaxSide = 50;

#ifndef NDEBUG
    std::cout << "WARNING: this is NOT a Release build (NDEBUG is not defined).\n"
        << "         Timing results are not valid. Switch to a Release configuration and run again.\n\n";
#endif

    std::ofstream csv(csvPath);
    if (!csv) {
        std::cerr << "Cannot open output file: " << csvPath << "\n";
        return 1;
    }
    csv << std::fixed << std::setprecision(3);
    csv << "phase,algo,width,height,trial,time_us,visited,path_len,seed,reps\n";

    std::uint64_t sink = 0; // 防止编译器把"结果没用到"的计算优化掉

    // 全局预热：让内存分配器、指令缓存先进入稳定状态（不计入结果）
    {
        MazeGrid w = generateMaze(50, 50, GenAlgorithm::Kruskal, baseSeed, 0.0);
        std::vector<Point> p, v, par;
        solveMaze(w, Point{ 0, 0 }, Point{ 49, 49 }, SolveAlgorithm::BFS, p, v, par);
        sink += p.size() + v.size();
    }

    const SolveAlgorithm solvers[] = { SolveAlgorithm::BFS, SolveAlgorithm::BFS_Recursive,
                                      SolveAlgorithm::DFS_Iterative, SolveAlgorithm::DFS_Recursive };

    for (int side : sizes) {
        const int reps = repsFor(side);
        std::cout << "Testing size " << side << "x" << side << " (" << trials << " seeds, " << reps
            << " reps each) ..." << std::endl;

        for (int trial = 0; trial < trials; ++trial) {
            const unsigned seed = baseSeed + static_cast<unsigned>(trial);
            MazeGrid forSolve;

            // ---- 生成：Kruskal 与 Prim 使用同一个种子编号 ----
            for (GenAlgorithm g : {GenAlgorithm::Kruskal, GenAlgorithm::Prim}) {
                MazeGrid maze = generateMaze(side, side, g, seed, 0.0); // 预热一次，不计时
                auto t0 = Clock::now();
                for (int r = 0; r < reps; ++r) {
                    MazeGrid tmp = generateMaze(side, side, g, seed, 0.0);
                    sink += tmp.size();
                }
                auto t1 = Clock::now();
                csv << "gen," << genName(g) << "," << side << "," << side << "," << trial << ","
                    << elapsedUs(t0, t1) / reps << ",,," << seed << "," << reps << "\n";
                if (g == GenAlgorithm::Kruskal) forSolve = std::move(maze);
            }

            // ---- 求解：在同一个 Kruskal 迷宫上，左上角 -> 右下角 ----
            Point start{ 0, 0 };
            Point end{ side - 1, side - 1 };
            for (SolveAlgorithm s : solvers) {
                if (isRecursive(s) && side > recursiveMaxSide) continue;

                std::vector<Point> path, visited, parent;
                solveMaze(forSolve, start, end, s, path, visited, parent); // 预热一次，不计时

                auto t0 = Clock::now();
                for (int r = 0; r < reps; ++r) {
                    std::vector<Point> p, v, par;
                    solveMaze(forSolve, start, end, s, p, v, par);
                    sink += p.size() + v.size();
                }
                auto t1 = Clock::now();
                csv << "solve," << solveName(s) << "," << side << "," << side << "," << trial << ","
                    << elapsedUs(t0, t1) / reps << "," << visited.size() << "," << path.size() << ","
                    << seed << "," << reps << "\n";
            }
        }
    }

    csv.close();
    std::cout << "Done. Results saved to: " << std::filesystem::absolute(csvPath).string() << "\n";
    std::cout << "(checksum " << sink << ", ignore)\n";
    return 0;
}