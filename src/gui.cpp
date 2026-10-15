#include "gui.h"
#include <QPainter>
#include <QMouseEvent>
#include <QString>
#include <algorithm>

namespace
{
    // 配色：沿用原来图例的颜色，生成阶段另加两种
    const QColor kBackground(250, 248, 240);
    const QColor kWall(50, 50, 50);
    const QColor kStart(34, 139, 34);
    const QColor kEnd(178, 34, 34);
    const QColor kCurrent(70, 130, 180);
    const QColor kVisited(173, 216, 230);
    const QColor kParentLine(180, 170, 150);
    const QColor kPath(220, 20, 60);
    const QColor kCarved(236, 222, 190);     // 生成阶段：已经和别的格子打通的格子
    const QColor kJustBroken(255, 165, 0);   // 生成阶段：本步刚拆墙连通的两个格子
}

MazeWidget::MazeWidget(QWidget* parent) : QWidget(parent)
{
    timer_ = new QTimer(this);
    timer_->setInterval(250);  // 默认每秒 4 步，MainWindow 会用速度滑块覆盖
    connect(timer_, &QTimer::timeout, this, [this]() { advance(stepsPerTick_); });
}

// ---------------- 数据设置 ----------------

void MazeWidget::setMaze(const MazeGrid& maze)
{
    pause();
    phase_ = Phase::Solving;
    maze_ = maze;
    genGrid_.clear();
    genSteps_.clear();
    carved_.clear();
    path_.clear();
    visitedOrder_.clear();
    parentOf_.clear();
    animationStep_ = 0;
    hasEndpoints_ = false;
    updateGeometryForMaze();
    emit stepChanged(0, 0);
    update();
}

void MazeWidget::setGeneration(const MazeGrid& finalMaze, const std::vector<WallBreak>& steps)
{
    setMaze(finalMaze);          // 先按普通方式载入（清空旧数据、算好格子大小）
    phase_ = Phase::Generating;  // 再切换到生成阶段
    genSteps_ = steps;
    rebuildGenerationGrid();
    emit stepChanged(0, totalSteps());
    update();
}

void MazeWidget::setEndpoints(Point start, Point end)
{
    start_ = start;
    end_ = end;
    hasEndpoints_ = true;
    update();
}

void MazeWidget::setSolution(const std::vector<Point>& path,
    const std::vector<Point>& visitedOrder, const std::vector<Point>& parentOf)
{
    pause();
    phase_ = Phase::Solving;  // 无论之前在哪个阶段，收到求解结果就进入求解阶段
    path_ = path;
    visitedOrder_ = visitedOrder;
    parentOf_ = parentOf;
    animationStep_ = 0;
    emit stepChanged(0, totalSteps());
    update();
}

// 根据迷宫大小自动选择格子像素：迷宫越大格子越小，保证整个迷宫不超过 kMaxCanvas 像素
void MazeWidget::updateGeometryForMaze()
{
    const int rows = static_cast<int>(maze_.size());
    const int cols = rows > 0 ? static_cast<int>(maze_[0].size()) : 0;
    if (rows == 0 || cols == 0)
    {
        setFixedSize(kMinWidth, 2 * kMargin + kLegendHeight);
        return;
    }

    cellSize_ = std::clamp(kMaxCanvas / std::max(rows, cols), kMinCellSize, kMaxCellSize);

    const int mazeW = cols * cellSize_;
    const int mazeH = rows * cellSize_;
    const int widgetW = std::max(mazeW + 2 * kMargin, kMinWidth);
    offsetX_ = (widgetW - mazeW) / 2;  // 迷宫比图例窄时水平居中
    offsetY_ = kMargin;
    setFixedSize(widgetW, mazeH + 2 * kMargin + kLegendHeight);
}

// ---------------- 生成阶段：拆墙回放 ----------------

