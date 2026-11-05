# maze-project

数据结构课程项目（Project 1）：迷宫生成与求解系统（C++17 + Qt 6 + WebAssembly 网页版）

| 成员 | 负责 |
|---|---|
| 李婷婷 | 迷宫生成：并查集、Kruskal、Prim（堆）、固定种子与环路 |
| 方雨婷 | 迷宫求解：BFS / DFS 的递归与非递归写法；Qt 界面与网页界面 |
| 白荷初 | 正确性测试、性能测试、对比分析与报告 |

- 生成：Kruskal（并查集）、Prim（堆优化），默认生成**完美迷宫**（任意两点之间有且仅有一条路径）
- 求解：BFS、DFS 各有递归 / 非递归两种写法（BFS 非递归用显式队列、递归为逐层递归；DFS 非递归用显式栈）
- 可视化：Qt 桌面版和网页版，两者功能一致
  - 闯关冒险：玩家带小探险家走迷宫找宝箱，通关后与 BFS 最短路线对比评星
  - 算法实验室：生成过程 / 求解过程逐步动画，生成算法对比（Kruskal vs Prim），求解算法对比（四种）

## 📋 课程要求对照

| 类别 | 要求 | 在哪里体现 |
|---|---|---|
| 基本 | 并查集 + Kruskal / Prim 生成完美迷宫（Prim 用堆） | `src/maze_generator.cpp`；实验室环路比例保持 0% 即完美迷宫，拆墙次数恰好 N−1 |
| 基本 | BFS 求最短路径、DFS 搜索 | `src/maze_solver.cpp`；实验室 → 求解算法 |
| 基本 | 搜索算法都有递归和非递归（显式栈 / 队列）两种写法 | 四种写法都有；求解算法对比会自动验证递归版与非递归版逐格一致 |
| 基本 | 逐步显示搜索路径 | 实验室的播放 / 单步 / 跳到结尾，热力图显示离起点的距离 |
| 额外 | 更好的交互式界面 | 闯关冒险 |
| 额外 | 可视化比较不同生成算法 | 生成算法对比：并排同步播放 + 多样本统计 |
| 额外 | 性能（时间 / 空间复杂度）分析 | 对比页给出实测耗时；完整测试与报告见 `test/` 与 `docs/` |

## 🌐 网页版（不用打开 VS）

直接双击 `web/index.html` 用浏览器（Chrome / Edge / Firefox / Safari）打开即可，不需要安装任何东西、也不需要服务器。

- 网页里的迷宫生成和求解**不是用 JavaScript 重写的**，而是把 `src/maze_generator.cpp`（李婷婷）和 `src/maze_solver.cpp`（方雨婷）
  原样编译成 WebAssembly 嵌在网页里运行；网页（JavaScript + Canvas）只负责画图和交互
- 页签：闯关冒险 / 算法实验室 / 生成算法对比 / 求解算法对比 / 关于项目（课程要求对照表），手机上也能用
- 闯关冒险是一条串联流程：**选关 → 生成动画（Kruskal / Prim 逐面拆墙）→ 玩家闯关 → 结算评星 → 算法复盘**
  （在刚玩过的地图上回放 BFS / DFS 的搜索过程，与玩家自己的路线对比）

**重新生成网页**（只有改了 `src/` 里的 C++ 算法或 `web/index.template.html` 时才需要）：

1. 下载 [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases) 的 Windows 版（`wasi-sdk-25.0-x86_64-windows.tar.gz`）并解压，
   默认路径 `D:\wasi-sdk-25.0-x86_64-windows`（解压到别处就修改 `build_wasm.bat` 里的 `WASI_SDK`）
2. 双击 `web/build_wasm.bat`：C++ → `maze.wasm` → 嵌入 `index.template.html`，生成 `index.html`

