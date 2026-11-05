// 性能测试：生成算法(Kruskal / Prim)与求解算法(BFS / BFS递归 / DFS递归 / DFS非递归)的耗时统计
// 用法：perf_test.exe [输出csv路径]    默认输出到当前目录的 perf_results.csv
// 一次运行产生两个文件(放在同一个文件夹)：
//   perf_results.csv  规模实验：10x10 ~ 500x500 的生成/求解耗时、访问格数、路径长度(完美迷宫)
//   loop_results.csv  环路实验：不同环路比例(loopRate)下 BFS 与 DFS 的路径长度、访问格数
// 注意：必须用 Release 模式编译，Debug 数据没有参考价值（Debug 下运行会打印警告）。
//
// 测量方法：
//  - 固定随机种子：第 trial 个迷宫使用种子 1000 + trial，Kruskal 与 Prim 共用同一组种子，结果可复现
//  - 每个计时点先空跑一次预热（不计入结果）
//  - 小迷宫单次耗时只有几微秒，所以连续重复 reps 次，CSV 里的 time_us 是单次平均耗时
//  - 求解在 Kruskal 生成的迷宫上进行，起点左上角，终点右下角
//  - 递归版求解(DFS_Recursive、BFS_Recursive)在边长超过 recursiveMaxSide 时跳过，避免栈溢出
//  - 环路实验只用两个非递归算法(BFS 队列、DFS 显式栈)，因此可以放心测 200x200
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

// 迷宫里打通的相邻格对数（通路数）。完美迷宫恰好是 格子数-1；多出来的就是环路数
static long countOpenPassages(const MazeGrid& m) {
    long e = 0;
    for (size_t r = 0; r < m.size(); ++r)
        for (size_t c = 0; c < m[r].size(); ++c) {
            if (c + 1 < m[r].size() && !m[r][c].wallRight) ++e;
            if (r + 1 < m.size() && !m[r][c].wallBottom) ++e;
        }
    return e;
}

// ---------------------------------------------------------------------------
// 环路实验：同一个迷宫上比较 BFS 与 DFS 的路径长度和访问格数
// ---------------------------------------------------------------------------
static bool runLoopExperiment(const std::string& path, unsigned baseSeed, std::uint64_t& sink) {
    const std::vector<int> sizes = { 50, 100, 200 };
    // loopRate = 在完美迷宫的基础上，额外拆除"剩余内部墙"的比例
    const std::vector<double> rates = { 0.0, 0.01, 0.02, 0.05, 0.1, 0.2, 0.3, 0.5 };
    const int trials = 20;

    std::ofstream csv(path);
    if (!csv) {
        std::cerr << "Cannot open output file: " << path << "\n";
        return false;
    }
    csv << std::fixed << std::setprecision(3);
    // loops：额外打通的通路数(= 通路数 - (格子数-1))，即迷宫里独立环路的个数
    // shortest_len：BFS 找到的路径长度，BFS 保证最短，用作对照
    csv << "algo,width,height,loop_rate,trial,seed,loops,time_us,visited,path_len,shortest_len,reps\n";

    for (int side : sizes) {
        const int reps = repsFor(side);
        std::cout << "Loop experiment " << side << "x" << side << " (" << rates.size() << " loop rates x "
            << trials << " seeds) ..." << std::endl;
        for (double rate : rates) {
            for (int trial = 0; trial < trials; ++trial) {
                const unsigned seed = baseSeed + static_cast<unsigned>(trial);
                MazeGrid maze = generateMaze(side, side, GenAlgorithm::Kruskal, seed, rate);
                const long loops = countOpenPassages(maze) - (static_cast<long>(side) * side - 1);
                Point start{ 0, 0 };
                Point end{ side - 1, side - 1 };

                std::vector<Point> bp, bv, bpar;
                solveMaze(maze, start, end, SolveAlgorithm::BFS, bp, bv, bpar);
                const size_t shortest = bp.size();

                for (SolveAlgorithm s : {SolveAlgorithm::BFS, SolveAlgorithm::DFS_Iterative}) {
                    std::vector<Point> p, v, par;
                    solveMaze(maze, start, end, s, p, v, par); // 预热一次，不计时
                    auto t0 = Clock::now();
                    for (int r = 0; r < reps; ++r) {
                        std::vector<Point> p2, v2, par2;
                        solveMaze(maze, start, end, s, p2, v2, par2);
                        sink += p2.size() + v2.size();
                    }
                    auto t1 = Clock::now();
                    csv << solveName(s) << "," << side << "," << side << "," << rate << "," << trial << ","
                        << seed << "," << loops << "," << elapsedUs(t0, t1) / reps << "," << v.size() << ","
                        << p.size() << "," << shortest << "," << reps << "\n";
                }
            }
        }
    }
    return true;
}

int main(int argc, char** argv) {
    const std::string csvPath = (argc > 1) ? argv[1] : "perf_results.csv";
    const std::string loopPath =
        (std::filesystem::path(csvPath).parent_path() / "loop_results.csv").string();

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

    // ---- 环路实验 ----
    if (!runLoopExperiment(loopPath, baseSeed, sink)) return 1;

    std::cout << "Done.\n  Scale results: " << std::filesystem::absolute(csvPath).string()
        << "\n  Loop results:  " << std::filesystem::absolute(loopPath).string() << "\n";
    std::cout << "(checksum " << sink << ", ignore)\n";
    return 0;
}
