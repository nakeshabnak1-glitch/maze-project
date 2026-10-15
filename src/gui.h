#ifndef GUI_H
#define GUI_H

#include <QWidget>
#include <QTimer>
#include <vector>
#include "maze_types.h"

class QMouseEvent;

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
    explicit MazeWidget(QWidget* parent = nullptr);

    // 换一个新迷宫：会清空旧的求解结果，并根据迷宫大小自动调整格子像素大小
    void setMaze(const MazeGrid& maze);

    // 设置起点、终点（画绿点和红点）。和求解结果分开，这样即使终点不可达也能画出来
    void setEndpoints(Point start, Point end);

    // 接收求解结果：动画回到第 0 步，处于暂停状态，等用户点"播放"
    void setSolution(const std::vector<Point>& path,
                     const std::vector<Point>& visitedOrder,
                     const std::vector<Point>& parentOf);

    // ---------- 播放控制（由 MainWindow 的按钮调用） ----------
    void play();                       // 开始/继续播放；如果已经播完，则从头重播
    void pause();                      // 暂停
    void stepOnce();                   // 暂停并只前进一步
    void resetAnimation();             // 暂停并回到第 0 步
    void setStepsPerSecond(int steps); // 播放速度：每秒前进多少步

    bool isPlaying() const;
    bool isFinished() const;           // 是否已经播放到最后一步
    int currentStep() const { return animationStep_; }
    int totalSteps() const { return static_cast<int>(visitedOrder_.size()); }

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
    QPoint cellCenter(Point p) const;        // 格子坐标 -> 屏幕像素坐标（格子中心）
    bool cellAt(const QPoint& pos, Point& out) const;  // 屏幕像素坐标 -> 格子坐标

    MazeGrid maze_;
    Point start_{ 0, 0 };
    Point end_{ 0, 0 };
    bool hasEndpoints_ = false;

    std::vector<Point> path_;
    std::vector<Point> visitedOrder_;
    std::vector<Point> parentOf_;

    QTimer* timer_;          // 定时器指针，用来驱动动画
    int animationStep_ = 0;  // 当前动画播放到第几步
    int stepsPerTick_ = 1;   // 每次定时器触发前进几步（速度很快时一次走多步，避免定时器间隔太短）

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
