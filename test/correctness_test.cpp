// 正确性测试：验证迷宫生成与求解模块的行为是否符合设计约定
// 运行：correctness_test.exe     全部通过时最后一行打印 ALL PASSED，退出码 0
// 失败时打印具体哪一项没通过（最多显示前 20 条），退出码 1
//
// 检查内容：
//  A. 完美迷宫性质：边界封闭、相邻格墙一致、拆墙数 = 格子数-1、全部连通（=> 是生成树、无环）
//  B. generateMazeWithSteps 的拆墙记录：条数 = 格子数-1，按顺序回放后与返回的迷宫完全一致
//  C. 求解正确性：路径起终点正确、每一步都不穿墙、访问序列无重复且父节点合法；
//     完美迷宫里所有算法的路径长度 = 独立算出的最短距离
//  D. 逐格一致：DFS_Recursive 与 DFS_Iterative；BFS_Recursive 与 BFS（访问顺序和路径都相同）
//  E. 非法参数：抛出 std::invalid_argument，且求解的三个输出已被清空
//  F. 随机种子：同一种子 + 同一算法 + 同一 loopRate 生成同一个迷宫；不同种子生成不同迷宫
//  G. 带环路迷宫(loopRate > 0)：边界封闭、墙一致、全部连通、含环；
//     BFS 与 BFS_Recursive 的路径一定最短，DFS 的路径不会比 BFS 更短
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "maze_generator.h"
#include "maze_solver.h"
#include "maze_types.h"

static long g_checks = 0;
static long g_fail = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        ++g_checks;                                                   \
        if (!(cond)) {                                                \
            ++g_fail;                                                 \
            if (g_fail <= 20) std::cerr << "FAIL: " << msg << "\n";   \
        }                                                             \
    } while (0)

// 方向顺序与 Direction 枚举一致：上、右、下、左
static const int DR[4] = { -1, 0, 1, 0 };
static const int DC[4] = { 0, 1, 0, -1 };

static bool isOpen(const Cell& c, int d) {
    switch (d) {
    case 0: return !c.wallTop;
    case 1: return !c.wallRight;
    case 2: return !c.wallBottom;
    default: return !c.wallLeft;
    }
}

static void setWall(Cell& c, int d, bool v) {
    switch (d) {
    case 0: c.wallTop = v; break;
    case 1: c.wallRight = v; break;
    case 2: c.wallBottom = v; break;
    default: c.wallLeft = v; break;
    }
}

static bool inBounds(int r, int c, int W, int H) { return r >= 0 && r < H && c >= 0 && c < W; }

static bool sameCell(const Cell& a, const Cell& b) {
    return a.wallTop == b.wallTop && a.wallRight == b.wallRight &&
        a.wallBottom == b.wallBottom && a.wallLeft == b.wallLeft;
}

static bool sameGrid(const MazeGrid& a, const MazeGrid& b) {
    if (a.size() != b.size()) return false;
    for (size_t r = 0; r < a.size(); ++r) {
        if (a[r].size() != b[r].size()) return false;
        for (size_t c = 0; c < a[r].size(); ++c)
            if (!sameCell(a[r][c], b[r][c])) return false;
    }
    return true;
}

static std::string tag(const std::string& what, int W, int H) {
    return what + " [" + std::to_string(W) + "x" + std::to_string(H) + "]";
}

