#include "gui.h"
#include "theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QVariantAnimation>
#include <QString>
#include <algorithm>
#include <cmath>

namespace
{
    // 两种颜色按 t∈[0,1] 混合：t=0 是 a，t=1 是 b（生成阶段的拖尾渐隐用）
    QColor blend(const QColor& a, const QColor& b, double t)
    {
        t = std::clamp(t, 0.0, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                                a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    }

    QColor withAlpha(QColor c, int alpha)
    {
        c.setAlpha(std::clamp(alpha, 0, 255));
        return c;
    }

    // 沿 direction 方向的相邻格子
    Point neighborOf(Point p, Direction d)
    {
        switch (d)
        {
        case Direction::Up:    --p.row; break;
        case Direction::Right: ++p.col; break;
        case Direction::Down:  ++p.row; break;
        case Direction::Left:  --p.col; break;
        }
        return p;
    }

    constexpr int kTrailLength = 16;  // 生成阶段：最近多少步拆墙保留"拖尾"高亮
}

MazeWidget::MazeWidget(QWidget* parent) : QWidget(parent)
{
    timer_ = new QTimer(this);
    timer_->setInterval(250);  // 默认每秒 4 步，MainWindow 会用速度滑块覆盖
    connect(timer_, &QTimer::timeout, this, [this]() { advance(stepsPerTick_); });

    // 路径描线动画：pathProgress_ 从 0 平滑变到 1，红线从起点一路"画"到终点
    pathAnim_ = new QVariantAnimation(this);
    pathAnim_->setStartValue(0.0);
    pathAnim_->setEndValue(1.0);
    pathAnim_->setEasingCurve(QEasingCurve::InOutQuad);
    connect(pathAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        pathProgress_ = v.toDouble();
        update();
    });

    // 终点脉冲：描线结束后，终点外圈扩散 3 次
    pulseAnim_ = new QVariantAnimation(this);
    pulseAnim_->setStartValue(0.0);
    pulseAnim_->setEndValue(1.0);
    pulseAnim_->setDuration(1100);
    pulseAnim_->setLoopCount(3);
    pulseAnim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(pulseAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        pulse_ = v.toDouble();
        update();
    });
    connect(pulseAnim_, &QVariantAnimation::finished, this, [this]() { pulse_ = -1.0; update(); });
    connect(pathAnim_, &QVariantAnimation::finished, pulseAnim_, [this]() { pulseAnim_->start(); });
}

// ---------------- 数据设置 ----------------

void MazeWidget::setMaze(const MazeGrid& maze)
{
    pause();
    stopFinishEffects();
    phase_ = Phase::Solving;
    maze_ = maze;
    genGrid_.clear();
    genSteps_.clear();
    carved_.clear();
    path_.clear();
    visitedOrder_.clear();
    parentOf_.clear();
    depth_.clear();
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
    stopFinishEffects();
    phase_ = Phase::Solving;  // 无论之前在哪个阶段，收到求解结果就进入求解阶段
    path_ = path;
    visitedOrder_ = visitedOrder;
    parentOf_ = parentOf;
    computeDepths();
    animationStep_ = 0;
    emit stepChanged(0, totalSteps());
    update();
}

// 热力图需要知道每个访问格"离起点几步"：沿父节点链往回数。
// visitedOrder_ 里父节点一定比子节点先出现，所以从前往后一遍就能算完：depth(子) = depth(父) + 1
void MazeWidget::computeDepths()
{
    depth_.assign(visitedOrder_.size(), 0);
    maxDepth_ = 1;
    if (maze_.empty()) return;
    const int cols = static_cast<int>(maze_[0].size());
    std::vector<int> depthOfCell(maze_.size() * cols, 0);

    for (std::size_t i = 0; i < visitedOrder_.size(); ++i)
    {
        const Point& p = visitedOrder_[i];
        const Point& parent = i < parentOf_.size() ? parentOf_[i] : Point{ -1, -1 };
        const int d = (parent.row < 0) ? 0 : depthOfCell[parent.row * cols + parent.col] + 1;
        depthOfCell[p.row * cols + p.col] = d;
        depth_[i] = d;
        maxDepth_ = std::max(maxDepth_, d);
    }
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

    const int maxCanvas = compact_ ? compactMaxCanvas_ : kMaxCanvas;
    cellSize_ = std::clamp(maxCanvas / std::max(rows, cols), kMinCellSize, kMaxCellSize);

    const int mazeW = cols * cellSize_;
    const int mazeH = rows * cellSize_;
    const int minWidth = compact_ ? 0 : kMinWidth;        // 紧凑模式没有图例，不需要最小宽度
    const int legendHeight = compact_ ? 0 : kLegendHeight;
    const int widgetW = std::max(mazeW + 2 * kMargin, minWidth);
    offsetX_ = (widgetW - mazeW) / 2;  // 迷宫比图例窄时水平居中
    offsetY_ = kMargin;
    setFixedSize(widgetW, mazeH + 2 * kMargin + legendHeight);
}