```
web/
├─ index.html            # 生成好的单文件网页（已嵌入 maze.wasm，双击打开）
├─ index.template.html   # 网页源码（HTML + CSS + JS），修改界面改这个
├─ maze.wasm             # 由 src/ 里的 C++ 算法编译出的 WebAssembly
├─ wasm_api.cpp          # C++ 与网页之间的接口：调用 generateMazeWithSteps / solveMaze，把结果交给 JS（不改算法）
├─ build_wasm.bat        # 编译脚本（Windows）：C++ → maze.wasm → index.html
└─ embed.ps1             # 被 build_wasm.bat 调用：把 maze.wasm 以 base64 嵌入网页
```

## 📁 目录结构

```
maze-project/
├─ src/                         # 源代码
│   ├─ maze_types.h             # 公共数据结构：Point / Direction / Cell / MazeGrid / WallBreak
│   ├─ maze_generator.h/cpp     # 李婷婷：迷宫生成，Union-Find、Kruskal、Prim
│   ├─ maze_solver.h/cpp        # 方雨婷：迷宫求解 BFS / DFS 的递归与非递归
│   ├─ theme.h                  # 方雨婷：界面主题（卡通配色与样式表，只有头文件）
│   ├─ app_window.h/cpp         # 方雨婷：顶层窗口，"闯关冒险" + "算法实验室" 两个页签
│   ├─ game_page.h/cpp          # 方雨婷：闯关冒险页（关卡、步数/用时、结算评星）
│   ├─ game_widget.h/cpp        # 方雨婷：闯关画布（角色移动、脚印、宝箱、撒花、最短路线叠加）
│   ├─ gui.h/cpp                # 方雨婷：MazeWidget，画迷宫、播放生成/求解动画（实验室和对比窗口共用）
│   ├─ main_window.h/cpp        # 方雨婷：算法实验室页，左侧控制面板 + 右侧迷宫
│   ├─ gen_compare_dialog.h/cpp # 方雨婷："生成算法对比"窗口（Kruskal vs Prim）
│   └─ compare_dialog.h/cpp     # 方雨婷："求解算法对比"窗口（四种求解算法）
├─ test/
│   ├─ performance_test.cpp     # 白荷初：性能测试、耗时统计实验
│   └─ correctness_test.cpp     # 白荷初：正确性测试
├─ web/                         # 方雨婷：网页版（见上文"网页版"）
├─ main.cpp                     # Qt 程序入口（套用卡通主题，创建 AppWindow）
├─ CMakeLists.txt               # CMake 编译脚本
├─ .gitignore                   # 忽略编译产物、IDE 临时文件
├─ README.md                    # 项目说明文档
└─ docs/                        # 提案 PPT、报告、会议纪要、性能数据等文档
```

> ⚠️ `CMakeLists.txt` 用 `file(GLOB src/*.cpp)` 收集源文件，**新增 .cpp 文件后必须重新配置 CMake**
> （Visual Studio：项目 → 删除缓存并重新配置），重新配置后要在启动项里重新选择 `maze_project.exe`。
> `web/` 不在 CMake 的编译范围内，不影响 Qt 版。

## 📌 模块接口说明

### 公共数据结构 `maze_types.h`

```cpp
struct Point { int row; int col; };                 // 行号、列号均从 0 开始
enum class Direction { Up, Right, Down, Left };
struct Cell { bool wallTop{true}, wallRight{true}, wallBottom{true}, wallLeft{true}; };  // true 表示有墙
using MazeGrid = std::vector<std::vector<Cell>>;   // maze[row][col]：height 行、width 列
struct WallBreak { Point from; Direction direction; };  // 一次拆墙：from 格子朝 direction 方向
```

### 迷宫生成模块 `maze_generator`（李婷婷）