// ---------- A / G. 迷宫结构 ----------
// perfect = true：必须恰好 N-1 条通路（生成树）；false：带环路，通路数 >= N-1，且规模足够大时 > N-1
static void checkMazeStructure(const MazeGrid& m, int W, int H, const std::string& name, bool perfect) {
    CHECK((int)m.size() == H, name << ": row count");
    if ((int)m.size() != H) return;
    for (int r = 0; r < H; ++r) {
        CHECK((int)m[r].size() == W, name << ": row " << r << " width");
        if ((int)m[r].size() != W) return;
    }
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            if (r == 0) CHECK(m[r][c].wallTop, name << ": top boundary open at " << r << "," << c);
            if (r == H - 1) CHECK(m[r][c].wallBottom, name << ": bottom boundary open at " << r << "," << c);
            if (c == 0) CHECK(m[r][c].wallLeft, name << ": left boundary open at " << r << "," << c);
            if (c == W - 1) CHECK(m[r][c].wallRight, name << ": right boundary open at " << r << "," << c);
            if (c + 1 < W)
                CHECK(m[r][c].wallRight == m[r][c + 1].wallLeft,
                    name << ": wall mismatch between " << r << "," << c << " and right neighbor");
            if (r + 1 < H)
                CHECK(m[r][c].wallBottom == m[r + 1][c].wallTop,
                    name << ": wall mismatch between " << r << "," << c << " and lower neighbor");
        }
    long edges = 0;
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            if (c + 1 < W && !m[r][c].wallRight) ++edges;
            if (r + 1 < H && !m[r][c].wallBottom) ++edges;
        }
    long tree = (long)W * H - 1;
    if (perfect)
        CHECK(edges == tree, name << ": open passages " << edges << ", expected " << tree);
    else {
        CHECK(edges >= tree, name << ": open passages " << edges << " < spanning tree size " << tree);
        if ((long)W * H >= 600)
            CHECK(edges > tree, name << ": loopRate > 0 but maze has no loop (open passages " << edges << ")");
    }

    std::vector<char> seen((size_t)W * H, 0);
    std::queue<int> q;
    q.push(0);
    seen[0] = 1;
    long cnt = 1;
    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        int r = cur / W, c = cur % W;
        for (int d = 0; d < 4; ++d) {
            if (!isOpen(m[r][c], d)) continue;
            int nr = r + DR[d], nc = c + DC[d];
            if (!inBounds(nr, nc, W, H)) continue;
            int id = nr * W + nc;
            if (!seen[id]) {
                seen[id] = 1;
                ++cnt;
                q.push(id);
            }
        }
    }
    CHECK(cnt == (long)W * H, name << ": only " << cnt << " of " << (long)W * H << " cells reachable");
}

// ---------- B. 拆墙记录回放 ----------
static long countEdges(const MazeGrid& m, int W, int H) {
    long edges = 0;
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            if (c + 1 < W && !m[r][c].wallRight) ++edges;
            if (r + 1 < H && !m[r][c].wallBottom) ++edges;
        }
    return edges;
}

// loopRate 的约定：先生成树(N-1 条通路)，再额外拆除 floor(loopRate * 剩余内部墙数) 面墙，1 则全部打开
static long expectedEdges(int W, int H, double loopRate) {
    long tree = (long)W * H - 1;
    long internalWalls = (long)(W - 1) * H + (long)(H - 1) * W;
    long remaining = internalWalls - tree;
    long extra = (loopRate == 1.0) ? remaining : (long)std::floor(loopRate * (double)remaining);
    return tree + extra;
}

static void checkSteps(const MazeGrid& m, const std::vector<WallBreak>& steps, int W, int H,
    const std::string& name, long expectedCount) {
    CHECK((long)steps.size() == expectedCount,
        name << ": steps count " << steps.size() << ", expected " << expectedCount);
    MazeGrid g((size_t)H, std::vector<Cell>((size_t)W));
    bool ok = true;
    for (const WallBreak& s : steps) {
        int d = static_cast<int>(s.direction);
        int r = s.from.row, c = s.from.col;
        int nr = r + DR[d], nc = c + DC[d];
        if (d < 0 || d > 3 || !inBounds(r, c, W, H) || !inBounds(nr, nc, W, H)) {
            ok = false;
            break;
        }
        if (isOpen(g[r][c], d)) { // 同一堵墙被拆两次
            ok = false;
            break;
        }
        setWall(g[r][c], d, false);
        setWall(g[nr][nc], (d + 2) % 4, false);
    }
    CHECK(ok, name << ": a step is out of range or breaks an already open wall");
    if (!ok) return;
    CHECK(sameGrid(m, g), name << ": replaying steps does not reproduce the returned maze");
}

