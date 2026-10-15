#include "gui.h"
#include <QPainter>
#include <QMouseEvent>
#include <QString>
#include <algorithm>

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
    maze_ = maze;
    path_.clear();
    visitedOrder_.clear();
    parentOf_.clear();
    animationStep_ = 0;
    hasEndpoints_ = false;
    updateGeometryForMaze();
    emit stepChanged(0, 0);
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

// ---------------- 播放控制 ----------------

bool MazeWidget::isPlaying() const { return timer_->isActive(); }

bool MazeWidget::isFinished() const
{
    return !visitedOrder_.empty() && animationStep_ >= totalSteps();
}

void MazeWidget::play()
{
    if (visitedOrder_.empty()) return;
    if (isFinished())
    {
        animationStep_ = 0;  // 播完了再点播放：从头重播
        emit stepChanged(0, totalSteps());
    }
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

void MazeWidget::resetAnimation()
{
    pause();
    animationStep_ = 0;
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

    animationStep_ = std::min(animationStep_ + count, total);
    emit stepChanged(animationStep_, total);

    if (animationStep_ >= total)
    {
        pause();
        emit finished();
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
    painter.fillRect(rect(), QColor(250, 248, 240));
    if (maze_.empty()) return;

    const int rows = static_cast<int>(maze_.size());
    const int cols = static_cast<int>(maze_[0].size());
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
        const QColor color = isCurrent ? QColor(70, 130, 180) : QColor(173, 216, 230);
        painter.fillRect(offsetX_ + p.col * cellSize_ + inset, offsetY_ + p.row * cellSize_ + inset,
                         cellSize_ - 2 * inset, cellSize_ - 2 * inset, color);
    }

    // 第二层：父节点连线（格子太小时线会糊成一片，就不画了）
    if (cellSize_ >= 8)
    {
        painter.setPen(QPen(QColor(180, 170, 150), 1));
        for (int i = 1; i < shown && i < static_cast<int>(parentOf_.size()); ++i)
        {
            const Point& parent = parentOf_[i];
            if (parent.row < 0 || parent.col < 0) continue;  // 起点没有父节点
            painter.drawLine(cellCenter(parent), cellCenter(visitedOrder_[i]));
        }
    }

    // 第三层：墙壁
    painter.setPen(QPen(QColor(50, 50, 50), wallWidth));
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const int x = offsetX_ + col * cellSize_;
            const int y = offsetY_ + row * cellSize_;
            const Cell& cell = maze_[row][col];
            if (cell.wallTop)    painter.drawLine(x, y, x + cellSize_, y);
            if (cell.wallLeft)   painter.drawLine(x, y, x, y + cellSize_);
            if (cell.wallRight)  painter.drawLine(x + cellSize_, y, x + cellSize_, y + cellSize_);
            if (cell.wallBottom) painter.drawLine(x, y + cellSize_, x + cellSize_, y + cellSize_);
        }
    }

    // 第四层：最终路径（动画播完才显示）
    if (isFinished() && path_.size() > 1)
    {
        QPen pathPen(QColor(220, 20, 60), pathWidth);
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
        painter.setBrush(QColor(34, 139, 34));
        painter.drawEllipse(cellCenter(start_), dotRadius, dotRadius);
        painter.setBrush(QColor(178, 34, 34));
        painter.drawEllipse(cellCenter(end_), dotRadius, dotRadius);
    }

    // ---- 图例：画在迷宫下方 ----
    const int legendTop = offsetY_ + rows * cellSize_ + 12;
    const int legendX = kMargin;
    const int rowH = 20;

    auto drawLegendItem = [&](int row, int col, QColor color, const QString& label, bool isLine = false)
        {
            const int x = legendX + col * 160;
            const int y = legendTop + row * rowH;
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
        };

    drawLegendItem(0, 0, QColor(34, 139, 34), "起点（左键设置）");
    drawLegendItem(0, 1, QColor(178, 34, 34), "终点（右键设置）");
    drawLegendItem(0, 2, QColor(70, 130, 180), "当前探索格");
    drawLegendItem(1, 0, QColor(173, 216, 230), "已访问格子");
    drawLegendItem(1, 1, QColor(180, 170, 150), "父节点连线", true);
    drawLegendItem(1, 2, QColor(220, 20, 60), "最终路径", true);
}
