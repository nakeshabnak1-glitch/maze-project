#include "maze_generator.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>

namespace
{
// 并查集：parent_ 保存父节点，size_ 保存以根节点为代表的集合大小。
class UnionFind
{
public:
    explicit UnionFind(std::size_t count) : parent_(count), size_(count, 1)
    {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }

    std::size_t find(std::size_t id)
    {
        // 内部调用者必须传入有效编号；at 同时防止误用造成越界访问。
        if (parent_.at(id) != id)
        {
            parent_[id] = find(parent_[id]); // 路径压缩：直接指向根节点。
        }
        return parent_[id];
    }

    bool unite(std::size_t first, std::size_t second)
    {
        first = find(first);
        second = find(second);
        if (first == second) return false;

        // 小集合挂到大集合下，避免树过高。
        if (size_[first] < size_[second]) std::swap(first, second);
        parent_[second] = first;
        size_[first] += size_[second];
        return true;
    }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> size_;
};

// 仅供 A 模块内部使用的方向，不属于 A/B 公共接口。
enum class InternalDirection
{
    Up,
    Right,
    Down,
    Left
};

// 保留已验证的内部方向表示，仅在生成公共记录时转换。
Direction toPublicDirection(InternalDirection direction)
{
    switch (direction)
    {
    case InternalDirection::Up: return Direction::Up;
    case InternalDirection::Right: return Direction::Right;
    case InternalDirection::Down: return Direction::Down;
    case InternalDirection::Left: return Direction::Left;
    default: throw std::invalid_argument("Unknown internal direction");
    }
}

// 使用 maze[row][col] 访问：height 行、width 列。
// Cell 的默认值保证每个格子初始四面都有墙。
MazeGrid createMaze(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("Maze width and height must be positive");
    }

    return MazeGrid(static_cast<std::size_t>(height),
                    std::vector<Cell>(static_cast<std::size_t>(width)));
}

// 当前格子、相邻格子或方向不合法时，返回 false，且不修改迷宫。
// 后续生成算法统一通过此函数拆墙，同时更新两个格子的对应墙壁。
// steps 非空时，只记录真正改变墙壁的成功操作，供生成动画回放使用。
bool removeWall(MazeGrid& maze, int row, int col, InternalDirection direction,
                std::vector<WallBreak>* steps = nullptr)
{
    if (row < 0 || col < 0)
    {
        return false;
    }

    const auto r = static_cast<std::size_t>(row);
    const auto c = static_cast<std::size_t>(col);
    if (r >= maze.size() || c >= maze[r].size())
    {
        return false;
    }

    auto neighborRow = r;
    auto neighborCol = c;
    switch (direction)
    {
    case InternalDirection::Up:
        if (r == 0) return false;
        --neighborRow;
        break;
    case InternalDirection::Right:
        ++neighborCol;
        break;
    case InternalDirection::Down:
        ++neighborRow;
        break;
    case InternalDirection::Left:
        if (c == 0) return false;
        --neighborCol;
        break;
    default:
        return false;
    }

    // 按目标行的实际列数检查，避免各行长度不一致时发生越界。
    if (neighborRow >= maze.size() || neighborCol >= maze[neighborRow].size())
    {
        return false;
    }

    Cell& current = maze[r][c];
    Cell& neighbor = maze[neighborRow][neighborCol];
    bool changed = false;
    switch (direction)
    {
    case InternalDirection::Up:
        changed = current.wallTop || neighbor.wallBottom;
        current.wallTop = false;
        neighbor.wallBottom = false;
        break;
    case InternalDirection::Right:
        changed = current.wallRight || neighbor.wallLeft;
        current.wallRight = false;
        neighbor.wallLeft = false;
        break;
    case InternalDirection::Down:
        changed = current.wallBottom || neighbor.wallTop;
        current.wallBottom = false;
        neighbor.wallTop = false;
        break;
    case InternalDirection::Left:
        changed = current.wallLeft || neighbor.wallRight;
        current.wallLeft = false;
        neighbor.wallRight = false;
        break;
    }

    if (steps != nullptr && changed)
        steps->push_back({{row, col}, toPublicDirection(direction)});
    return true;
}
// 只保存右墙和下墙，避免同一面内部墙被重复加入候选列表。
struct CandidateWall
{
    int row;
    int col;
    InternalDirection direction;
};

