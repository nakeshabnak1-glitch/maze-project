#include "game_widget.h"
#include "maze_solver.h"
#include "theme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    Point step(Point p, Direction d)
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

    bool samePoint(Point a, Point b) { return a.row == b.row && a.col == b.col; }

    // 画一个五角星（闪光、星星用）
    QPainterPath starPath(QPointF c, double outer, double inner)
    {
        QPainterPath path;
        for (int i = 0; i < 10; ++i)
        {
            const double r = (i % 2 == 0) ? outer : inner;
            const double a = -kPi / 2 + i * kPi / 5;
            const QPointF pt(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
            if (i == 0) path.moveTo(pt); else path.lineTo(pt);
        }
        path.closeSubpath();
        return path;
    }
}

GameWidget::GameWidget(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);  // 能接收键盘事件

    frameTimer_ = new QTimer(this);
    frameTimer_->setInterval(16);     // 约 60fps
    connect(frameTimer_, &QTimer::timeout, this, &GameWidget::onFrame);
    animClock_.start();
    frameTimer_->start();
}

// ---------------- 关卡 ----------------

void GameWidget::loadLevel(const MazeGrid& maze, Point start, Point goal)
{
    maze_ = maze;
    start_ = start;
    goal_ = goal;

    // 用 B 的求解模块算出最短路线：通关后给玩家对比
    std::vector<Point> visitedOrder, parentOf;
    solveMaze(maze_, start_, goal_, SolveAlgorithm::BFS, shortest_, visitedOrder, parentOf);

    updateGeometryForMaze();
    restart();
}

void GameWidget::restart()
{
    player_ = start_;
    moveFrom_ = start_;
    moveT_ = 1.0;
    bumpT_ = 1.0;
    pendingDir_ = -1;
    facingLeft_ = false;
    steps_ = 0;
    won_ = false;
    finalSeconds_ = 0.0;
    playClock_.invalidate();
    showShortest_ = false;
    confetti_.clear();
    confettiLeft_ = 0.0;
    visited_.assign(maze_.size(), std::vector<char>(maze_.empty() ? 0 : maze_[0].size(), 0));
    if (!maze_.empty()) visited_[start_.row][start_.col] = 1;
    emit stepsChanged(0);
    setFocus();
    update();
}

void GameWidget::setShowShortestPath(bool show)
{
    showShortest_ = show;
    update();
}

int GameWidget::shortestSteps() const
{
    return shortest_.empty() ? 0 : static_cast<int>(shortest_.size()) - 1;
}

double GameWidget::elapsedSeconds() const
{
    if (won_) return finalSeconds_;
    return playClock_.isValid() ? playClock_.elapsed() / 1000.0 : 0.0;
}

void GameWidget::updateGeometryForMaze()
{
    const int rows = static_cast<int>(maze_.size());
    const int cols = rows > 0 ? static_cast<int>(maze_[0].size()) : 0;
    if (rows == 0 || cols == 0) { setFixedSize(400, 400); return; }
    cellSize_ = std::clamp(kMaxCanvas / std::max(rows, cols), 16, 64);
    offsetX_ = kMargin;
    offsetY_ = kMargin;
    setFixedSize(cols * cellSize_ + 2 * kMargin, rows * cellSize_ + 2 * kMargin);
}

// ---------------- 输入与移动 ----------------

void GameWidget::keyPressEvent(QKeyEvent* event)
{
    int dir = -1;
    switch (event->key())
    {
    case Qt::Key_Up:    case Qt::Key_W: dir = static_cast<int>(Direction::Up); break;
    case Qt::Key_Right: case Qt::Key_D: dir = static_cast<int>(Direction::Right); break;
    case Qt::Key_Down:  case Qt::Key_S: dir = static_cast<int>(Direction::Down); break;
    case Qt::Key_Left:  case Qt::Key_A: dir = static_cast<int>(Direction::Left); break;
    case Qt::Key_R: emit restartRequested(); return;
    default: QWidget::keyPressEvent(event); return;
    }
    if (won_) return;

    if (moveT_ < 1.0) pendingDir_ = dir;  // 还在走上一步：先记下来，走完马上接着走
    else tryMove(static_cast<Direction>(dir));
}