// ---------- 独立的最短距离（与被测代码无关） ----------
static int refDist(const MazeGrid& m, int W, int H, Point s, Point e) {
    std::vector<int> dist((size_t)W * H, -1);
    std::queue<int> q;
    dist[s.row * W + s.col] = 0;
    q.push(s.row * W + s.col);
    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        int r = cur / W, c = cur % W;
        if (r == e.row && c == e.col) return dist[cur];
        for (int d = 0; d < 4; ++d) {
            if (!isOpen(m[r][c], d)) continue;
            int nr = r + DR[d], nc = c + DC[d];
            if (!inBounds(nr, nc, W, H)) continue;
            int id = nr * W + nc;
            if (dist[id] < 0) {
                dist[id] = dist[cur] + 1;
                q.push(id);
            }
        }
    }
    return -1;
}

static const char* algoName(SolveAlgorithm a) {
    switch (a) {
    case SolveAlgorithm::BFS: return "BFS";
    case SolveAlgorithm::BFS_Recursive: return "BFS_Recursive";
    case SolveAlgorithm::DFS_Recursive: return "DFS_Recursive";
    default: return "DFS_Iterative";
    }
}

// ---------- C. 求解正确性 ----------
// mustBeShortest：路径长度必须等于最短距离（完美迷宫里所有算法，或带环迷宫里的 BFS 类算法）；
// 否则只要求路径不比最短距离更短
static void checkSolve(const MazeGrid& m, int W, int H, Point s, Point e, SolveAlgorithm algo,
    bool mustBeShortest, std::vector<Point>& pathOut, std::vector<Point>& visitedOut) {
    std::string name = tag(std::string(algoName(algo)) + " " + std::to_string(s.row) + "," +
        std::to_string(s.col) + "->" + std::to_string(e.row) + "," + std::to_string(e.col),
        W, H);
    std::vector<Point> path, visited, parent;
    solveMaze(m, s, e, algo, path, visited, parent);

    int dist = refDist(m, W, H, s, e);
    CHECK(dist >= 0, name << ": reference BFS says unreachable (maze is not connected?)");
    CHECK(!path.empty(), name << ": empty path");
    if (!path.empty()) {
        CHECK(path.front().row == s.row && path.front().col == s.col, name << ": path does not start at start");
        CHECK(path.back().row == e.row && path.back().col == e.col, name << ": path does not end at end");
        bool walkOk = true;
        for (size_t i = 1; i < path.size(); ++i) {
            bool found = false;
            for (int d = 0; d < 4; ++d)
                if (path[i - 1].row + DR[d] == path[i].row && path[i - 1].col + DC[d] == path[i].col &&
                    inBounds(path[i - 1].row, path[i - 1].col, W, H) && isOpen(m[path[i - 1].row][path[i - 1].col], d))
                    found = true;
            if (!found) {
                walkOk = false;
                break;
            }
        }
        CHECK(walkOk, name << ": path passes through a wall or jumps");
        if (dist >= 0) {
            if (mustBeShortest)
                CHECK((int)path.size() == dist + 1, name << ": path length " << path.size() << ", shortest is " << dist + 1);
            else
                CHECK((int)path.size() >= dist + 1, name << ": path length " << path.size() << " is SHORTER than the true shortest " << dist + 1);
        }
    }

    CHECK(visited.size() == parent.size(), name << ": visitedOrder and parentOf sizes differ");
    if (!visited.empty() && visited.size() == parent.size()) {
        CHECK(visited[0].row == s.row && visited[0].col == s.col, name << ": visitedOrder[0] is not start");
        CHECK(parent[0].row == -1 && parent[0].col == -1, name << ": parentOf[0] is not {-1,-1}");
        std::vector<int> idx((size_t)W * H, -1);
        bool uniqueOk = true, parentOk = true, inRange = true;
        for (size_t i = 0; i < visited.size(); ++i) {
            if (!inBounds(visited[i].row, visited[i].col, W, H)) {
                inRange = false;
                break;
            }
            int id = visited[i].row * W + visited[i].col;
            if (idx[id] >= 0) uniqueOk = false;
            idx[id] = (int)i;
            if (i > 0) {
                Point p = parent[i];
                if (!inBounds(p.row, p.col, W, H) || idx[p.row * W + p.col] < 0 || idx[p.row * W + p.col] >= (int)i)
                    parentOk = false;
                else {
                    bool adj = false;
                    for (int d = 0; d < 4; ++d)
                        if (p.row + DR[d] == visited[i].row && p.col + DC[d] == visited[i].col &&
                            isOpen(m[p.row][p.col], d))
                            adj = true;
                    if (!adj) parentOk = false;
                }
            }
        }
        CHECK(inRange, name << ": visited cell out of range");
        CHECK(uniqueOk, name << ": a cell appears twice in visitedOrder");
        CHECK(parentOk, name << ": a parentOf entry is invalid (not visited earlier / not adjacent / wall between)");
        CHECK(idx[e.row * W + e.col] >= 0, name << ": end cell not in visitedOrder");
    }
    pathOut = path;
    visitedOut = visited;
}