MazeGrid generateKruskal(int width, int height, std::vector<WallBreak>* steps)
{
    // 先检查尺寸，再进行无符号转换和乘法，避免非法尺寸或编号溢出。
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Maze width and height must be positive");
    const auto columns = static_cast<std::size_t>(width);
    const auto rows = static_cast<std::size_t>(height);
    if (rows > std::numeric_limits<std::size_t>::max() / columns)
        throw std::length_error("Maze cell count is too large");
    const auto cellCount = rows * columns;

    MazeGrid maze = createMaze(width, height);
    if (cellCount == 1) return maze;

    std::vector<CandidateWall> walls;
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            if (col < width - 1)
                walls.push_back({row, col, InternalDirection::Right});
            if (row < height - 1)
                walls.push_back({row, col, InternalDirection::Down});
        }
    }

    // 每次调用使用局部随机引擎，不保存全局生成状态。
    std::mt19937 randomEngine(std::random_device{}());
    std::shuffle(walls.begin(), walls.end(), randomEngine);

    UnionFind sets(cellCount);
    std::size_t removedCount = 0;
    for (const auto& wall : walls)
    {
        // 前面有 row 个完整行，每行 width 个格子，再加本行的 col 偏移。
        const auto first = static_cast<std::size_t>(wall.row) * columns
                         + static_cast<std::size_t>(wall.col);
        const auto second = first + (wall.direction == InternalDirection::Right
                                      ? std::size_t{1} : columns);
        if (sets.find(first) == sets.find(second)) continue;

        // 所有墙壁更新统一交给 removeWall；候选墙合法时必须拆除成功。
        if (!removeWall(maze, wall.row, wall.col, wall.direction, steps))
            throw std::logic_error("Invalid Kruskal candidate wall");
        sets.unite(first, second);
        ++removedCount;
        if (removedCount == cellCount - 1) break;
    }

    if (removedCount != cellCount - 1)
        throw std::logic_error("Kruskal did not connect all cells");
    return maze;
}
// 候选墙始终从已访问格子指向尚未访问格子；入堆后目标也可能被别的边连接。
struct PrimWall
{
    int row;
    int col;
    int targetRow;
    int targetCol;
    InternalDirection direction;
    std::mt19937::result_type priority;
};

struct PrimWallCompare
{
    bool operator()(const PrimWall& first, const PrimWall& second) const
    {
        // priority_queue 默认大值优先；这里反向比较，实现小权重优先。
        return first.priority > second.priority;
    }
};

using PrimQueue = std::priority_queue<PrimWall, std::vector<PrimWall>, PrimWallCompare>;

// 每个格子只在首次访问时扩展。边首次成为候选时分配随机权重，之后不再改变。
void addPrimWalls(int row, int col, int width, int height,
                  const std::vector<std::vector<bool>>& visited,
                  PrimQueue& walls, std::mt19937& randomEngine)
{
    if (row > 0 && !visited[row - 1][col])
        walls.push({row, col, row - 1, col, InternalDirection::Up, randomEngine()});
    if (col < width - 1 && !visited[row][col + 1])
        walls.push({row, col, row, col + 1, InternalDirection::Right, randomEngine()});
    if (row < height - 1 && !visited[row + 1][col])
        walls.push({row, col, row + 1, col, InternalDirection::Down, randomEngine()});
    if (col > 0 && !visited[row][col - 1])
        walls.push({row, col, row, col - 1, InternalDirection::Left, randomEngine()});
}

MazeGrid generatePrim(int width, int height, std::vector<WallBreak>* steps)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Maze width and height must be positive");
    const auto rows = static_cast<std::size_t>(height);
    const auto columns = static_cast<std::size_t>(width);
    if (rows > std::numeric_limits<std::size_t>::max() / columns)
        throw std::length_error("Maze cell count is too large");
    const auto cellCount = rows * columns;

    MazeGrid maze = createMaze(width, height);
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(columns, false));
    std::mt19937 randomEngine(std::random_device{}());
    const int startRow = std::uniform_int_distribution<int>(0, height - 1)(randomEngine);
    const int startCol = std::uniform_int_distribution<int>(0, width - 1)(randomEngine);
    visited[startRow][startCol] = true;
    std::size_t visitedCount = 1;
    PrimQueue walls;
    addPrimWalls(startRow, startCol, width, height, visited, walls, randomEngine);

    while (!walls.empty() && visitedCount < cellCount)
    {
        const PrimWall wall = walls.top();
        walls.pop();
        // 跳过过期候选墙，避免连接两个已访问格子而产生环。
        if (visited[wall.targetRow][wall.targetCol]) continue;
        if (!removeWall(maze, wall.row, wall.col, wall.direction, steps))
            throw std::logic_error("Invalid Prim candidate wall");
        visited[wall.targetRow][wall.targetCol] = true;
        ++visitedCount;
        addPrimWalls(wall.targetRow, wall.targetCol, width, height,
                     visited, walls, randomEngine);
    }

    // 起点之外，每访问一个新格子恰好拆一面墙，因此最终拆墙数为 N-1。
    if (visitedCount != cellCount)
        throw std::logic_error("Prim did not connect all cells");
    return maze;
}
// 两个公共入口共用分发和算法，空指针表示不收集过程记录。
MazeGrid generateMazeInternal(int width, int height, GenAlgorithm algo,
                              std::vector<WallBreak>* steps)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Maze width and height must be positive");

    switch (algo)
    {
    case GenAlgorithm::Kruskal:
        return generateKruskal(width, height, steps);
    case GenAlgorithm::Prim:
        return generatePrim(width, height, steps);
    default:
        throw std::invalid_argument("Unknown maze generation algorithm");
    }
}
} // 匿名命名空间：以上辅助类型和函数仅在当前源文件内可用。

MazeGrid generateMaze(int width, int height, GenAlgorithm algo)
{
    return generateMazeInternal(width, height, algo, nullptr);
}

MazeGrid generateMazeWithSteps(int width, int height, GenAlgorithm algo,
                               std::vector<WallBreak>& outSteps)
{
    outSteps.clear();
    // 成功后再交付记录；生成中途抛出异常时，外部不会拿到不完整记录。
    std::vector<WallBreak> steps;
    MazeGrid maze = generateMazeInternal(width, height, algo, &steps);
    outSteps.swap(steps);
    return maze;
}
