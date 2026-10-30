#ifndef GAME_WIDGET_H
#define GAME_WIDGET_H

#include <QElapsedTimer>
#include <QPointF>
#include <QWidget>
#include <vector>
#include "maze_types.h"

class QTimer;
class QPainter;
class QKeyEvent;

/*
 * GameWidget："闯关冒险"模式的游戏画布。
 *
 * 玩家用 方向键 / WASD 控制小探险家在迷宫里走，目标是右下角的宝箱。
 *   - 画面：草地棋盘格 + 树篱墙，起点插一面小旗，终点放一个会闪光的宝箱
 *   - 角色：完全用 QPainter 画出来的原创小探险家（探险帽、大眼睛、腮红），走路会弹跳、会转身
 *   - 撞墙时角色会往墙的方向轻轻抖一下，不计步数
 *   - 走过的格子留下脚印；到达宝箱后撒花
 *   - 载入关卡时用 BFS 预先算好最短路线，通关后可以叠加显示（金色虚线），和自己走的路对比
 *
 * 所有动画都由一个约 60fps 的定时器驱动（onFrame），按真实经过的时间推进，不受卡顿影响。
 */
class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    // 载入一关：迷宫、起点、宝箱位置。会重新计算 BFS 最短路线
    void loadLevel(const MazeGrid& maze, Point start, Point goal);
    void restart();                          // 同一关从头再来

    void setShowShortestPath(bool show);     // 是否叠加显示 BFS 最短路线（金色虚线）
    bool showShortestPath() const { return showShortest_; }

    int steps() const { return steps_; }
    int shortestSteps() const;               // 最短路线的步数（格子数 - 1）
    double elapsedSeconds() const;           // 从第一步开始计时，通关后停表
    bool hasWon() const { return won_; }

signals:
    void stepsChanged(int steps);
    void reachedGoal(int steps, int shortestSteps, double seconds);
    void restartRequested();                 // 玩家按了 R 键

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct Confetti
    {
        QPointF pos;
        QPointF vel;
        double angle;
        double spin;
        QColor color;
    };

    void onFrame();                          // 每帧推进动画
    void tryMove(Direction dir);             // 尝试朝某个方向走一步（撞墙就抖一下）
    void startConfetti();
    void updateGeometryForMaze();

    QPointF cellCenterF(double row, double col) const;   // 支持小数坐标，用于平滑移动
    void drawGround(QPainter& p);
    void drawFootprints(QPainter& p);
    void drawShortestPath(QPainter& p);
    void drawHedges(QPainter& p);
    void drawFlag(QPainter& p, Point cell);
    void drawChest(QPainter& p, Point cell);
    void drawExplorer(QPainter& p);
    void drawConfetti(QPainter& p);

    MazeGrid maze_;
    Point start_{ 0, 0 };
    Point goal_{ 0, 0 };
    Point player_{ 0, 0 };                   // 逻辑位置（格子坐标）
    std::vector<Point> shortest_;            // BFS 最短路线
    std::vector<std::vector<char>> visited_; // 走过的格子（画脚印）
    bool showShortest_ = false;

    int steps_ = 0;
    bool won_ = false;
    QElapsedTimer playClock_;                // 计时：第一步开始
    double finalSeconds_ = 0.0;

    // ---- 动画状态 ----
    QTimer* frameTimer_ = nullptr;
    QElapsedTimer animClock_;
    qint64 lastFrameMs_ = 0;
    double time_ = 0.0;                      // 动画总时间（秒），用于呼吸、闪光等循环效果

    Point moveFrom_{ 0, 0 };
    double moveT_ = 1.0;                     // 0→1：从 moveFrom_ 走到 player_ 的进度
    int pendingDir_ = -1;                    // 移动中按下的下一个方向（缓冲一次，连按更顺滑）
    bool facingLeft_ = false;

    Direction bumpDir_ = Direction::Up;
    double bumpT_ = 1.0;                     // 0→1：撞墙抖动进度

    std::vector<Confetti> confetti_;
    double confettiLeft_ = 0.0;              // 撒花剩余时间（秒）

    int cellSize_ = 40;
    int offsetX_ = 16;
    int offsetY_ = 16;
    static constexpr int kMargin = 16;
    static constexpr int kMaxCanvas = 600;
    static constexpr double kMoveDuration = 0.11;  // 走一格用时（秒）
};

#endif