void MazeWidget::rebuildGenerationGrid()
{
    const std::size_t rows = maze_.size();
    const std::size_t cols = rows > 0 ? maze_[0].size() : 0;
    genGrid_.assign(rows, std::vector<Cell>(cols));  // Cell 默认四面都是墙
    carved_.assign(rows, std::vector<char>(cols, 0));
}

// 和 A 的 removeWall 规则一致：from 格子朝 direction 方向的墙打开，相邻格子对应的墙也打开
void MazeWidget::applyWallBreak(const WallBreak& step)
{
    const int rows = static_cast<int>(genGrid_.size());
    if (rows == 0) return;
    const int cols = static_cast<int>(genGrid_[0].size());

    Point a = step.from;
    Point b = a;
    switch (step.direction)
    {
    case Direction::Up:    b.row -= 1; break;
    case Direction::Right: b.col += 1; break;
    case Direction::Down:  b.row += 1; break;
    case Direction::Left:  b.col -= 1; break;
    }
    // 防御性检查：记录不合法就跳过，不让越界访问搞崩界面
    if (a.row < 0 || a.row >= rows || a.col < 0 || a.col >= cols) return;
    if (b.row < 0 || b.row >= rows || b.col < 0 || b.col >= cols) return;

    Cell& ca = genGrid_[a.row][a.col];
    Cell& cb = genGrid_[b.row][b.col];
    switch (step.direction)
    {
    case Direction::Up:    ca.wallTop = false;    cb.wallBottom = false; break;
    case Direction::Right: ca.wallRight = false;  cb.wallLeft = false;   break;
    case Direction::Down:  ca.wallBottom = false; cb.wallTop = false;    break;
    case Direction::Left:  ca.wallLeft = false;   cb.wallRight = false;  break;
    }
    carved_[a.row][a.col] = 1;
    carved_[b.row][b.col] = 1;
}

// ---------------- 播放控制 ----------------

int MazeWidget::totalSteps() const
{
    return static_cast<int>(phase_ == Phase::Generating ? genSteps_.size() : visitedOrder_.size());
}

bool MazeWidget::isPlaying() const { return timer_->isActive(); }

bool MazeWidget::isFinished() const
{
    return totalSteps() > 0 && animationStep_ >= totalSteps();
}

void MazeWidget::play()
{
    if (totalSteps() == 0) return;
    if (isFinished())
        resetAnimation();  // 播完了再点播放：从头重播
    if (!timer_->isActive())
    {
        timer_->start();
        emit playingChanged(true);
    }
}

void MazeWidget::pause()
{
    if (timer_->isActive())
    {
        timer_->stop();
        emit playingChanged(false);
    }
}

void MazeWidget::stepOnce()
{
    pause();
    if (!isFinished()) advance(1);
}

void MazeWidget::skipToEnd()
{
    pause();
    if (!isFinished()) advance(totalSteps() - animationStep_);
}

void MazeWidget::resetAnimation()
{
    pause();
    animationStep_ = 0;
    if (phase_ == Phase::Generating) rebuildGenerationGrid();
    emit stepChanged(0, totalSteps());
    update();
}

// 速度很快时（比如每秒 500 步），定时器间隔不能设成 2ms（Windows 下定时器精度大约 15ms），
// 所以改成"间隔不小于 16ms，每次触发走多步"
void MazeWidget::setStepsPerSecond(int steps)
{
    steps = std::clamp(steps, 1, 2000);
    const int interval = std::max(16, 1000 / steps);
    stepsPerTick_ = std::max(1, (steps * interval + 500) / 1000);
    timer_->setInterval(interval);
}

void MazeWidget::advance(int count)
{
    const int total = totalSteps();
    if (animationStep_ >= total) return;

    const int target = std::min(animationStep_ + count, total);
    if (phase_ == Phase::Generating)
    {
        for (int i = animationStep_; i < target; ++i)
            applyWallBreak(genSteps_[i]);
    }
    animationStep_ = target;
    emit stepChanged(animationStep_, total);

    if (animationStep_ >= total)
    {
        pause();
        emit finished();  // 注意：生成阶段播完时，MainWindow 收到这个信号会调用 setSolution 切到求解阶段
    }
    update();  // 请求重绘，Qt 会在合适的时候调用 paintEvent
}