void GameWidget::tryMove(Direction dir)
{
    if (dir == Direction::Left) facingLeft_ = true;
    else if (dir == Direction::Right) facingLeft_ = false;

    if (!canMove(maze_, player_, dir))   // 复用求解模块的 canMove 判断有没有墙
    {
        bumpDir_ = dir;
        bumpT_ = 0.0;                     // 撞墙：抖一下，不计步数
        return;
    }

    if (!playClock_.isValid()) playClock_.start();  // 第一步开始计时
    moveFrom_ = player_;
    player_ = step(player_, dir);
    moveT_ = 0.0;
    ++steps_;
    visited_[player_.row][player_.col] = 1;
    emit stepsChanged(steps_);
}

void GameWidget::onFrame()
{
    const qint64 now = animClock_.elapsed();
    const double dt = std::min(0.05, (now - lastFrameMs_) / 1000.0);
    lastFrameMs_ = now;
    time_ += dt;

    // 走一格的补间动画
    if (moveT_ < 1.0)
    {
        moveT_ = std::min(1.0, moveT_ + dt / kMoveDuration);
        if (moveT_ >= 1.0)
        {
            if (samePoint(player_, goal_) && !won_)
            {
                won_ = true;
                finalSeconds_ = playClock_.isValid() ? playClock_.elapsed() / 1000.0 : 0.0;
                pendingDir_ = -1;
                startConfetti();
                emit reachedGoal(steps_, shortestSteps(), finalSeconds_);
            }
            else if (pendingDir_ >= 0)
            {
                const int d = pendingDir_;
                pendingDir_ = -1;
                tryMove(static_cast<Direction>(d));
            }
        }
    }
    if (bumpT_ < 1.0) bumpT_ = std::min(1.0, bumpT_ + dt / 0.18);

    // 彩纸：重力下落 + 旋转
    if (!confetti_.empty())
    {
        confettiLeft_ -= dt;
        for (Confetti& c : confetti_)
        {
            c.vel.setY(c.vel.y() + 900.0 * dt);
            c.vel.setX(c.vel.x() * (1.0 - 0.6 * dt));
            c.pos += c.vel * dt;
            c.angle += c.spin * dt;
        }
        if (confettiLeft_ <= 0.0) confetti_.clear();
    }
    update();
}

void GameWidget::startConfetti()
{
    static const QColor colors[] = { QColor("#FF6B6B"), QColor("#FFD93D"), QColor("#6BCB77"),
                                     QColor("#4D96FF"), QColor("#FF9F43"), QColor("#C77DFF") };
    auto* rng = QRandomGenerator::global();
    const QPointF origin = cellCenterF(goal_.row, goal_.col);
    confetti_.clear();
    for (int i = 0; i < 110; ++i)
    {
        const double a = -kPi / 2 + (rng->generateDouble() - 0.5) * 2.2;  // 主要往上喷
        const double speed = 260 + rng->generateDouble() * 420;
        confetti_.push_back({ origin, QPointF(std::cos(a) * speed, std::sin(a) * speed),
                              rng->generateDouble() * 360, (rng->generateDouble() - 0.5) * 900,
                              colors[i % 6] });
    }
    confettiLeft_ = 2.4;
}

// ---------------- 绘制 ----------------

QPointF GameWidget::cellCenterF(double row, double col) const
{
    return QPointF(offsetX_ + (col + 0.5) * cellSize_, offsetY_ + (row + 0.5) * cellSize_);
}

void GameWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Theme::kWindowBg);
    if (maze_.empty()) return;

    drawGround(p);
    drawFootprints(p);
    if (showShortest_) drawShortestPath(p);
    drawFlag(p, start_);
    drawHedges(p);
    drawChest(p, goal_);
    drawExplorer(p);
    drawConfetti(p);
}

// 草地：圆角绿框 + 两种深浅交替的棋盘格
void GameWidget::drawGround(QPainter& p)
{
    const int rows = static_cast<int>(maze_.size());
    const int cols = static_cast<int>(maze_[0].size());
    const QRectF board(offsetX_, offsetY_, cols * cellSize_, rows * cellSize_);

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::kCanvasFrame);
    p.drawRoundedRect(board.adjusted(-10, -10, 10, 10), 18, 18);

    p.save();
    QPainterPath clip;
    clip.addRoundedRect(board.adjusted(-2, -2, 2, 2), 10, 10);
    p.setClipPath(clip);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            p.fillRect(QRectF(offsetX_ + c * cellSize_, offsetY_ + r * cellSize_, cellSize_, cellSize_),
                       ((r + c) % 2 == 0) ? Theme::kGrassA : Theme::kGrassB);
    p.restore();
}

