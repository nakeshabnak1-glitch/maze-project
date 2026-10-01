#include <QApplication>
#include "src/maze_generator.h"
#include"src/maze_solver.h"
#include "src/gui.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MazeGrid maze = generateMaze(15, 15, GenAlgorithm::Kruskal);

    Point start{ 0, 0 };
    Point end{ 14, 14 };
    std::vector<Point> path, visitedOrder, parentOf;
    solveMaze(maze, start, end, SolveAlgorithm::BFS, path, visitedOrder, parentOf);

    MazeWidget widget;
    widget.setMaze(maze);
    widget.setSolution(path, visitedOrder, parentOf);
    widget.setWindowTitle("迷宫求解动画 - BFS");
    widget.show();
    widget.startAnimation();  // 窗口显示后立刻开始播放动画
    return app.exec();//事件循环
}

/*
#include <iostream>
#include <windows.h>
#include "src/maze_generator.h"
#include "src/maze_solver.h"
using namespace std;

void runTest(const MazeGrid& maze, Point start, Point end, SolveAlgorithm algo, const string& name)
{
    vector<Point> path, visitedOrder, parentOf;
    solveMaze(maze, start, end, algo, path, visitedOrder, parentOf);

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
*/
