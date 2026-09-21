# maze-project
## 📌 模块接口说明

### 迷宫生成模块 `maze_generator`

```
// 核心数据结构
struct Cell { bool wallTop, wallRight, wallBottom, wallLeft; };
using MazeGrid = std::vector<std::vector<Cell>>;

// 对外接口
MazeGrid generateMaze(int width, int height, GenAlgorithm algo);
```
### 迷宫求解模块 `maze_solver`

```
void solveMaze(const MazeGrid& maze,
               Point start,
               Point end,
               SolveAlgorithm algo,
               std::vector<Point>& outPath,
               std::vector<Point>& outVisitedOrder);
```

- `outPath`：输出最终从起点到终点的路径
- `outVisitedOrder`：输出节点访问顺序，用于 GUI 动画回放

> 
> 返回统一格式迷宫网格，供求解模块直接调用。
以下是整体逻辑框架
maze-project/
├─ src/                     # 源代码
│   ├─ maze_generator.h/cpp # A：迷宫生成，Union‑Find、Kruskal、Prim
│   ├─ maze_solver.h/cpp    # B：迷宫求解 BFS / DFS 递归 / DFS 非递归
│   └─ gui.h/cpp            # B：Qt 图形界面（加分项，后期实现）
├─ test/
│   └─ performance_test.cpp # C：性能测试、耗时统计实验
├─ main.cpp                 # 程序入口
├─ CMakeLists.txt           # CMake 编译脚本
├─ .gitignore               # 忽略编译产物、IDE 临时文件
├─ README.md                # 项目说明文档
└─ docs/                    # 提案 PPT、报告、会议纪要等文档
