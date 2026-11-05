#ifndef GEN_COMPARE_DIALOG_H
#define GEN_COMPARE_DIALOG_H

#include <QDialog>
#include <vector>
#include "maze_generator.h"

class MazeWidget;
class QLabel;
class QPushButton;
class QSlider;
class QTableWidget;

/*
 * GenCompareDialog："生成算法对比"窗口（课程额外要求：visually compare different generation algorithms）
 *   - 左栏：Kruskal 和 Prim 两个迷宫并排，同步播放"逐面拆墙"的生成过程；生成完显示 BFS 最短路线
 *   - 右栏：每种算法各生成 20~100 个随机迷宫取平均，比较 7 项指标，并自动写结论
 * 生成的都是完美迷宫（loopRate = 0）。
 */
class GenCompareDialog : public QDialog
{
    Q_OBJECT

public:
    GenCompareDialog(int width, int height, QWidget* parent = nullptr);

private:
    // 一个迷宫的形状指标
    struct MazeStats
    {
        double genMicros = 0;     // 生成耗时（微秒）
        int wallsRemoved = 0;     // 拆墙次数
        int deadEnds = 0;         // 死胡同：只有 1 个出口的格子
        int junctions = 0;        // 岔路口：有 3~4 个出口的格子
        double straightRatio = 0; // 直道格占比：恰好两个出口且是对面（上下或左右）的格子
        int pathLength = 0;       // 左上角到右下角的最短路径（格子数）
        int pathTurns = 0;        // 这条路径拐弯的次数
    };

    // 每种算法一栏：算法、名字、平均指标、画面控件
    struct AlgoColumn
    {
        GenAlgorithm algo;
        QString name;
        MazeStats avg;
        MazeWidget* view = nullptr;
        QLabel* caption = nullptr;
    };

    void newSample();                     // 重新生成两个用于动画展示的样本迷宫
    void runStatistics();                 // 各生成 samples_count_ 个迷宫，统计平均指标
    void buildTable();                    // 把平均指标填进表格
    QString buildSummary() const;         // 根据统计结果自动写结论
    void updateButtons();                 // 根据播放状态更新按钮文字
    static MazeStats measure(const MazeGrid& maze);  // 计算一个迷宫的各项指标

    int samples_count_ = 30;   // 统计用的迷宫数，构造函数里按大小重新计算

    int width_;
    int height_;
    std::vector<AlgoColumn> columns_;
    std::vector<MazeGrid> samples_;                   // 动画展示用的样本迷宫（与 columns_ 一一对应）
    std::vector<std::vector<WallBreak>> sampleSteps_; // 样本迷宫的拆墙顺序

    QPushButton* playButton_ = nullptr;
    QPushButton* skipButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QSlider* speedSlider_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
};

#endif