static bool samePoints(const std::vector<Point>& a, const std::vector<Point>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].row != b[i].row || a[i].col != b[i].col) return false;
    return true;
}

// ---------- 对一个迷宫做全部求解检查 ----------
// perfect = true：完美迷宫，所有算法路径都必须最短；false：带环路迷宫
static void testSolving(const MazeGrid& m, int W, int H, std::mt19937& rng, bool perfect) {
    if (W * H < 2) return;
    std::vector<std::pair<Point, Point>> pairs;
    pairs.push_back({ Point{0, 0}, Point{H - 1, W - 1} });
    for (int i = 0; i < 4; ++i) {
        Point s{ (int)(rng() % H), (int)(rng() % W) }, e{ (int)(rng() % H), (int)(rng() % W) };
        if (s.row == e.row && s.col == e.col) continue;
        pairs.push_back({ s, e });
    }
    bool doRecursive = (long)W * H <= 2500; // 递归版只在小迷宫上测，避免栈溢出
    std::string where = std::string(perfect ? "perfect" : "loop");
    for (auto& pr : pairs) {
        std::vector<Point> pb, vb, pi, vi, pdr, vdr, pbr, vbr;
        checkSolve(m, W, H, pr.first, pr.second, SolveAlgorithm::BFS, true, pb, vb);
        checkSolve(m, W, H, pr.first, pr.second, SolveAlgorithm::DFS_Iterative, perfect, pi, vi);
        // DFS 找到的路径不会比 BFS 的最短路径更短
        CHECK(pi.size() >= pb.size(), tag("DFS path shorter than BFS path (" + where + ")", W, H));
        if (doRecursive) {
            // D. 逐格一致
            checkSolve(m, W, H, pr.first, pr.second, SolveAlgorithm::DFS_Recursive, perfect, pdr, vdr);
            CHECK(samePoints(vi, vdr), tag("DFS recursive/iterative visitedOrder differ (" + where + ")", W, H));
            CHECK(samePoints(pi, pdr), tag("DFS recursive/iterative path differ (" + where + ")", W, H));
            checkSolve(m, W, H, pr.first, pr.second, SolveAlgorithm::BFS_Recursive, true, pbr, vbr);
            CHECK(samePoints(vb, vbr), tag("BFS vs BFS_Recursive visitedOrder differ (" + where + ")", W, H));
            CHECK(samePoints(pb, pbr), tag("BFS vs BFS_Recursive path differ (" + where + ")", W, H));
        }
    }
}