// ---------------- 坐标换算 ----------------

QPoint MazeWidget::cellCenter(Point p) const
{
    return QPoint(offsetX_ + p.col * cellSize_ + cellSize_ / 2,
                  offsetY_ + p.row * cellSize_ + cellSize_ / 2);
}

bool MazeWidget::cellAt(const QPoint& pos, Point& out) const
{
    if (maze_.empty()) return false;
    const int rows = static_cast<int>(maze_.size());
    const int cols = static_cast<int>(maze_[0].size());
    const int x = pos.x() - offsetX_;
    const int y = pos.y() - offsetY_;
    if (x < 0 || y < 0) return false;
    const int col = x / cellSize_;
    const int row = y / cellSize_;
    if (row >= rows || col >= cols) return false;
    out = { row, col };
    return true;
}

void MazeWidget::mousePressEvent(QMouseEvent* event)
{
    Point cell;
    if (cellAt(event->pos(), cell))
        emit cellClicked(cell, event->button());
}

// ---------------- 绘制 ----------------

void MazeWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), kBackground);
    if (maze_.empty()) return;

    if (phase_ == Phase::Generating) paintGeneration(painter);
    else paintSolving(painter);
}

void MazeWidget::paintWalls(QPainter& painter, const MazeGrid& grid, int wallWidth)
{
    painter.setPen(QPen(kWall, wallWidth));
    for (int row = 0; row < static_cast<int>(grid.size()); ++row)
    {
        for (int col = 0; col < static_cast<int>(grid[row].size()); ++col)
        {
            const int x = offsetX_ + col * cellSize_;
            const int y = offsetY_ + row * cellSize_;
            const Cell& cell = grid[row][col];
            if (cell.wallTop)    painter.drawLine(x, y, x + cellSize_, y);
            if (cell.wallLeft)   painter.drawLine(x, y, x, y + cellSize_);
            if (cell.wallRight)  painter.drawLine(x + cellSize_, y, x + cellSize_, y + cellSize_);
            if (cell.wallBottom) painter.drawLine(x, y + cellSize_, x + cellSize_, y + cellSize_);
        }
    }
}

void MazeWidget::drawLegendItem(QPainter& painter, int row, int col, QColor color, const QString& label, bool isLine)
{
    const int legendTop = offsetY_ + static_cast<int>(maze_.size()) * cellSize_ + 12;
    const int x = kMargin + col * 160;
    const int y = legendTop + row * 20;
    if (isLine)
    {
        painter.setPen(QPen(color, 3));
        painter.drawLine(x, y + 7, x + 16, y + 7);
    }
    else
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRect(x, y, 14, 14);
    }
    painter.setPen(Qt::black);
    painter.drawText(x + 22, y + 12, label);
}

