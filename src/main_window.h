#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QWidget>
#include <vector>
#include "maze_types.h"
#include "maze_solver.h"

class MazeWidget;
class QSpinBox;
class QComboBox;
class QPushButton;
class QSlider;
class QLabel;

/*
 * MainWindow：程序主窗口 = 左侧控制面板 + 右侧迷宫画布（MazeWidget）。
 *
 * 控制面板分四组：
 *   1. 迷宫设置：宽、高、生成算法、"生成新迷宫"按钮
 *   2. 求解算法：BFS / DFS递归 / DFS非递归（切换后在"同一个迷宫"上重新求解，方便对比）
 *   3. 动画控制：速度滑块、播放/暂停、单步、重置
 *   4. 统计信息：已访问格数、路径长度、算法耗时
 *
 * 迷宫数据和求解结果保存在这里，MazeWidget 只拿一份副本去画。
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QWidget* buildControlPanel();          // 创建左侧所有控件
    void regenerateMaze();                 // 按当前设置生成新迷宫，然后求解
    void solveCurrentMaze();               // 用当前算法求解当前迷宫（不重新生成）
    void onCellClicked(Point cell, Qt::MouseButton button);  // 左键设起点，右键设终点
    void onSpeedChanged(int sliderValue);
    void updatePlayButton();
    void updateStats();
    void updateRecursiveDfsAvailability(); // 迷宫太大时禁用递归 DFS，防止栈溢出崩溃
    SolveAlgorithm currentSolveAlgorithm() const;
    static QString algorithmName(SolveAlgorithm algo);

    // 递归 DFS 允许的最大格子数。
    // 递归深度最坏等于格子总数，Windows 默认线程栈只有 1MB，Debug 模式每层递归大约占几百字节，
    // 50×50 = 2500 格以内是安全的；再大就可能栈溢出直接闪退（C++ 里栈溢出无法用 try/catch 捕获）。
    static constexpr int kMaxCellsForRecursiveDfs = 2500;

    MazeWidget* mazeWidget_ = nullptr;

    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QComboBox* genAlgoCombo_ = nullptr;
    QPushButton* generateButton_ = nullptr;

    QComboBox* solveAlgoCombo_ = nullptr;

    QSlider* speedSlider_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* stepButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;

    QLabel* statsLabel_ = nullptr;
    QLabel* noticeLabel_ = nullptr;

    MazeGrid maze_;
    Point start_{ 0, 0 };
    Point end_{ 0, 0 };
    std::vector<Point> path_;
    std::vector<Point> visitedOrder_;
    std::vector<Point> parentOf_;
    double solveMicros_ = 0.0;  // 最近一次求解算法本身的耗时（微秒），不含动画
};

#endif