// ---------- E. 非法参数 ----------
static void testErrors() {
    for (GenAlgorithm g : {GenAlgorithm::Kruskal, GenAlgorithm::Prim}) {
        const int bad[][2] = { {0, 5}, {5, 0}, {0, 0}, {-1, 5}, {5, -3} };
        for (auto& b : bad) {
            bool thrown = false;
            try {
                generateMaze(b[0], b[1], g);
            }
            catch (const std::invalid_argument&) {
                thrown = true;
            }
            catch (...) {
            }
            CHECK(thrown, "generateMaze(" << b[0] << "," << b[1] << ") should throw invalid_argument");
        }
    }
    // loopRate 必须是 [0,1] 内的有限数
    const double badRates[] = { -0.1, 1.5, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity() };
    for (double br : badRates) {
        bool thrown = false;
        try {
            generateMaze(5, 5, GenAlgorithm::Kruskal, 1u, br);
        }
        catch (const std::invalid_argument&) {
            thrown = true;
        }
        catch (...) {
        }
        CHECK(thrown, "generateMaze with loopRate=" << br << " should throw invalid_argument");
    }
    MazeGrid m = generateMaze(5, 5, GenAlgorithm::Kruskal);
    struct Case { Point s, e; const char* what; };
    Case cases[] = { {Point{-1, 0}, Point{4, 4}, "start row -1"},
                    {Point{0, 5}, Point{4, 4}, "start col out of range"},
                    {Point{0, 0}, Point{5, 0}, "end row out of range"},
                    {Point{0, 0}, Point{0, -1}, "end col -1"} };
    for (SolveAlgorithm a : {SolveAlgorithm::BFS, SolveAlgorithm::BFS_Recursive, SolveAlgorithm::DFS_Iterative,
        SolveAlgorithm::DFS_Recursive})
        for (auto& cs : cases) {
            std::vector<Point> path{ {9, 9} }, visited{ {9, 9} }, parent{ {9, 9} };
            bool thrown = false;
            try {
                solveMaze(m, cs.s, cs.e, a, path, visited, parent);
            }
            catch (const std::invalid_argument&) {
                thrown = true;
            }
            catch (...) {
            }
            CHECK(thrown, algoName(a) << ": " << cs.what << " should throw invalid_argument");
            CHECK(path.empty() && visited.empty() && parent.empty(),
                algoName(a) << ": " << cs.what << " outputs not cleared before throw");
        }
    MazeGrid empty;
    std::vector<Point> p, v, par;
    bool thrown = false;
    try {
        solveMaze(empty, Point{ 0, 0 }, Point{ 0, 0 }, SolveAlgorithm::BFS, p, v, par);
    }
    catch (const std::invalid_argument&) {
        thrown = true;
    }
    catch (...) {
    }
    CHECK(thrown, "solveMaze on empty maze should throw invalid_argument");
}

// ---------- F. 随机种子 ----------
static void testSeeds() {
    for (GenAlgorithm g : {GenAlgorithm::Kruskal, GenAlgorithm::Prim})
        for (double rate : {0.0, 0.2}) {
            std::string nm = std::string(g == GenAlgorithm::Kruskal ? "Kruskal" : "Prim") + " loopRate=" + std::to_string(rate);
            for (unsigned seed : {0u, 1u, 777u, 123456u}) { // seed = 0 也是有效的固定种子
                MazeGrid a = generateMaze(20, 20, g, seed, rate);
                MazeGrid b = generateMaze(20, 20, g, seed, rate);
                CHECK(sameGrid(a, b), nm << ": same seed " << seed << " produced different mazes");
            }
            MazeGrid x = generateMaze(20, 20, g, 777u, rate);
            MazeGrid y = generateMaze(20, 20, g, 778u, rate);
            CHECK(!sameGrid(x, y), nm << ": seeds 777 and 778 produced the same maze (is seed ignored?)");
        }
}

