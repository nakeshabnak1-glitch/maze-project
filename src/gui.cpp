#include "gui.h"
#include <QPainter>
#include<QString>

MazeWidget::MazeWidget(QWidget* parent) : QWidget(parent)
{
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this]() {
        if (animationStep_ < static_cast<int>(visitedOrder_.size()))
        {
            animationStep_++;
            setWindowTitle(QString("迷宫求解动画 - 已访问 %1 格").arg(animationStep_));
            update();
        }
        else
        {
            // 动画播放完毕，显示最终统计结果
            setWindowTitle(QString("迷宫求解动画 - 已访问 %1 格 | 路径长度 %2")
                .arg(visitedOrder_.size())
                .arg(path_.size()));
            timer_->stop();
        }
        });

    // 新增：开始按钮，点击之后才真正开始播放动画，而不是窗口一显示就自动开始
    startButton_ = new QPushButton("开始演示", this);
    startButton_->move(10, 8);
    startButton_->resize(100, topBarHeight_ - 16);
    connect(startButton_, &QPushButton::clicked, this, [this]() {
        startButton_->setEnabled(false);      // 防止重复点击
        startButton_->setText("演示中…");
        startAnimation();
        });
}

void MazeWidget::setMaze(const MazeGrid& maze)
{
    maze_ = maze;
    if (!maze_.empty())
    {
        int height = static_cast<int>(maze_.size()) * cellSize_;
        int width = static_cast<int>(maze_[0].size()) * cellSize_;
        setFixedSize(width + 20, height + 20 + legendHeight_ + topBarHeight_);
    }
    update();
}

void MazeWidget::setSolution(const std::vector<Point>& path,
    const std::vector<Point>& visitedOrder, const std::vector<Point>& parentOf)
{
    path_ = path;
    visitedOrder_ = visitedOrder;
    parentOf_ = parentOf;
    animationStep_ = 0;
    update();
}

void MazeWidget::startAnimation()
{
    animationStep_ = 0;
    timer_->start(250);  // 调慢后的速度
}

// 坐标换算抽成小函数，方便画连线时复用
static QPoint cellCenter(Point p, int offsetX, int offsetY, int cellSize)
{
    return QPoint(offsetX + p.col * cellSize + cellSize / 2,
        offsetY + p.row * cellSize + cellSize / 2);
}

void MazeWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int offsetX = 10;
    const int offsetY = 10 + topBarHeight_;  // 纵向多留出顶部按钮的高度，迷宫整体往下移
    painter.fillRect(rect(), QColor(250, 248, 240));

    // 第一层：已访问格子填色
    for (int i = 0; i < animationStep_ && i < static_cast<int>(visitedOrder_.size()); ++i)
    {
        const Point& p = visitedOrder_[i];
        int x = offsetX + p.col * cellSize_;
        int y = offsetY + p.row * cellSize_;
        bool isCurrent = (i == animationStep_ - 1);
        QColor color = isCurrent ? QColor(70, 130, 180) : QColor(173, 216, 230);
        painter.fillRect(x + 2, y + 2, cellSize_ - 4, cellSize_ - 4, color);
    }

    // 第二层：父节点连线（新增）
    QPen parentPen(QColor(180, 170, 150), 1);
    painter.setPen(parentPen);
    for (int i = 1; i < animationStep_ && i < static_cast<int>(parentOf_.size()); ++i)
    {
        const Point& parent = parentOf_[i];
        if (parent.row < 0 || parent.col < 0) continue;  // 起点没有父节点，跳过
        QPoint from = cellCenter(parent, offsetX, offsetY, cellSize_);
        QPoint to = cellCenter(visitedOrder_[i], offsetX, offsetY, cellSize_);
        painter.drawLine(from, to);
    }

    // 第三层：墙壁
    QPen wallPen(QColor(50, 50, 50), 2);
    painter.setPen(wallPen);
    for (std::size_t row = 0; row < maze_.size(); ++row)
    {
        for (std::size_t col = 0; col < maze_[row].size(); ++col)
        {
            int x = offsetX + static_cast<int>(col) * cellSize_;
            int y = offsetY + static_cast<int>(row) * cellSize_;
            const Cell& cell = maze_[row][col];
            if (cell.wallTop)    painter.drawLine(x, y, x + cellSize_, y);
            if (cell.wallLeft)   painter.drawLine(x, y, x, y + cellSize_);
            if (cell.wallRight)  painter.drawLine(x + cellSize_, y, x + cellSize_, y + cellSize_);
            if (cell.wallBottom) painter.drawLine(x, y + cellSize_, x + cellSize_, y + cellSize_);
        }
    }

    // 第四层：最终路径
    if (animationStep_ >= static_cast<int>(visitedOrder_.size()) && path_.size() > 1)
    {
        QPen pathPen(QColor(220, 20, 60), 4);
        pathPen.setCapStyle(Qt::RoundCap);
        pathPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pathPen);
        for (std::size_t i = 0; i + 1 < path_.size(); ++i)
            painter.drawLine(cellCenter(path_[i], offsetX, offsetY, cellSize_),
                cellCenter(path_[i + 1], offsetX, offsetY, cellSize_));
    }

    // 第五层：起点终点圆点
    if (!visitedOrder_.empty())
    {
        painter.setBrush(QColor(34, 139, 34));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(cellCenter(visitedOrder_.front(), offsetX, offsetY, cellSize_), cellSize_ / 4, cellSize_ / 4);
    }
    if (!path_.empty())
    {
        painter.setBrush(QColor(178, 34, 34));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(cellCenter(path_.back(), offsetX, offsetY, cellSize_), cellSize_ / 4, cellSize_ / 4);
    }

    // ---- 图例：画在迷宫下方 ----
    int legendTop = offsetY + static_cast<int>(maze_.size()) * cellSize_ + 15;
    int legendX = offsetX;
    int rowH = 18;

    auto drawLegendItem = [&](int row, int col, QColor color, const QString& label, bool isLine = false)
        {
            int x = legendX + col * 160;
            int y = legendTop + row * rowH;
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

    drawLegendItem(0, 0, QColor(34, 139, 34), "起点");
    drawLegendItem(0, 1, QColor(178, 34, 34), "终点");
    drawLegendItem(0, 2, QColor(70, 130, 180), "当前探索格");
    drawLegendItem(1, 0, QColor(173, 216, 230), "已访问格子");
    drawLegendItem(1, 1, QColor(180, 170, 150), "父节点连线", true);
    drawLegendItem(1, 2, QColor(220, 20, 60), "最终路径", true);
}
