#ifndef GUI_H
#define GUI_H

#include <QWidget>
#include <QTimer>
#include <vector>
#include "maze_types.h"

class QMouseEvent;
class QPainter;
class QVariantAnimation;

/*
 * MazeWidget：只负责两件事 ——"画迷宫"和"播放求解动画"。
 *
 * 界面上的按钮、下拉框、滑块都不放在这里，而是统一放在 MainWindow（main_window.h）里，
 * MainWindow 通过下面这些公开函数来控制它，再通过信号得知动画进度。
 * 这样分工的好处：画图逻辑和控件逻辑互不干扰，以后加"生成动画"、"对比视图"也只需要在各自的类里改。
 */
class MazeWidget : public QWidget
{
    Q_OBJECT  // 这个宏是Qt信号槽机制必需的，缺了会导致MOC编译报错

public:
    // 动画分两个阶段：先播放"迷宫生成"（可选），再播放"迷宫求解"。
    // 播放/暂停/单步/重置/跳到结尾 这些控制对当前所处的阶段生效。
    enum class Phase
    {
        Generating,  // 从四面全是墙的网格开始，按生成算法记录的顺序逐面拆墙
        Solving      // 迷宫已完整，播放 BFS/DFS 的探索过程
    };

    explicit MazeWidget(QWidget* parent = nullptr);

    // 换一个新迷宫（不播放生成过程）：直接进入求解阶段，清空旧的求解结果，并自动调整格子大小
    void setMaze(const MazeGrid& maze);

    // 换一个新迷宫，并准备播放它的生成过程：
    // finalMaze 是生成好的完整迷宫，steps 是生成算法依次拆掉的墙（来自 A 的 generateMazeWithSteps）。
    // 调用后画面显示四面全是墙的网格，处于暂停状态，等用户点"播放"。
    // 生成阶段播完后会发出 finished()，由 MainWindow 调用 setSolution 进入求解阶段。
    void setGeneration(const MazeGrid& finalMaze, const std::vector<WallBreak>& steps);

    Phase phase() const { return phase_; }

    // 紧凑模式：不画底部图例、不强制最小宽度，迷宫最大边长改为 maxCanvas 像素。
    // 用在算法对比窗口里，让几个小迷宫可以并排放下。需要在 setMaze 之前调用。
    void setCompact(bool compact, int maxCanvas = 340);

    // 设置起点、终点（画绿点和红点）。和求解结果分开，这样即使终点不可达也能画出来
    void setEndpoints(Point start, Point end);

    // 接收求解结果：进入求解阶段，动画回到第 0 步，处于暂停状态，等用户点"播放"
    void setSolution(const std::vector<Point>& path,
                     const std::vector<Point>& visitedOrder,
                     const std::vector<Point>& parentOf);

    // ---------- 播放控制（由 MainWindow 的按钮调用） ----------
    void play();                       // 开始/继续播放；如果已经播完，则从头重播
    void pause();                      // 暂停
    void stepOnce();                   // 暂停并只前进一步
    void resetAnimation();             // 暂停并回到当前阶段的第 0 步
    void skipToEnd();                  // 直接跳到当前阶段的最后一步
    void setStepsPerSecond(int steps); // 播放速度：每秒前进多少步