void MazeWidget::setCompact(bool compact, int maxCanvas)
{
    compact_ = compact;
    compactMaxCanvas_ = std::max(40, maxCanvas);
    updateGeometryForMaze();
    update();
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

    const Point a = step.from;
    const Point b = neighborOf(a, step.direction);
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
    stopFinishEffects();
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
        const bool solvingDone = (phase_ == Phase::Solving);
        pause();
        emit finished();  // 注意：生成阶段播完时，MainWindow 收到这个信号会调用 setSolution 切到求解阶段
        if (solvingDone && phase_ == Phase::Solving && isFinished())
            startFinishEffects();  // 求解播完：开始画路径
    }
    update();  // 请求重绘，Qt 会在合适的时候调用 paintEvent
}

// ---------------- 收尾动画：路径描线 + 终点脉冲 ----------------

void MazeWidget::startFinishEffects()
{
    stopFinishEffects();
    if (path_.size() < 2)
    {
        pathProgress_ = 1.0;
        return;
    }
    // 路径越长画得越久，但控制在 0.6~1.8 秒之间
    pathAnim_->setDuration(std::clamp(static_cast<int>(path_.size()) * 25, 600, 1800));
    pathAnim_->start();
}

void MazeWidget::stopFinishEffects()
{
    if (pathAnim_) pathAnim_->stop();
    if (pulseAnim_) pulseAnim_->stop();
    pathProgress_ = 0.0;
    pulse_ = -1.0;
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
    painter.fillRect(rect(), Theme::kCanvasBg);
    if (maze_.empty()) return;

    // 迷宫底板：比画布底色略亮一点的圆角矩形，让迷宫"浮"起来
    const int rows = static_cast<int>(maze_.size());
    const int cols = static_cast<int>(maze_[0].size());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#FFF1D6"));
    painter.drawRoundedRect(QRectF(offsetX_ - 4, offsetY_ - 4, cols * cellSize_ + 8, rows * cellSize_ + 8), 6, 6);

    if (phase_ == Phase::Generating) paintGeneration(painter);
    else paintSolving(painter);
}

void MazeWidget::paintWalls(QPainter& painter, const MazeGrid& grid, int wallWidth)
{
    QPen pen(Theme::kWall, wallWidth);
    pen.setCapStyle(Qt::RoundCap);  // 圆头端点：墙角接缝处更圆润
    painter.setPen(pen);
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
    const int legendTop = offsetY_ + static_cast<int>(maze_.size()) * cellSize_ + 14;
    const int x = kMargin + col * 160;
    const int y = legendTop + row * 20;
    if (isLine)
    {
        QPen pen(color, 3);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(x + 1, y + 7, x + 15, y + 7);
    }
    else
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(x, y, 14, 14), 3, 3);
    }
    painter.setPen(Theme::kTextMuted);
    painter.drawText(x + 22, y + 12, label);
}