```cpp
enum class GenAlgorithm { Kruskal, Prim };

// 生成 height 行、width 列的完美迷宫
MazeGrid generateMaze(int width, int height, GenAlgorithm algo);

// 同上，并输出真实拆墙顺序（共 格子数-1 条），GUI 用它播放生成动画
MazeGrid generateMazeWithSteps(int width, int height, GenAlgorithm algo,
                               std::vector<WallBreak>& outSteps);

// 带固定种子（结果可复现）和环路比例的重载；loopRate = 0 时仍是完美迷宫
MazeGrid generateMaze(int width, int height, GenAlgorithm algo,
                      unsigned seed, double loopRate = 0.0);
MazeGrid generateMazeWithSteps(int width, int height, GenAlgorithm algo,
                               std::vector<WallBreak>& outSteps,
                               unsigned seed, double loopRate = 0.0);
```

- 课程基本要求是完美迷宫，默认 `loopRate = 0`；带环路只是为了体现 BFS 求最短路优势的扩展
- 带环路时，拆墙记录里前 格子数-1 步是生成树，之后是额外拆的墙
- 宽或高 ≤ 0、`loopRate` 不在 [0,1] 内时抛出 `std::invalid_argument`
- 同一种子在不同标准库（MSVC / libc++）下生成的迷宫可能不同，所以 Qt 版和网页版的同一关卡迷宫不一样

### 迷宫求解模块 `maze_solver`（方雨婷）

```cpp
enum class SolveAlgorithm
{
    BFS,            // 广度优先，非递归（显式队列）
    DFS_Recursive,  // 深度优先，递归
    DFS_Iterative,  // 深度优先，非递归（显式栈）
    BFS_Recursive   // 广度优先，递归（逐层递归），追加在末尾，不影响已有枚举值
};

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
- 每种搜索的递归版与非递归版访问顺序**逐格完全一致**（都按 上→右→下→左 尝试），便于公平对比：
  - `DFS_Recursive` / `DFS_Iterative`：系统调用栈 vs 堆上的显式栈
  - `BFS_Recursive` / `BFS`：每层递归处理"当前这一圈"格子 vs 显式队列
- ⚠️ 递归版的递归深度可能接近格子总数（DFS 为路径深度，BFS 为层数），迷宫很大时会栈溢出崩溃
  （无法用 try/catch 捕获），界面中超过 50×50 格会自动禁用两个递归版；性能测试中请对递归版限制规模

## 🖥️ 界面用法（Qt 版与网页版相同）

### 闯关冒险
- 方向键 / WASD 移动，R 重新开始；撞墙不计步数（网页版在手机上可以滑动或点方向按钮）
- 6 个关卡（固定种子，同一关大家玩到的迷宫相同）；前两关是完美迷宫，后四关带环路
- 到达宝箱后结算：你的步数 vs BFS 最短步数 → 探索效率 → 1~3 星（≥95% 三星，≥70% 二星）；
  评语按实际多走的步数写，只有正好等于最短步数才算"完美通关"
- 网页版额外有：选关后的生成动画、通关后的"算法复盘"

### 算法实验室
- **迷宫设置**：宽、高（2~120）、生成算法；勾选"播放生成过程"后，点"生成新迷宫"会先从全墙网格开始逐步拆墙
- **环路比例**（0~30%）：默认 0% = 完美迷宫（课程基本要求）；调大后在生成树之外再随机拆掉一部分墙形成环路，
  路径不再唯一，配合"求解算法对比"可以看到 BFS 路径始终最短、DFS 路径可能明显更长
- **求解算法**：BFS 非递归 / BFS 递归 / DFS 递归 / DFS 非递归，切换后在同一个迷宫上重新求解
- **鼠标**：在迷宫上左键设置起点，右键设置终点
- **动画控制**：速度（每秒 1~1000 步）、播放/暂停、单步、跳到结尾、重置
- **统计信息**：生成/求解耗时（不含动画）、访问格数及占比、路径长度
- **生成算法对比**：同尺寸下 Kruskal 与 Prim 并排同步播放生成过程；统计多个随机迷宫的平均生成耗时、拆墙次数、死胡同、岔路口、路径长度等
- **求解算法对比**：同一迷宫、同一起终点上四种求解算法并排同步播放，并列出访问格数、路径长度、是否最短、平均求解耗时