int main() {
    std::mt19937 rng(20260101); // 固定的种子只用来挑选起点终点和各个迷宫的种子

    struct Spec { int W, H, count; };
    const Spec specs[] = { {1, 1, 5},    {1, 7, 20},  {7, 1, 20},  {2, 2, 100}, {3, 5, 200}, {5, 3, 200},
                          {10, 10, 300}, {25, 25, 200}, {40, 30, 100}, {50, 50, 60}, {100, 100, 15}, {200, 200, 3} };

    long mazes = 0;
    for (const Spec& sp : specs) {
        std::cout << "Testing perfect mazes " << sp.W << "x" << sp.H << " (" << sp.count << " mazes x 2 algorithms) ..." << std::endl;
        for (int i = 0; i < sp.count; ++i)
            for (GenAlgorithm g : {GenAlgorithm::Kruskal, GenAlgorithm::Prim}) {
                std::string gn = (g == GenAlgorithm::Kruskal) ? "Kruskal" : "Prim";
                std::vector<WallBreak> steps;
                MazeGrid m = generateMazeWithSteps(sp.W, sp.H, g, steps);
                checkMazeStructure(m, sp.W, sp.H, tag(gn + " WithSteps", sp.W, sp.H), true);
                checkSteps(m, steps, sp.W, sp.H, tag(gn, sp.W, sp.H), (long)sp.W * sp.H - 1);
                MazeGrid m2 = generateMaze(sp.W, sp.H, g);
                checkMazeStructure(m2, sp.W, sp.H, tag(gn + " plain", sp.W, sp.H), true);
                testSolving(m, sp.W, sp.H, rng, true);
                ++mazes;
            }
    }

    // G. 带环路迷宫
    const Spec loopSpecs[] = { {10, 10, 60}, {25, 25, 30}, {40, 30, 15}, {50, 50, 8}, {100, 100, 3} };
    for (double rate : {0.1, 0.2, 0.3, 1.0}) {
        for (const Spec& sp : loopSpecs) {
            std::cout << "Testing loop mazes " << sp.W << "x" << sp.H << " loopRate=" << rate << " ("
                << sp.count << " mazes x 2 algorithms) ..." << std::endl;
            for (int i = 0; i < sp.count; ++i)
                for (GenAlgorithm g : {GenAlgorithm::Kruskal, GenAlgorithm::Prim}) {
                    std::string gn = (g == GenAlgorithm::Kruskal ? "Kruskal" : "Prim") +
                        std::string(" loop") + std::to_string(rate).substr(0, 3);
                    std::vector<WallBreak> steps;
                    MazeGrid m = generateMazeWithSteps(sp.W, sp.H, g, steps, static_cast<unsigned>(rng()), rate);
                    checkMazeStructure(m, sp.W, sp.H, tag(gn, sp.W, sp.H), false);
                    long want = expectedEdges(sp.W, sp.H, rate);
                    CHECK(countEdges(m, sp.W, sp.H) == want,
                        tag(gn, sp.W, sp.H) << ": open passages " << countEdges(m, sp.W, sp.H) << ", expected " << want);
                    checkSteps(m, steps, sp.W, sp.H, tag(gn + " steps", sp.W, sp.H), want);
                    testSolving(m, sp.W, sp.H, rng, false);
                    ++mazes;
                }
        }
    }

    std::cout << "Testing random seeds ..." << std::endl;
    testSeeds();
    std::cout << "Testing invalid arguments ..." << std::endl;
    testErrors();

    std::cout << "\nMazes generated: " << mazes << "\nChecks run: " << g_checks << "\nChecks failed: " << g_fail << "\n";
    if (g_fail == 0) {
        std::cout << "ALL PASSED\n";
        return 0;
    }
    std::cout << "SOME CHECKS FAILED (first 20 shown above)\n";
    return 1;
}