// 热力图图例：一小段渐变色带
void MazeWidget::drawHeatLegend(QPainter& painter, int row, int col)
{
    const int legendTop = offsetY_ + static_cast<int>(maze_.size()) * cellSize_ + 14;
    const int x = kMargin + col * 160;
    const int y = legendTop + row * 20;
    QLinearGradient g(x, 0, x + 36, 0);
    for (int i = 0; i <= 4; ++i) g.setColorAt(i / 4.0, Theme::heatColor(i / 4.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(g);
    painter.drawRoundedRect(QRectF(x, y, 36, 14), 3, 3);
    painter.setPen(Theme::kTextMuted);
    painter.drawText(x + 44, y + 12, "离起点 近→远");
}

// 生成阶段：已打通的格子填深蓝灰；最新一步拆墙的两格是琥珀色，之前 kTrailLength 步留下一条渐隐的浅蓝"拖尾"
void MazeWidget::paintGeneration(QPainter& painter)
{
    const int inset = std::max(1, cellSize_ / 15);
    const int wallWidth = cellSize_ >= 12 ? 2 : 1;
    const double radius = cellSize_ >= 10 ? cellSize_ * 0.18 : 0.0;

    auto fillCell = [&](const Point& p, const QColor& c) {
        const QRectF r(offsetX_ + p.col * cellSize_ + inset, offsetY_ + p.row * cellSize_ + inset,
                       cellSize_ - 2 * inset, cellSize_ - 2 * inset);
        if (radius > 0) { painter.setPen(Qt::NoPen); painter.setBrush(c); painter.drawRoundedRect(r, radius, radius); }
        else painter.fillRect(r, c);
    };

    for (int row = 0; row < static_cast<int>(carved_.size()); ++row)
        for (int col = 0; col < static_cast<int>(carved_[row].size()); ++col)
            if (carved_[row][col]) fillCell({ row, col }, Theme::kCarved);

    // 拖尾：越早拆的墙颜色越接近"已打通"的底色，最新一步最亮（播完之后不再显示）
    if (animationStep_ > 0 && !isFinished())
    {
        const int from = std::max(0, animationStep_ - kTrailLength);
        // 最新一步用琥珀色；更早的从浅蓝渐隐到底色（琥珀直接混深蓝会变成泥褐色，不好看）
        const QColor trailHead("#FFD08A");
        for (int i = from; i < animationStep_; ++i)
        {
            const int age = animationStep_ - 1 - i;  // 0 = 最新
            const QColor c = (age == 0) ? Theme::kJustBroken
                                        : blend(trailHead, Theme::kCarved, static_cast<double>(age) / kTrailLength);
            fillCell(genSteps_[i].from, c);
            fillCell(neighborOf(genSteps_[i].from, genSteps_[i].direction), c);
        }
    }

    paintWalls(painter, genGrid_, wallWidth);

    if (compact_) return;
    drawLegendItem(painter, 0, 0, Theme::kCarved, "已打通的格子", false);
    drawLegendItem(painter, 0, 1, Theme::kJustBroken, "本步拆墙 / 拖尾", false);
    drawLegendItem(painter, 0, 2, Theme::kWall, "墙壁", true);
}

// 最终路径：三层叠画做出光晕（外层宽而淡、中层、最内层亮芯），描线动画进行中只画前一部分
void MazeWidget::drawPath(QPainter& painter, int pathWidth)
{
    if (path_.size() < 2 || pathProgress_ <= 0.0) return;

    // 按进度截取折线：完整的段 + 最后一段的一部分
    const double segments = static_cast<double>(path_.size() - 1);
    const double reach = pathProgress_ * segments;
    const int full = static_cast<int>(reach);
    QPolygonF line;
    for (int i = 0; i <= full && i < static_cast<int>(path_.size()); ++i)
        line << QPointF(cellCenter(path_[i]));
    if (full < static_cast<int>(path_.size()) - 1)
    {
        const QPointF a = cellCenter(path_[full]);
        const QPointF b = cellCenter(path_[full + 1]);
        line << a + (b - a) * (reach - full);
    }

    auto stroke = [&](const QColor& c, double width) {
        QPen pen(c, width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(line);
    };
    stroke(withAlpha(Theme::kEnd, 45), pathWidth * 3.4);   // 外层光晕
    stroke(withAlpha(Theme::kEnd, 110), pathWidth * 2.0);  // 中层
    stroke(Theme::kEnd, pathWidth);                         // 主体
    stroke(QColor("#FFE8E8"), std::max(1.0, pathWidth / 3.0));  // 亮芯

    // 描线进行中：笔尖处画一个亮点
    if (pathProgress_ < 1.0)
    {
        const QPointF tip = line.back();
        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(Qt::white, 90));
        painter.drawEllipse(tip, pathWidth * 2.2, pathWidth * 2.2);
        painter.setBrush(Qt::white);
        painter.drawEllipse(tip, pathWidth * 0.9, pathWidth * 0.9);
    }
}

void MazeWidget::paintSolving(QPainter& painter)
{
    // 线宽、内缩随格子大小缩放，大迷宫格子很小时也能看清
    const int inset = std::max(1, cellSize_ / 15);
    const int wallWidth = cellSize_ >= 12 ? 2 : 1;
    const int pathWidth = std::max(2, cellSize_ / 7);
    const int dotRadius = std::max(2, cellSize_ / 4);
    const double radius = cellSize_ >= 10 ? cellSize_ * 0.18 : 0.0;

    // 第一层：已访问格子按"离起点的步数"上热力图颜色；当前正在探索的格子用亮黄并带一圈光晕
    const int shown = std::min(animationStep_, totalSteps());
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < shown; ++i)
    {
        const Point& p = visitedOrder_[i];
        const QRectF r(offsetX_ + p.col * cellSize_ + inset, offsetY_ + p.row * cellSize_ + inset,
                       cellSize_ - 2 * inset, cellSize_ - 2 * inset);
        const QColor c = Theme::heatColor(static_cast<double>(depth_[i]) / maxDepth_);
        if (radius > 0) { painter.setBrush(c); painter.drawRoundedRect(r, radius, radius); }
        else painter.fillRect(r, c);
    }
    if (shown > 0 && !isFinished())
    {
        const Point& p = visitedOrder_[shown - 1];
        const QRectF r(offsetX_ + p.col * cellSize_, offsetY_ + p.row * cellSize_, cellSize_, cellSize_);
        painter.setBrush(withAlpha(Theme::kCurrent, 70));
        painter.drawRoundedRect(r.adjusted(-cellSize_ * 0.25, -cellSize_ * 0.25, cellSize_ * 0.25, cellSize_ * 0.25),
                                cellSize_ * 0.4, cellSize_ * 0.4);
        painter.setBrush(Theme::kCurrent);
        painter.drawRoundedRect(r.adjusted(inset, inset, -inset, -inset), radius, radius);
    }

    // 第二层：父节点连线（半透明白色；格子太小时线会糊成一片，就不画了）
    if (cellSize_ >= 8)
    {
        painter.setPen(QPen(Theme::kParentLine, 1));
        for (int i = 1; i < shown && i < static_cast<int>(parentOf_.size()); ++i)
        {
            const Point& parent = parentOf_[i];
            if (parent.row < 0 || parent.col < 0) continue;  // 起点没有父节点
            painter.drawLine(cellCenter(parent), cellCenter(visitedOrder_[i]));
        }
    }

    // 第三层：墙壁
    paintWalls(painter, maze_, wallWidth);

    // 第四层：最终路径（求解播完后，从起点"画"到终点）
    if (isFinished()) drawPath(painter, pathWidth);

    // 第五层：起点终点圆点（外圈深色描边，在任何底色上都清楚）；路径画完后终点扩散脉冲
    if (hasEndpoints_)
    {
        if (pulse_ >= 0.0)
        {
            const double r = dotRadius * (1.0 + 2.4 * pulse_);
            painter.setPen(QPen(withAlpha(Theme::kEnd, static_cast<int>(220 * (1.0 - pulse_))), 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QPointF(cellCenter(end_)), r, r);
        }
        painter.setPen(QPen(Theme::kCanvasBg, 2.0));
        painter.setBrush(Theme::kStart);
        painter.drawEllipse(QPointF(cellCenter(start_)), dotRadius, dotRadius);
        painter.setBrush(Theme::kEnd);
        painter.drawEllipse(QPointF(cellCenter(end_)), dotRadius, dotRadius);
    }

    // ---- 图例：画在迷宫下方（紧凑模式不画） ----
    if (compact_) return;
    drawLegendItem(painter, 0, 0, Theme::kStart, "起点（左键设置）", false);
    drawLegendItem(painter, 0, 1, Theme::kEnd, "终点（右键设置）", false);
    drawLegendItem(painter, 0, 2, Theme::kCurrent, "当前探索格", false);
    drawHeatLegend(painter, 1, 0);
    drawLegendItem(painter, 1, 1, QColor(139, 94, 60, 140), "父节点连线", true);
    drawLegendItem(painter, 1, 2, Theme::kEnd, "最终路径", true);
}
