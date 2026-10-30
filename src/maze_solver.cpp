#include "maze_solver.h"
#include"maze_types.h"
#include <queue>
#include <stack>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
    constexpr Direction kAllDirections[4] = {
        Direction::Up, Direction::Right, Direction::Down, Direction::Left
    };

    // 沿某个方向走一步后的坐标（不检查合法性，配合 canMove 使用）

// ---------- 内部工具函数 ----------

    Point neighborInDirection(Point from, Direction direction)
    {
        switch (direction)
        {
        case Direction::Up:    return { from.row - 1, from.col };
        case Direction::Right: return { from.row, from.col + 1 };
        case Direction::Down:  return { from.row + 1, from.col };
        case Direction::Left:  return { from.row, from.col - 1 };
        }
        return from;
    }

    //判断两个坐标是不是同一个点
    bool samePoint(const Point& a, const Point& b)
    {
        return a.row == b.row && a.col == b.col;
    }

    std::vector<Point> reconstructPath(
        const std::vector<std::vector<Point>>& parent,
        Point start, Point end, bool found)
    {
        //parent是一个二维数组，parent[r][c]记录的是"当初是从哪个格子走到(r,c)这里的
        //逻辑：从终点开始，一路上问是从哪来的，一直问到起点为止
        std::vector<Point> path;
        if (!found) return path;
        //保护逻辑，处理根本走不到终点的情况

        Point cur = end;
        while (!samePoint(cur, start))
        {
            path.push_back(cur);
            cur = parent[cur.row][cur.col];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
        //从终点问到起点是反的，所以需要反过来
        return path;
    }

    // 判断坐标是否落在 height 行、width 列的迷宫范围内
    bool inBounds(const Point& p, int height, int width)
    {
        return p.row >= 0 && p.row < height && p.col >= 0 && p.col < width;
    }

    std::string pointToString(const Point& p)
    {
        return "(" + std::to_string(p.row) + "," + std::to_string(p.col) + ")";
    }

    // 输入检查：不合法就抛异常，避免后面 maze[0]、visited[r][c] 越界访问导致崩溃
    void validateInput(const MazeGrid& maze, Point start, Point end)
    {
        if (maze.empty() || maze[0].empty())
            throw std::invalid_argument("solveMaze: maze is empty");

        const int height = static_cast<int>(maze.size());
        const int width = static_cast<int>(maze[0].size());
        // 每一行的列数必须相同，否则 visited/parent 按 maze[0] 的宽度建表会越界
        for (const auto& row : maze)
        {
            if (static_cast<int>(row.size()) != width)
                throw std::invalid_argument("solveMaze: maze rows have different lengths");
        }

        if (!inBounds(start, height, width))
            throw std::invalid_argument("solveMaze: start " + pointToString(start) + " is out of range");
        if (!inBounds(end, height, width))
            throw std::invalid_argument("solveMaze: end " + pointToString(end) + " is out of range");
    }
}//匿名命名空间

// ---------- 对外接口：canMove ----------

bool canMove(const MazeGrid& maze, Point from, Direction direction)
{
    const int height = static_cast<int>(maze.size());
    if (from.row < 0 || from.row >= height) return false;
    const int width = static_cast<int>(maze[from.row].size());
    if (from.col < 0 || from.col >= width) return false;

    const Cell& cell = maze[from.row][from.col];
    switch (direction)
    {
    case Direction::Up:    return from.row > 0 && !cell.wallTop;
    case Direction::Right: return from.col < width - 1 && !cell.wallRight;
    case Direction::Down:  return from.row < height - 1 && !cell.wallBottom;
    case Direction::Left:  return from.col > 0 && !cell.wallLeft;
    }
    return false;
}

// ---------- BFS ----------

//广度优先搜索
/*
BFS的核心是队列（Queue），队列的特点是"先进先出"，先放进去的先被取出来处理。

流程是：

把起点放进队列，标记为"已访问"
不断从队列头部取出一个格子cur，记录它被访问了（outVisitedOrder）
检查这个格子是不是终点，是的话直接结束
否则，看它上下左右四个方向能不能走、有没有访问过，能走且没访问过的话：标记已访问、记录"来源"（parent）、放进队列尾部，等着后面处理

因为队列是"先进先出"，所以BFS会一层一层往外扩展——先把起点周围一圈都探索完，再探索下一圈，这保证了第一次到达终点时走的路径一定是最短路径，这就是为什么BFS专门用来找最短路。
*/
static void solveBFS(const MazeGrid& maze, Point start, Point end,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    const int height = static_cast<int>(maze.size());
    const int width = static_cast<int>(maze[0].size());

    std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
    std::vector<std::vector<Point>> parent(height, std::vector<Point>(width, { -1, -1 }));

    std::queue<Point> q;
    q.push(start);
    visited[start.row][start.col] = true;
    //一个表格，记录这个格式有没有被处理过，有就标true，防止重走

    bool found = false;

    while (!q.empty())
    {
        Point cur = q.front();
        q.pop();
        outVisitedOrder.push_back(cur);
        //记录访问了起点
        outParentOf.push_back(parent[cur.row][cur.col]);

        if (samePoint(cur, end)) { found = true; break; }

        for (Direction dir : kAllDirections)
        {
            if (!canMove(maze, cur, dir)) continue;
            Point next = neighborInDirection(cur, dir);
            if (visited[next.row][next.col]) continue;

            visited[next.row][next.col] = true;
            parent[next.row][next.col] = cur;
            q.push(next);
        }
    }
    //这一份记录是GUI做探索动画时要用到的数据，还原BFS探索的过程

    outPath = reconstructPath(parent, start, end, found);
}

