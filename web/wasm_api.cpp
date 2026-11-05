// 网页版的"桥"：把 A 的迷宫生成（maze_generator.cpp）和 B 的迷宫求解（maze_solver.cpp）
// 原封不动地编译成 WebAssembly，再用几个 C 风格的导出函数交给网页里的 JavaScript 调用。
// 网页只负责画图和交互，迷宫怎么生成、怎么求解，全部由这两份 C++ 源码完成。
//
// 数据交换格式（都是 int 数组，放在 WebAssembly 线性内存里，JS 直接按指针读写）：
//   墙：每格一个 int，按位表示四面墙，1=上 2=右 4=下 8=左（有墙为 1）
//   拆墙步骤：每步 3 个 int：row, col, direction（0上 1右 2下 3左）
//   坐标序列：每个点 2 个 int：row, col
#include "maze_generator.h"
#include "maze_solver.h"

#include <cstdlib>
#include <vector>

#define EXPORT(name) extern "C" __attribute__((export_name(name)))

namespace
{
    MazeGrid g_maze;                 // 当前要求解的迷宫
    std::vector<int> g_walls;        // 生成结果：每格的墙
    std::vector<int> g_steps;        // 生成结果：拆墙顺序
    std::vector<int> g_input;        // JS 写入的迷宫（求解前载入）
    std::vector<int> g_path, g_visited, g_parents;

    int encode(const Cell& c)
    {
        return (c.wallTop ? 1 : 0) | (c.wallRight ? 2 : 0) | (c.wallBottom ? 4 : 0) | (c.wallLeft ? 8 : 0);
    }

    void flatten(const std::vector<Point>& pts, std::vector<int>& out)
    {
        out.clear();
        out.reserve(pts.size() * 2);
        for (const Point& p : pts) { out.push_back(p.row); out.push_back(p.col); }
    }
}

// 生成迷宫：algo 0=Kruskal 1=Prim。成功返回拆墙步数，之后用 mz_walls / mz_steps 取结果
EXPORT("mz_generate") int mz_generate(int width, int height, int algo, unsigned seed, double loopRate)
{
    std::vector<WallBreak> steps;
    g_maze = generateMazeWithSteps(width, height, algo == 1 ? GenAlgorithm::Prim : GenAlgorithm::Kruskal,
                                   steps, seed, loopRate);
    g_walls.clear();
    for (const auto& row : g_maze)
        for (const Cell& c : row) g_walls.push_back(encode(c));
    g_steps.clear();
    for (const WallBreak& s : steps)
    {
        g_steps.push_back(s.from.row);
        g_steps.push_back(s.from.col);
        g_steps.push_back(static_cast<int>(s.direction));
    }
    return static_cast<int>(steps.size());
}
EXPORT("mz_walls") int* mz_walls() { return g_walls.data(); }
EXPORT("mz_steps") int* mz_steps() { return g_steps.data(); }

// 载入 JS 端的迷宫：先调用 mz_input 拿到缓冲区写入墙，再调用 mz_use_input
EXPORT("mz_input") int* mz_input(int width, int height)
{
    g_input.assign(static_cast<std::size_t>(width) * height, 15);
    g_maze.assign(height, std::vector<Cell>(width));
    return g_input.data();
}
EXPORT("mz_use_input") void mz_use_input()
{
    const int height = static_cast<int>(g_maze.size());
    const int width = height > 0 ? static_cast<int>(g_maze[0].size()) : 0;
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c)
        {
            const int w = g_input[static_cast<std::size_t>(r) * width + c];
            Cell& cell = g_maze[r][c];
            cell.wallTop = w & 1; cell.wallRight = w & 2; cell.wallBottom = w & 4; cell.wallLeft = w & 8;
        }
}

// 求解当前迷宫：algo 与 SolveAlgorithm 枚举一致（0=BFS 1=DFS递归 2=DFS非递归 3=BFS递归）。
// 返回访问格数；路径/访问顺序/父节点用下面几个函数取
EXPORT("mz_solve") int mz_solve(int algo, int sr, int sc, int er, int ec)
{
    std::vector<Point> path, visited, parents;
    solveMaze(g_maze, Point{ sr, sc }, Point{ er, ec }, static_cast<SolveAlgorithm>(algo), path, visited, parents);
    flatten(path, g_path);
    flatten(visited, g_visited);
    flatten(parents, g_parents);
    return static_cast<int>(visited.size());
}
EXPORT("mz_path_len") int mz_path_len() { return static_cast<int>(g_path.size() / 2); }
EXPORT("mz_path") int* mz_path() { return g_path.data(); }
EXPORT("mz_visited") int* mz_visited() { return g_visited.data(); }
EXPORT("mz_parents") int* mz_parents() { return g_parents.data(); }

// 浏览器里没有 C++ 异常运行时。网页在调用前已经检查过参数，正常不会抛异常；
// 万一抛出，就直接中止这次调用，JS 端会捕获到 RuntimeError 并提示。
extern "C" void* __cxa_allocate_exception(std::size_t n) { return std::malloc(n); }
extern "C" void __cxa_free_exception(void* p) { std::free(p); }
extern "C" [[noreturn]] void __cxa_throw(void*, void*, void (*)(void*)) { __builtin_trap(); }