    bool isPlaying() const;
    bool isFinished() const;           // 当前阶段是否已经播放到最后一步
    int currentStep() const { return animationStep_; }
    int totalSteps() const;            // 当前阶段的总步数：生成阶段=拆墙次数，求解阶段=访问格数

signals:
    void stepChanged(int step, int total);  // 每前进（或重置）一次就发一次，MainWindow 用来刷新统计
    void playingChanged(bool playing);      // 播放/暂停状态改变，MainWindow 用来切换按钮文字
    void finished();                        // 播放到最后一步
    void cellClicked(Point cell, Qt::MouseButton button);  // 用户点了某个格子（左键/右键）

protected:
    void paintEvent(QPaintEvent* event) override;        // Qt会自动调用这个函数来绘制界面，回调机制
    void mousePressEvent(QMouseEvent* event) override;   // 鼠标点击：换算成格子坐标后发出 cellClicked

private:
    void advance(int count);                 // 动画前进 count 步
    void updateGeometryForMaze();            // 根据迷宫行列数计算格子大小和窗口大小
    void rebuildGenerationGrid();            // 生成阶段：恢复成四面全是墙的初始网格
    void applyWallBreak(const WallBreak& step);  // 生成阶段：执行一次拆墙（两侧格子的墙同时打开）
    void paintGeneration(QPainter& painter);
    void paintSolving(QPainter& painter);
    void paintWalls(QPainter& painter, const MazeGrid& grid, int wallWidth);
    void drawLegendItem(QPainter& painter, int row, int col, QColor color, const QString& label, bool isLine);
    void drawHeatLegend(QPainter& painter, int row, int col);   // 热力图色带图例
    void computeDepths();                    // 求解阶段：算出每个访问格离起点的步数，用于热力图上色
    void startFinishEffects();               // 求解播完：启动"路径描线"和"终点脉冲"动画
    void stopFinishEffects();                // 重置/重新求解时停止上述动画
    void drawPath(QPainter& painter, int pathWidth);  // 画（可能只画了一部分的）最终路径，带光晕
    QPoint cellCenter(Point p) const;        // 格子坐标 -> 屏幕像素坐标（格子中心）
    bool cellAt(const QPoint& pos, Point& out) const;  // 屏幕像素坐标 -> 格子坐标

    Phase phase_ = Phase::Solving;

    MazeGrid maze_;          // 完整的最终迷宫（求解阶段画的就是它）

    // ---- 生成阶段用的数据 ----
    MazeGrid genGrid_;                    // 正在被"拆墙"的网格：开始时四面全是墙，每步拆一面
    std::vector<WallBreak> genSteps_;     // 生成算法依次拆掉的墙
    std::vector<std::vector<char>> carved_;  // carved_[r][c]：这个格子是否已经和别的格子打通

    Point start_{ 0, 0 };
    Point end_{ 0, 0 };
    bool hasEndpoints_ = false;

    std::vector<Point> path_;
    std::vector<Point> visitedOrder_;
    std::vector<Point> parentOf_;

    // ---- 视觉效果 ----
    std::vector<int> depth_;   // depth_[i]：visitedOrder_[i] 离起点的步数（沿父节点链），热力图按它上色
    int maxDepth_ = 1;
    QVariantAnimation* pathAnim_ = nullptr;   // 最终路径从起点"画"到终点，pathProgress_ 从 0 到 1
    double pathProgress_ = 0.0;
    QVariantAnimation* pulseAnim_ = nullptr;  // 终点的扩散脉冲环，pulse_ 从 0 到 1 循环几次
    double pulse_ = -1.0;                     // <0 表示不画脉冲

    QTimer* timer_;          // 定时器指针，用来驱动动画
    int animationStep_ = 0;  // 当前动画播放到第几步
    int stepsPerTick_ = 1;   // 每次定时器触发前进几步（速度很快时一次走多步，避免定时器间隔太短）

    bool compact_ = false;           // 紧凑模式：不画图例
    int compactMaxCanvas_ = 340;     // 紧凑模式下迷宫最大边长

    int cellSize_ = 30;      // 每个格子的像素边长，setMaze 时自动计算
    int offsetX_ = 10;       // 迷宫左上角在控件中的位置
    int offsetY_ = 10;

    static constexpr int kMargin = 10;        // 迷宫四周留白
    static constexpr int kLegendHeight = 60;  // 底部图例预留高度
    static constexpr int kMaxCanvas = 600;    // 迷宫绘制区域最大边长（像素），超过就缩小格子
    static constexpr int kMaxCellSize = 48;   // 小迷宫格子放大，但不超过这个值
    static constexpr int kMinCellSize = 4;
    static constexpr int kMinWidth = 500;     // 控件最小宽度，保证三列图例能完整显示
};

#endif
