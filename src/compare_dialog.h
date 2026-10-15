#ifndef COMPARE_DIALOG_H
#define COMPARE_DIALOG_H

#include <QDialog>
#include <vector>
#include "maze_types.h"
#include "maze_solver.h"

class MazeWidget;
class QPushButton;
class QSlider;
class QLabel;
class QTableWidget;

/*
 * CompareDialog："三种算法对比"窗口。
 *
 * 在【同一个迷宫、同一对起终点】上分别运行 BFS、DFS 递归、DFS 非递归：
 *   - 上半部分：三个小迷宫并排，可以同步播放探索过程（谁访问的格子多一眼就能看出来）
 *   - 下半部分：数据表 —— 访问格数、占比、路径长度、是否最短、平均求解耗时
 *
 * 耗时测量：单次求解往往只有几十微秒，计时误差比结果本身还大，
 * 所以每个算法重复运行多次（总时长约 20ms，最多 2000 次）取平均。
 */
class CompareDialog : public QDialog
{
    Q_OBJECT

public:
    // allowRecursive=false 时（迷宫过大）不运行递归 DFS，避免栈溢出崩溃
    CompareDialog(const MazeGrid& maze, Point start, Point end,
                  const QString& mazeDescription, bool allowRecursive,
                  QWidget* parent = nullptr);

private:
    struct Result
    {
        SolveAlgorithm algo = SolveAlgorithm::BFS;
        QString name;
        bool ran = false;              // 是否实际运行（递归 DFS 在大迷宫上会跳过）
        std::vector<Point> path;
        std::vector<Point> visitedOrder;
        std::vector<Point> parentOf;
        double avgMicros = 0.0;        // 平均单次求解耗时（微秒）
        int repeats = 0;               // 计时重复次数
    };

    void runAll();                     // 运行三种算法并计时
    void buildTable();                 // 填写数据表
    QString buildSummary() const;      // 一句话结论
    void updateButtons();

    MazeGrid maze_;
    Point start_;
    Point end_;
    bool allowRecursive_;
    std::vector<Result> results_;

    std::vector<MazeWidget*> views_;   // 三个小迷宫，与 results_ 一一对应
    QPushButton* playButton_ = nullptr;
    QPushButton* skipButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QSlider* speedSlider_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
};

#endif