// ---------- BFS 递归版 ----------

/*
BFS 的递归写法：不用队列，而是"一层一层"地递归。

队列版 BFS 的本质是按距离分层：先处理离起点 0 步的格子，再处理 1 步的，再处理 2 步的……
递归版把"一层"当作一次函数调用：
  - 参数 frontier 是当前这一圈格子（离起点距离相同），按入队的先后顺序排好
  - 依次处理这一圈里的每个格子：记录访问；如果是终点就结束；
    否则把它还没访问过的邻居（按 上→右→下→左）追加到 next（下一圈）
  - 这一圈处理完后，如果 next 不空，就递归处理 next

因为每圈内部的顺序、追加邻居的顺序都和队列版完全一样，
所以访问顺序、父节点、最终路径都和队列版逐格相同，可以直接对比两种写法的性能差异。

递归深度 = 层数 = 起点到最远访问格的步数。完美迷宫里这个距离可能接近格子总数，
所以大迷宫上递归 BFS 和递归 DFS 一样有栈溢出风险（GUI 里会一起限制规模）。
*/
static bool bfsRecursiveLevel(const MazeGrid& maze, const std::vector<Point>& frontier, Point end,
    std::vector<std::vector<bool>>& visited,
    std::vector<std::vector<Point>>& parent,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    std::vector<Point> next;  // 下一圈
    for (const Point& cur : frontier)
    {
        outVisitedOrder.push_back(cur);
        outParentOf.push_back(parent[cur.row][cur.col]);
        if (samePoint(cur, end)) return true;

        for (Direction dir : kAllDirections)
        {
            if (!canMove(maze, cur, dir)) continue;
            Point nb = neighborInDirection(cur, dir);
            if (visited[nb.row][nb.col]) continue;

            visited[nb.row][nb.col] = true;   // 和队列版一样：入"队"时就标记，避免重复加入
            parent[nb.row][nb.col] = cur;
            next.push_back(nb);
        }
    }
    if (next.empty()) return false;          // 没有下一圈了：终点不可达
    return bfsRecursiveLevel(maze, next, end, visited, parent, outVisitedOrder, outParentOf);
}

static void solveBFSRecursive(const MazeGrid& maze, Point start, Point end,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    const int height = static_cast<int>(maze.size());
    const int width = static_cast<int>(maze[0].size());

    std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
    std::vector<std::vector<Point>> parent(height, std::vector<Point>(width, { -1, -1 }));

    visited[start.row][start.col] = true;
    const bool found = bfsRecursiveLevel(maze, { start }, end, visited, parent, outVisitedOrder, outParentOf);
    outPath = reconstructPath(parent, start, end, found);
}

// ---------- DFS 递归版 ----------

/*
DFS的思路完全不同：一条路走到黑，走不通再回头（这就是"深度优先"的意思）。

这里用的是函数递归调用自己来实现"走到黑再回头"：

先把当前格子标记已访问
检查是不是终点，是的话return true（找到了，一路把true传回去）
否则，尝试往上走：如果能走，就递归调用自己去处理"上面那个格子"，这个调用会一直深入下去，直到走到终点或者走进死胡同
如果递归调用返回true，说明那条路走通了，直接把true继续往上传，不用再试其他方向
如果试了上下左右都不通（都返回false），说明这条路走进了死胡同，return false，回到调用它的地方（也就是"上一步"），让上一步换个方向继续试

这个"递归调用自己→深入→碰壁返回→换方向"的过程，本质上就是计算机自动帮你做了"回溯"，不需要你手动维护一个"退回去"的机制，因为函数调用栈本身就记录了"我是怎么一步步走到这里的"。
*/

static bool dfsRecursiveHelper(const MazeGrid& maze, Point cur, Point end,
    std::vector<std::vector<bool>>& visited,
    std::vector<std::vector<Point>>& parent,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    visited[cur.row][cur.col] = true;
    outVisitedOrder.push_back(cur);
    outParentOf.push_back(parent[cur.row][cur.col]);

    if (samePoint(cur, end)) return true;

    for (Direction dir : kAllDirections)
    {
        if (!canMove(maze, cur, dir)) continue;
        Point next = neighborInDirection(cur, dir);
        if (visited[next.row][next.col]) continue;

        parent[next.row][next.col] = cur;
        if (dfsRecursiveHelper(maze, next, end, visited, parent, outVisitedOrder,outParentOf))
            return true;
    }
    return false;
}