// 生成阶段：从全墙网格开始，已打通的格子填浅色，本步刚拆的墙两侧格子填橙色
void MazeWidget::paintGeneration(QPainter& painter)
{
    const int inset = std::max(1, cellSize_ / 15);
    const int wallWidth = cellSize_ >= 12 ? 2 : 1;

    for (int row = 0; row < static_cast<int>(carved_.size()); ++row)
        for (int col = 0; col < static_cast<int>(carved_[row].size()); ++col)
            if (carved_[row][col])
                painter.fillRect(offsetX_ + col * cellSize_ + inset, offsetY_ + row * cellSize_ + inset,
                                 cellSize_ - 2 * inset, cellSize_ - 2 * inset, kCarved);

    // 高亮最近一步拆墙连通的两个格子（播完之后就不再高亮）
    if (animationStep_ > 0 && !isFinished())
    {
        const WallBreak& last = genSteps_[animationStep_ - 1];
        Point b = last.from;
        switch (last.direction)
        {
        case Direction::Up:    b.row -= 1; break;
        case Direction::Right: b.col += 1; break;
        case Direction::Down:  b.row += 1; break;
        case Direction::Left:  b.col -= 1; break;
        }
        for (const Point& p : { last.from, b })
            painter.fillRect(offsetX_ + p.col * cellSize_ + inset, offsetY_ + p.row * cellSize_ + inset,
                             cellSize_ - 2 * inset, cellSize_ - 2 * inset, kJustBroken);
    }

    paintWalls(painter, genGrid_, wallWidth);

    drawLegendItem(painter, 0, 0, kCarved, "已打通的格子", false);
    drawLegendItem(painter, 0, 1, kJustBroken, "本步拆墙连通", false);
    drawLegendItem(painter, 0, 2, kWall, "墙壁", true);
}

void MazeWidget::paintSolving(QPainter& painter)
{
    // 线宽、内缩随格子大小缩放，大迷宫格子很小时也能看清
    const int inset = std::max(1, cellSize_ / 15);
    const int wallWidth = cellSize_ >= 12 ? 2 : 1;
    const int pathWidth = std::max(2, cellSize_ / 7);
    const int dotRadius = std::max(2, cellSize_ / 4);

    // 第一层：已访问格子填色（当前正在探索的格子用深蓝）
    const int shown = std::min(animationStep_, totalSteps());
    for (int i = 0; i < shown; ++i)
    {
        const Point& p = visitedOrder_[i];
        const bool isCurrent = (i == animationStep_ - 1) && !isFinished();
        painter.fillRect(offsetX_ + p.col * cellSize_ + inset, offsetY_ + p.row * cellSize_ + inset,
                         cellSize_ - 2 * inset, cellSize_ - 2 * inset, isCurrent ? kCurrent : kVisited);
    }

    // 第二层：父节点连线（格子太小时线会糊成一片，就不画了）
    if (cellSize_ >= 8)
    {
        painter.setPen(QPen(kParentLine, 1));
        for (int i = 1; i < shown && i < static_cast<int>(parentOf_.size()); ++i)
        {
            const Point& parent = parentOf_[i];
            if (parent.row < 0 || parent.col < 0) continue;  // 起点没有父节点
            painter.drawLine(cellCenter(parent), cellCenter(visitedOrder_[i]));
        }
    }

    // 第三层：墙壁
    paintWalls(painter, maze_, wallWidth);

    // 第四层：最终路径（动画播完才显示）
    if (isFinished() && path_.size() > 1)
    {
        QPen pathPen(kPath, pathWidth);
        pathPen.setCapStyle(Qt::RoundCap);
        pathPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pathPen);
        for (std::size_t i = 0; i + 1 < path_.size(); ++i)
            painter.drawLine(cellCenter(path_[i]), cellCenter(path_[i + 1]));
    }

    // 第五层：起点终点圆点
    if (hasEndpoints_)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(kStart);
        painter.drawEllipse(cellCenter(start_), dotRadius, dotRadius);
        painter.setBrush(kEnd);
        painter.drawEllipse(cellCenter(end_), dotRadius, dotRadius);
    }

    // ---- 图例：画在迷宫下方 ----
    drawLegendItem(painter, 0, 0, kStart, "起点（左键设置）", false);
    drawLegendItem(painter, 0, 1, kEnd, "终点（右键设置）", false);
    drawLegendItem(painter, 0, 2, kCurrent, "当前探索格", false);
    drawLegendItem(painter, 1, 0, kVisited, "已访问格子", false);
    drawLegendItem(painter, 1, 1, kParentLine, "父节点连线", true);
    drawLegendItem(painter, 1, 2, kPath, "最终路径", true);
}
