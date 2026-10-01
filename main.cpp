#include <iostream>
#include <windows.h>
#include "src/maze_generator.h"
#include "src/maze_solver.h"
using namespace std;

void runTest(const MazeGrid& maze, Point start, Point end, SolveAlgorithm algo, const string& name)
{
    vector<Point> path, visitedOrder;
    solveMaze(maze, start, end, algo, path, visitedOrder);

    cout << "===== " << name << " =====\n";
    cout << "路径长度: " << path.size() << "\n";
    cout << "访问节点数: " << visitedOrder.size() << "\n";
    cout << "路径: ";
    for (const auto& p : path)
        cout << "(" << p.row << "," << p.col << ") ";
    cout << "\n\n";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8); // 让控制台用UTF-8显示，解决中文乱码

    // 固定用同一个迷宫测三种算法，这样对比才有意义
    MazeGrid maze = generateMaze(5, 5, GenAlgorithm::Kruskal);
    Point start{ 0, 0 };
    Point end{ 4, 4 };

    runTest(maze, start, end, SolveAlgorithm::BFS, "BFS");
    runTest(maze, start, end, SolveAlgorithm::DFS_Recursive, "DFS递归");
    runTest(maze, start, end, SolveAlgorithm::DFS_Iterative, "DFS非递归");

    return 0;
}