static void solveDFSRecursive(const MazeGrid& maze, Point start, Point end,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    const int height = static_cast<int>(maze.size());
    const int width = static_cast<int>(maze[0].size());

    std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
    std::vector<std::vector<Point>> parent(height, std::vector<Point>(width, { -1, -1 }));

    bool found = dfsRecursiveHelper(maze, start, end, visited, parent, outVisitedOrder,outParentOf);
    outPath = reconstructPath(parent, start, end, found);
}

// ---------- DFS 非递归版 ----------

/*
思路：用一个显式的栈，完全模拟递归版的"函数调用栈"。

递归版每进入一个格子，调用栈上就多一层，这一层里记着两件事：
  1. 当前在哪个格子
  2. 四个方向已经试到第几个了（for 循环的进度）
所以这里的栈里存的不是单纯的格子，而是 Frame{格子, 下一个要试的方向}。

每一轮循环只看栈顶：
  - 从栈顶记录的方向开始，往后找第一个"能走且没访问过"的邻居
  - 找到了：记下进度，给邻居标记已访问、记父节点，把邻居压栈（相当于递归调用）
  - 四个方向都试完了：弹栈（相当于函数 return false，回到上一步）

这样两种 DFS 在任何迷宫里（包括有环路的迷宫）访问顺序都一模一样，
唯一区别是：递归版用的是系统调用栈（大小固定，会溢出），
这里用的是堆上的 std::stack（可以动态扩容），因此能处理大得多的迷宫。

（旧写法是"弹出一个格子，把所有邻居一次性压栈"，由于栈后进先出，
 实际探索顺序变成了 左→下→右→上，和递归版相反，对比数据就不公平了。）
*/
static void solveDFSIterative(const MazeGrid& maze, Point start, Point end,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    const int height = static_cast<int>(maze.size());
    const int width = static_cast<int>(maze[0].size());

    std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
    std::vector<std::vector<Point>> parent(height, std::vector<Point>(width, { -1, -1 }));

    struct Frame
    {
        Point cell;
        int nextDir;  // 下一个要尝试的方向在 kAllDirections 中的下标，0~4，4 表示都试完了
    };

    std::stack<Frame> s;

    // "进入"一个格子：和递归版函数开头做的事完全一样
    auto enter = [&](Point cell) {
        visited[cell.row][cell.col] = true;
        outVisitedOrder.push_back(cell);
        outParentOf.push_back(parent[cell.row][cell.col]);
        s.push({ cell, 0 });
    };

    enter(start);
    bool found = samePoint(start, end);

    while (!found && !s.empty())
    {
        Frame& top = s.top();  // 引用：直接修改栈顶的方向进度
        bool advanced = false;

        while (top.nextDir < 4)
        {
            Direction dir = kAllDirections[top.nextDir];
            ++top.nextDir;  // 先推进进度，回溯回来时从下一个方向继续

            if (!canMove(maze, top.cell, dir)) continue;
            Point next = neighborInDirection(top.cell, dir);
            if (visited[next.row][next.col]) continue;

            parent[next.row][next.col] = top.cell;
            enter(next);  // 注意：push 之后 top 引用可能失效，所以马上 break
            advanced = true;
            if (samePoint(next, end)) found = true;
            break;
        }

        if (!advanced) s.pop();  // 四个方向都走不通，回溯
    }

    outPath = reconstructPath(parent, start, end, found);
}

// ---------- 对外统一入口 ----------

void solveMaze(const MazeGrid& maze,
    Point start,
    Point end,
    SolveAlgorithm algo,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder,
    std::vector<Point>& outParentOf)
{
    outPath.clear();
    outVisitedOrder.clear();
    outParentOf.clear();

    validateInput(maze, start, end);  // 先把三个输出数组清空，再检查输入，不合法直接抛 std::invalid_argument

    switch (algo)
    {
    case SolveAlgorithm::BFS:
        solveBFS(maze, start, end, outPath, outVisitedOrder,outParentOf);
        break;
    case SolveAlgorithm::DFS_Recursive:
        solveDFSRecursive(maze, start, end, outPath, outVisitedOrder,outParentOf);
        break;
    case SolveAlgorithm::BFS_Recursive:
        solveBFSRecursive(maze, start, end, outPath, outVisitedOrder, outParentOf);
        break;
    case SolveAlgorithm::DFS_Iterative:
        solveDFSIterative(maze, start, end, outPath, outVisitedOrder, outParentOf);
        break;
    default:
        throw std::invalid_argument("solveMaze: unknown algorithm");
    }
}