// 走过的格子：一对小脚印
void GameWidget::drawFootprints(QPainter& p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Theme::kFootprint.red(), Theme::kFootprint.green(), Theme::kFootprint.blue(), 170));
    const double w = cellSize_ * 0.11, h = cellSize_ * 0.16;
    for (int r = 0; r < static_cast<int>(visited_.size()); ++r)
        for (int c = 0; c < static_cast<int>(visited_[r].size()); ++c)
        {
            if (!visited_[r][c] || (r == player_.row && c == player_.col)) continue;
            const QPointF ctr = cellCenterF(r, c);
            p.drawEllipse(ctr + QPointF(-cellSize_ * 0.09, cellSize_ * 0.06), w, h);
            p.drawEllipse(ctr + QPointF(cellSize_ * 0.09, -cellSize_ * 0.06), w, h);
        }
}

// BFS 最短路线：金色虚线（外面一圈白色描边，在草地上更醒目）
void GameWidget::drawShortestPath(QPainter& p)
{
    if (shortest_.size() < 2) return;
    QPolygonF line;
    for (const Point& pt : shortest_) line << cellCenterF(pt.row, pt.col);

    QPen outline(QColor(255, 255, 255, 200), cellSize_ * 0.26);
    outline.setCapStyle(Qt::RoundCap);
    outline.setJoinStyle(Qt::RoundJoin);
    p.setPen(outline);
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);

    QPen gold(Theme::kGoldPath, cellSize_ * 0.16);
    gold.setCapStyle(Qt::RoundCap);
    gold.setJoinStyle(Qt::RoundJoin);
    gold.setDashPattern({ 1.2, 1.4 });
    gold.setDashOffset(-time_ * 3.0);   // 虚线缓缓流动
    p.setPen(gold);
    p.drawPolyline(line);
}

// 树篱墙：先画深绿粗线，再在上面叠一条浅绿高光，看起来圆滚滚的
void GameWidget::drawHedges(QPainter& p)
{
    const int rows = static_cast<int>(maze_.size());
    const int cols = static_cast<int>(maze_[0].size());
    const double w = std::max(4.0, cellSize_ * 0.24);

    std::vector<QLineF> segs;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            const double x = offsetX_ + c * cellSize_, y = offsetY_ + r * cellSize_;
            const Cell& cell = maze_[r][c];
            // 每面墙只画一次：每格画上边和左边，最后一列补右边，最后一行补下边
            if (cell.wallTop) segs.emplace_back(x, y, x + cellSize_, y);
            if (cell.wallLeft) segs.emplace_back(x, y, x, y + cellSize_);
            if (c == cols - 1 && cell.wallRight) segs.emplace_back(x + cellSize_, y, x + cellSize_, y + cellSize_);
            if (r == rows - 1 && cell.wallBottom) segs.emplace_back(x, y + cellSize_, x + cellSize_, y + cellSize_);
        }

    QPen dark(Theme::kHedge, w);
    dark.setCapStyle(Qt::RoundCap);
    p.setPen(dark);
    for (const QLineF& s : segs) p.drawLine(s);

    QPen light(Theme::kHedgeLight, w * 0.42);
    light.setCapStyle(Qt::RoundCap);
    p.setPen(light);
    const QPointF up(0, -w * 0.14);
    for (const QLineF& s : segs) p.drawLine(s.translated(up));
}

