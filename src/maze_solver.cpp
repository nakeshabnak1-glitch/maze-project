#include "maze_solver.h"
#include"maze_types.h"
#include <queue>
#include <stack>
#include <algorithm>

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
    std::vector<Point>& outVisitedOrder)
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
    std::vector<Point>& outVisitedOrder)
{
    visited[cur.row][cur.col] = true;
    outVisitedOrder.push_back(cur);

    if (samePoint(cur, end)) return true;

    for (Direction dir : kAllDirections)
    {
        if (!canMove(maze, cur, dir)) continue;
        Point next = neighborInDirection(cur, dir);
        if (visited[next.row][next.col]) continue;

        parent[next.row][next.col] = cur;
        if (dfsRecursiveHelper(maze, next, end, visited, parent, outVisitedOrder))
            return true;
    }
    return false;
}

static void solveDFSRecursive(const MazeGrid& maze, Point start, Point end,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder)
{
    const int height = static_cast<int>(maze.size());
    const int width = static_cast<int>(maze[0].size());

    std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
    std::vector<std::vector<Point>> parent(height, std::vector<Point>(width, { -1, -1 }));

    bool found = dfsRecursiveHelper(maze, start, end, visited, parent, outVisitedOrder);
    outPath = reconstructPath(parent, start, end, found);
}

// ---------- DFS 非递归版 ----------

static void solveDFSIterative(const MazeGrid& maze, Point start, Point end,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder)
{
    const int height = static_cast<int>(maze.size());
    const int width = static_cast<int>(maze[0].size());

    std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
    std::vector<std::vector<Point>> parent(height, std::vector<Point>(width, { -1, -1 }));

    std::stack<Point> s;
    s.push(start);
    bool found = false;

    while (!s.empty())
    {
        Point cur = s.top();
        s.pop();
        //取出栈顶元素

        if (visited[cur.row][cur.col]) continue;
        visited[cur.row][cur.col] = true;
        outVisitedOrder.push_back(cur);

        if (samePoint(cur, end)) { found = true; break; }

        for (Direction dir : kAllDirections)
        {
            if (!canMove(maze, cur, dir)) continue;
            Point next = neighborInDirection(cur, dir);
            if (visited[next.row][next.col]) continue;

            parent[next.row][next.col] = cur;
            s.push(next);
        }
    }

    outPath = reconstructPath(parent, start, end, found);
}

// ---------- 对外统一入口 ----------

void solveMaze(const MazeGrid& maze,
    Point start,
    Point end,
    SolveAlgorithm algo,
    std::vector<Point>& outPath,
    std::vector<Point>& outVisitedOrder)
{
    outPath.clear();
    outVisitedOrder.clear();

    switch (algo)
    {
    case SolveAlgorithm::BFS:
        solveBFS(maze, start, end, outPath, outVisitedOrder);
        break;
    case SolveAlgorithm::DFS_Recursive:
        solveDFSRecursive(maze, start, end, outPath, outVisitedOrder);
        break;
    case SolveAlgorithm::DFS_Iterative:
        solveDFSIterative(maze, start, end, outPath, outVisitedOrder);
        break;
    }
}