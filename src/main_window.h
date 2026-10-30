#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QWidget>
#include <vector>
#include "maze_types.h"
#include "maze_generator.h"
#include "maze_solver.h"

class MazeWidget;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;

/*
 * MainWindow：程序主窗口 = 左侧控制面板 + 右侧迷宫画布（MazeWidget）。
 *
 * 控制面板分四组：
 *   1. 迷宫设置：宽、高、生成算法、"播放生成过程"勾选框、"生成新迷宫"按钮
 *   2. 求解算法：BFS / DFS递归 / DFS非递归（切换后在"同一个迷宫"上重新求解，方便对比），
 *      以及"求解算法对比"按钮（打开 CompareDialog，四个迷宫并排 + 数据表）
 *   3. 动画控制：速度滑块、播放/暂停、单步、跳到结尾、重置
 *   4. 统计信息：生成和求解两个阶段的进度、耗时、访问格数、路径长度
 *
 * 动画流程：生成新迷宫 →（勾选时）播放生成过程 → 生成播完自动载入求解结果 → 播放求解过程
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
    // 按当前设置生成新迷宫并求解。animate=true 时先进入"生成阶段"等待播放，
    // 否则直接进入求解阶段（程序启动时用 false，避免一打开就是一张全是墙的空网格）
    void regenerateMaze(bool animate);
    void solveCurrentMaze();               // 用当前算法求解当前迷宫（不重新生成）
    void pushSolutionToWidget();           // 把求解结果交给 MazeWidget，进入求解阶段
    void onAnimationFinished();            // 某个阶段播完：生成阶段播完就自动切到求解阶段
    void openCompareDialog();              // 打开"求解算法对比"窗口（四种求解算法，同一迷宫、同一起终点）
    void onCellClicked(Point cell, Qt::MouseButton button);  // 左键设起点，右键设终点
    void onSpeedChanged(int sliderValue);
    void updatePlayButton();
    void updateStats();
    void updateRecursiveDfsAvailability(); // 迷宫太大时禁用递归 BFS / DFS，防止栈溢出崩溃
    SolveAlgorithm currentSolveAlgorithm() const;
    static QString algorithmName(SolveAlgorithm algo);
    static QString algorithmName(GenAlgorithm algo);
    bool inGenerationPhase() const;

    // 递归版（BFS 递归、DFS 递归）允许的最大格子数。
    // 递归深度最坏等于格子总数，Windows 默认线程栈只有 1MB，Debug 模式每层递归大约占几百字节，
    // 50×50 = 2500 格以内是安全的；再大就可能栈溢出直接闪退（C++ 里栈溢出无法用 try/catch 捕获）。
    static constexpr int kMaxCellsForRecursiveDfs = 2500;

    MazeWidget* mazeWidget_ = nullptr;

    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QComboBox* genAlgoCombo_ = nullptr;
    QCheckBox* animateGenCheck_ = nullptr;
    QPushButton* generateButton_ = nullptr;

    QComboBox* solveAlgoCombo_ = nullptr;
    QPushButton* compareButton_ = nullptr;

    QSlider* speedSlider_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* stepButton_ = nullptr;
    QPushButton* skipButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;

    QLabel* statsLabel_ = nullptr;
    QLabel* noticeLabel_ = nullptr;

    MazeGrid maze_;
    GenAlgorithm genAlgo_ = GenAlgorithm::Kruskal;  // 当前迷宫是用哪个算法生成的
    int genStepCount_ = 0;      // 生成时拆墙的次数（= 格子数 - 1）
    double genMicros_ = 0.0;    // 生成算法本身的耗时（微秒）
    Point start_{ 0, 0 };
    Point end_{ 0, 0 };
    std::vector<Point> path_;
    std::vector<Point> visitedOrder_;
    std::vector<Point> parentOf_;
    double solveMicros_ = 0.0;  // 最近一次求解算法本身的耗时（微秒），不含动画
};

#endif
