# maze-project

数据结构课程项目：迷宫生成与求解系统（C++17 + Qt 6）

- 生成：Kruskal（并查集）、Prim（堆优化），生成的都是**完美迷宫**（任意两点之间有且仅有一条路径）
- 求解：BFS、DFS 递归、DFS 非递归（显式栈）
- 可视化：Qt 图形界面，可播放生成过程和求解过程动画，并对三种求解算法做并排对比

## 📁 目录结构

```
maze-project/
├─ src/                       # 源代码
│   ├─ maze_types.h           # 公共数据结构：Point / Direction / Cell / MazeGrid / WallBreak（A、B 共用）
│   ├─ maze_generator.h/cpp   # A：迷宫生成，Union-Find、Kruskal、Prim
│   ├─ maze_solver.h/cpp      # B：迷宫求解 BFS / DFS 递归 / DFS 非递归
│   ├─ gui.h/cpp              # B：MazeWidget，负责画迷宫、播放生成/求解动画
│   ├─ main_window.h/cpp      # B：主窗口，左侧控制面板 + 右侧迷宫
│   └─ compare_dialog.h/cpp   # B："三种算法对比"窗口
├─ test/
│   └─ performance_test.cpp   # C：性能测试、耗时统计实验
├─ main.cpp                   # 程序入口（创建 MainWindow）
├─ CMakeLists.txt             # CMake 编译脚本
├─ .gitignore                 # 忽略编译产物、IDE 临时文件
├─ README.md                  # 项目说明文档
└─ docs/                      # 提案 PPT、报告、会议纪要等文档
```

> ⚠️ `CMakeLists.txt` 用 `file(GLOB src/*.cpp)` 收集源文件，**新增 .cpp 文件后必须重新配置 CMake**
> （Visual Studio：项目 → 删除缓存并重新配置），否则会报"无法解析的外部符号"。

## 📌 模块接口说明

### 公共数据结构 `maze_types.h`

```cpp
struct Point { int row; int col; };                 // 行号、列号均从 0 开始
enum class Direction { Up, Right, Down, Left };
struct Cell { bool wallTop{true}, wallRight{true}, wallBottom{true}, wallLeft{true}; };  // true 表示有墙
using MazeGrid = std::vector<std::vector<Cell>>;   // maze[row][col]：height 行、width 列
struct WallBreak { Point from; Direction direction; };  // 一次拆墙：from 格子朝 direction 方向
```

### 迷宫生成模块 `maze_generator`（A）

```cpp
enum class GenAlgorithm { Kruskal, Prim };

// 生成 height 行、width 列的完美迷宫
MazeGrid generateMaze(int width, int height, GenAlgorithm algo);

// 同上，并输出真实拆墙顺序（共 格子数-1 条），GUI 用它播放生成动画
MazeGrid generateMazeWithSteps(int width, int height, GenAlgorithm algo,
                               std::vector<WallBreak>& outSteps);
```

- 宽或高 ≤ 0 时抛出 `std::invalid_argument`

### 迷宫求解模块 `maze_solver`（B）

```cpp
enum class SolveAlgorithm { BFS, DFS_Recursive, DFS_Iterative };

void solveMaze(const MazeGrid& maze,
               Point start,
               Point end,
               SolveAlgorithm algo,
               std::vector<Point>& outPath,
               std::vector<Point>& outVisitedOrder,
               std::vector<Point>& outParentOf);
```

- `outPath`：从起点到终点的路径（含两端）；**终点不可达时为空**（不抛异常）
- `outVisitedOrder`：格子被访问的先后顺序，用于 GUI 动画回放；其长度即"访问格数"
- `outParentOf`：`outParentOf[i]` 是 `outVisitedOrder[i]` 的父节点（从哪个格子走过来的），起点固定为 `{-1,-1}`
- 迷宫为空、各行长度不一致、起点或终点越界时，抛出 `std::invalid_argument`（抛出前三个输出已清空）
- `DFS_Recursive` 与 `DFS_Iterative` 的访问顺序**逐格完全一致**（都按 上→右→下→左 尝试），
  区别只在于前者用系统调用栈、后者用堆上的显式栈，便于公平对比
- ⚠️ 递归 DFS 的递归深度最坏等于格子总数，迷宫很大时会栈溢出崩溃（无法用 try/catch 捕获），
  GUI 中超过 50×50 格会自动禁用；性能测试中请对递归 DFS 限制规模

## 🖥️ 图形界面用法

- **迷宫设置**：宽、高（2~120）、生成算法；勾选"播放生成过程"后，点"生成新迷宫"会先从全墙网格开始逐步拆墙
- **求解算法**：BFS / DFS 递归 / DFS 非递归，切换后在同一个迷宫上重新求解
- **鼠标**：在迷宫上左键设置起点，右键设置终点
- **动画控制**：速度（每秒 1~1000 步）、播放/暂停、单步、跳到结尾、重置
- **统计信息**：生成/求解耗时（不含动画）、访问格数及占比、路径长度
- **三种算法对比**：同一迷宫、同一起终点上三个算法并排同步播放，并列出访问格数、路径长度、是否最短、平均求解耗时