// 起点：一面插在地上的小红旗
void GameWidget::drawFlag(QPainter& p, Point cell)
{
    const QPointF c = cellCenterF(cell.row, cell.col);
    const double s = cellSize_;
    p.setPen(QPen(QColor("#8B5A2B"), std::max(2.0, s * 0.06), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(c + QPointF(-s * 0.18, s * 0.32), c + QPointF(-s * 0.18, -s * 0.34));
    const double wave = std::sin(time_ * 5.0) * s * 0.04;  // 旗子轻轻飘
    QPainterPath flag;
    flag.moveTo(c + QPointF(-s * 0.16, -s * 0.34));
    flag.quadTo(c + QPointF(s * 0.05, -s * 0.30 + wave), c + QPointF(s * 0.24, -s * 0.24));
    flag.lineTo(c + QPointF(-s * 0.16, -s * 0.10));
    flag.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#FF6B6B"));
    p.drawPath(flag);
}

// 终点：宝箱。没打开时周围有星星闪烁；通关后箱盖打开、里面透出金光
void GameWidget::drawChest(QPainter& p, Point cell)
{
    const QPointF c = cellCenterF(cell.row, cell.col);
    const double s = cellSize_ * 0.78;
    const QRectF body(c.x() - s * 0.42, c.y() - s * 0.05, s * 0.84, s * 0.42);

    if (won_)
    {
        // 金光
        QRadialGradient glow(c, s * 0.9);
        glow.setColorAt(0, QColor(255, 225, 120, 200));
        glow.setColorAt(1, QColor(255, 225, 120, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(c, s * 0.9, s * 0.9);
    }

    // 地上的影子
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawEllipse(QPointF(c.x(), body.bottom() + s * 0.02), s * 0.44, s * 0.08);

    // 箱体
    p.setPen(QPen(QColor("#7A4A1E"), std::max(1.5, s * 0.04)));
    p.setBrush(QColor("#B5702F"));
    p.drawRoundedRect(body, s * 0.06, s * 0.06);

    // 箱盖：没打开时盖在上面，打开后向后翻起
    const QRectF lid(body.left(), body.top() - s * 0.26, body.width(), s * 0.30);
    p.save();
    if (won_)
    {
        p.translate(lid.left(), lid.bottom());
        p.rotate(-35);
        p.translate(-lid.left(), -lid.bottom());
    }
    QPainterPath lidPath;
    lidPath.moveTo(lid.bottomLeft());
    lidPath.lineTo(lid.left(), lid.top() + lid.height() * 0.45);
    lidPath.quadTo(lid.center().x(), lid.top() - lid.height() * 0.35, lid.right(), lid.top() + lid.height() * 0.45);
    lidPath.lineTo(lid.bottomRight());
    lidPath.closeSubpath();
    p.setBrush(QColor("#C98240"));
    p.drawPath(lidPath);
    p.restore();

    // 金色箍带和锁扣
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#FFC93C"));
    p.drawRect(QRectF(c.x() - s * 0.06, body.top(), s * 0.12, body.height()));
    if (!won_)
    {
        p.drawRect(QRectF(c.x() - s * 0.06, lid.top() + lid.height() * 0.2, s * 0.12, lid.height() * 0.8));
        p.setBrush(QColor("#E0A800"));
        p.drawRoundedRect(QRectF(c.x() - s * 0.08, body.top() - s * 0.06, s * 0.16, s * 0.14), 2, 2);
    }
    else
    {
        // 打开后：箱口一排金币
        p.setBrush(QColor("#FFD93D"));
        for (int i = -2; i <= 2; ++i)
            p.drawEllipse(QPointF(c.x() + i * s * 0.14, body.top() + s * 0.02), s * 0.08, s * 0.05);
    }

    // 闪烁的小星星（三颗，相位错开）
    for (int i = 0; i < 3; ++i)
    {
        const double phase = time_ * 3.0 + i * 2.1;
        const double a = (std::sin(phase) + 1.0) / 2.0;
        if (a < 0.15) continue;
        const QPointF pos = c + QPointF((i - 1) * s * 0.55, -s * (0.55 + 0.15 * (i % 2)));
        p.setBrush(QColor(255, 236, 140, static_cast<int>(255 * a)));
        p.drawPath(starPath(pos, s * 0.13 * a + s * 0.03, s * 0.05));
    }
}

// 小探险家：影子、身体、脑袋、探险帽、眼睛、腮红、微笑。走路时弹跳，向左走时整个人水平翻转
void GameWidget::drawExplorer(QPainter& p)
{
    // 平滑移动的位置（ease-out：起步快、到位时减速）
    const double t = 1.0 - (1.0 - moveT_) * (1.0 - moveT_);
    const double row = moveFrom_.row + (player_.row - moveFrom_.row) * t;
    const double col = moveFrom_.col + (player_.col - moveFrom_.col) * t;
    QPointF c = cellCenterF(row, col);
    const double s = cellSize_;

    // 撞墙抖动：朝墙的方向推一下再弹回来
    if (bumpT_ < 1.0)
    {
        const double k = std::sin(bumpT_ * kPi) * s * 0.12;
        const Point d = step({ 0, 0 }, bumpDir_);
        c += QPointF(d.col * k, d.row * k);
    }

    double hop = (moveT_ < 1.0) ? std::sin(moveT_ * kPi) * s * 0.16 : 0.0;   // 走路弹跳
    if (won_)
    {
        // 通关：站到宝箱左边，开心地一蹦一蹦（不挡住打开的宝箱）
        c += QPointF(-s * 0.48, s * 0.02);
        hop = std::fabs(std::sin(time_ * 6.0)) * s * 0.18;
    }
    const double breathe = (moveT_ >= 1.0) ? std::sin(time_ * 3.0) * 0.025 : 0.0;  // 站着时轻轻呼吸

    // 影子（不跟着跳，跳起来时变小）
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 45));
    const double shadowScale = 1.0 - hop / s;
    p.drawEllipse(c + QPointF(0, s * 0.32), s * 0.24 * shadowScale, s * 0.07 * shadowScale);

    p.save();
    p.translate(c + QPointF(0, -hop));
    p.scale(facingLeft_ ? -1.0 : 1.0, 1.0 + breathe);
    const QColor outline("#5A3E2B");
    const double lw = std::max(1.2, s * 0.035);

    // 身体：绿色探险服
    p.setPen(QPen(outline, lw));
    p.setBrush(QColor("#6AB04C"));
    p.drawRoundedRect(QRectF(-s * 0.18, s * 0.06, s * 0.36, s * 0.26), s * 0.10, s * 0.10);
    // 小背包带
    p.setPen(QPen(QColor("#8B5A2B"), lw * 1.2));
    p.drawLine(QPointF(-s * 0.08, s * 0.07), QPointF(-s * 0.02, s * 0.30));

    // 脑袋
    p.setPen(QPen(outline, lw));
    p.setBrush(QColor("#FFD7B5"));
    p.drawEllipse(QPointF(0, -s * 0.06), s * 0.22, s * 0.20);

    // 眼睛（朝前看，所以稍微偏向前方）+ 高光
    p.setPen(Qt::NoPen);
    p.setBrush(outline);
    p.drawEllipse(QPointF(s * 0.03, -s * 0.05), s * 0.035, s * 0.05);
    p.drawEllipse(QPointF(s * 0.13, -s * 0.05), s * 0.035, s * 0.05);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(s * 0.042, -s * 0.07), s * 0.013, s * 0.016);
    p.drawEllipse(QPointF(s * 0.142, -s * 0.07), s * 0.013, s * 0.016);

    // 腮红
    p.setBrush(QColor(255, 140, 150, 140));
    p.drawEllipse(QPointF(-s * 0.03, s * 0.03), s * 0.045, s * 0.028);
    p.drawEllipse(QPointF(s * 0.18, s * 0.03), s * 0.04, s * 0.026);

    // 微笑
    p.setPen(QPen(outline, lw * 0.9, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(s * 0.05, -s * 0.01, s * 0.08, s * 0.06), 200 * 16, 140 * 16);

    // 探险帽：宽帽檐 + 帽顶 + 深色帽带
    p.setPen(QPen(outline, lw));
    p.setBrush(QColor("#D4A762"));
    p.drawEllipse(QPointF(0, -s * 0.19), s * 0.30, s * 0.07);
    QPainterPath crown;
    crown.moveTo(-s * 0.17, -s * 0.19);
    crown.cubicTo(-s * 0.16, -s * 0.40, s * 0.16, -s * 0.40, s * 0.17, -s * 0.19);
    crown.closeSubpath();
    p.setBrush(QColor("#C8964F"));
    p.drawPath(crown);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#8B5A2B"));
    p.drawRect(QRectF(-s * 0.165, -s * 0.245, s * 0.33, s * 0.05));

    p.restore();
}

void GameWidget::drawConfetti(QPainter& p)
{
    if (confetti_.empty()) return;
    const double alpha = std::clamp(confettiLeft_ / 0.8, 0.0, 1.0);  // 最后 0.8 秒淡出
    const double w = std::max(4.0, cellSize_ * 0.14);
    p.setPen(Qt::NoPen);
    for (const Confetti& c : confetti_)
    {
        QColor col = c.color;
        col.setAlphaF(alpha);
        p.save();
        p.translate(c.pos);
        p.rotate(c.angle);
        p.setBrush(col);
        p.drawRect(QRectF(-w / 2, -w / 4, w, w / 2));
        p.restore();
    }
}
