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
 * GenCompareDialog："生成算法对比"窗口（对应课程额外要求：visually compare different generation algorithms）。
 *
 * 同样大小的迷宫，分别用 Kruskal（并查集）和 Prim（堆优化）生成：
 *   - 上半部分：两个迷宫并排，同步播放"逐面拆墙"的生成过程。
 *     Kruskal 是在整张图上随机挑墙、一块块合并；Prim 是从一个点向外生长 —— 动画里一眼能看出区别。
 *     生成完后自动显示起点到终点的 BFS 最短路线和热力图，直观比较两种迷宫的"形状"。
 *   - 下半部分：统计表。每种算法各生成 20~100 个随机迷宫（按大小自动决定）取平均，比较：
 *     生成耗时、拆墙次数、死胡同数、岔路口数、直道格占比、起点到终点的路径长度、路径拐弯次数。
 *
 * 生成的都是完美迷宫（loopRate = 0），符合课程基本要求。
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
        double genMicros = 0;   // 生成耗时（微秒）
        int wallsRemoved = 0;   // 拆墙次数
        int deadEnds = 0;       // 死胡同：只有 1 个出口的格子
        int junctions = 0;      // 岔路口：有 3~4 个出口的格子
        double straightRatio = 0; // 直道格占比：恰好两个出口且是对面（上下或左右）的格子
        int pathLength = 0;     // 左上角到右下角的最短路径（格子数）
        int pathTurns = 0;      // 这条路径拐弯的次数
    };

    struct AlgoColumn
    {
        GenAlgorithm algo;
        QString name;
        MazeStats avg;          // 多个迷宫的平均值
        MazeWidget* view = nullptr;
        QLabel* caption = nullptr;
    };

    void newSample();                     // 重新生成两个用于动画展示的样本迷宫
    void runStatistics();                 // 各生成 kSamples 个迷宫，统计平均指标
    void buildTable();
    QString buildSummary() const;
    void updateButtons();
    static MazeStats measure(const MazeGrid& maze);

    int samples_count_ = 30;   // 统计用的迷宫数：小迷宫多取一些（最多 100），大迷宫少取（至少 20），保证窗口打开不卡

    int width_;
    int height_;
    std::vector<AlgoColumn> columns_;
    std::vector<MazeGrid> samples_;                  // 动画展示用的样本迷宫（与 columns_ 一一对应）
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
