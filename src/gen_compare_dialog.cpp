#include "gen_compare_dialog.h"
#include "gui.h"
#include "maze_solver.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
    // 与主窗口相同的速度映射：滑块 0~100 -> 每秒 1~1000 步（对数刻度）
    int sliderToStepsPerSecond(int value)
    {
        return static_cast<int>(std::lround(std::pow(10.0, value * 3.0 / 100.0)));
    }

    QTableWidgetItem* cell(const QString& text, bool center = true)
    {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        if (center) item->setTextAlignment(Qt::AlignCenter);
        return item;
    }

    // 格子的出口数（没有墙的方向数）
    int openings(const Cell& c)
    {
        return (!c.wallTop) + (!c.wallRight) + (!c.wallBottom) + (!c.wallLeft);
    }
}

GenCompareDialog::GenCompareDialog(int width, int height, QWidget* parent)
    : QDialog(parent), width_(width), height_(height)
{
    samples_count_ = std::clamp(200000 / std::max(1, width * height), 20, 100);
    setWindowTitle("生成算法对比：Kruskal（并查集） vs Prim（堆优化）");

    columns_.push_back({ GenAlgorithm::Kruskal, "Kruskal（并查集）", {} });
    columns_.push_back({ GenAlgorithm::Prim, "Prim（堆优化）", {} });

    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);  // 窗口大小按内容固定，迷宫不会被挤压重叠
    auto* header = new QLabel(QString("迷宫大小：%1 行 × %2 列（共 %3 格，完美迷宫）。"
                                      "同步播放两种算法的拆墙过程，生成完后显示起点到终点的最短路线。")
                                  .arg(height_).arg(width_).arg(width_ * height_), this);
    header->setWordWrap(true);
    layout->addWidget(header);

    // 左右两栏：左边两个迷宫 + 播放控制，右边统计表 + 结论（这样窗口不会太高）
    auto* body = new QHBoxLayout();
    auto* left = new QVBoxLayout();
    auto* right = new QVBoxLayout();
    body->addLayout(left);
    body->addSpacing(12);
    body->addLayout(right);
    layout->addLayout(body);

    // ---------- 两个迷宫并排 ----------
    auto* viewsRow = new QHBoxLayout();
    for (int index = 0; index < static_cast<int>(columns_.size()); ++index)
    {
        AlgoColumn& col = columns_[index];
        auto* column = new QVBoxLayout();
        auto* title = new QLabel("<b style='font-size:12pt;'>" + col.name + "</b>", this);
        title->setAlignment(Qt::AlignCenter);
        col.view = new MazeWidget(this);
        col.view->setCompact(true, 300);
        col.caption = new QLabel(this);
        col.caption->setAlignment(Qt::AlignCenter);
        column->addWidget(title);
        column->addWidget(col.view, 0, Qt::AlignHCenter);
        column->addWidget(col.caption);
        column->addStretch();
        viewsRow->addLayout(column);

        MazeWidget* view = col.view;
        QLabel* caption = col.caption;
        // 进度文字
        connect(view, &MazeWidget::stepChanged, this, [this, view, caption](int step, int total) {
            if (view->phase() == MazeWidget::Phase::Generating)
                caption->setText(QString("拆墙 %1 / %2 次").arg(step).arg(total));
            updateButtons();
        });
        connect(view, &MazeWidget::playingChanged, this, [this](bool) { updateButtons(); });
        // 生成播完 -> 用 BFS 求出左上角到右下角的最短路线，直接显示结果（热力图 + 路径）
        connect(view, &MazeWidget::finished, this, [this, view, caption, index]() {
            if (view->phase() == MazeWidget::Phase::Generating)
            {
                const Point start{ 0, 0 }, end{ height_ - 1, width_ - 1 };
                std::vector<Point> path, visited, parent;
                solveMaze(samples_[index], start, end, SolveAlgorithm::BFS, path, visited, parent);
                view->setEndpoints(start, end);
                view->setSolution(path, visited, parent);
                view->skipToEnd();
                caption->setText(QString("生成完成　起点→终点路径 %1 格").arg(path.size()));
            }
            updateButtons();
        });
    }
    left->addLayout(viewsRow);

    // ---------- 同步播放控制 ----------
    auto* controls = new QHBoxLayout();
    speedSlider_ = new QSlider(Qt::Horizontal, this);
    speedSlider_->setRange(0, 100);
    speedSlider_->setValue(57);  // 约每秒 50 步
    speedSlider_->setMaximumWidth(140);
    speedLabel_ = new QLabel(this);
    speedLabel_->setMinimumWidth(80);
    playButton_ = new QPushButton("同步播放", this);
    playButton_->setObjectName("accentButton");
    skipButton_ = new QPushButton("全部跳到结尾", this);
    resetButton_ = new QPushButton("重置", this);
    auto* resampleButton = new QPushButton("换一组样本", this);
    controls->addWidget(new QLabel("速度：", this));
    controls->addWidget(speedSlider_);
    controls->addWidget(speedLabel_);
    controls->addSpacing(16);
    controls->addWidget(playButton_);
    controls->addWidget(skipButton_);
    controls->addWidget(resetButton_);
    controls->addWidget(resampleButton);
    left->addLayout(controls);
    left->addStretch();

    auto applySpeed = [this](int value) {
        const int steps = sliderToStepsPerSecond(value);
        for (AlgoColumn& col : columns_) col.view->setStepsPerSecond(steps);
        speedLabel_->setText(QString("%1 步/秒").arg(steps));
    };
    connect(speedSlider_, &QSlider::valueChanged, this, applySpeed);

    connect(playButton_, &QPushButton::clicked, this, [this]() {
        bool anyPlaying = false;
        for (AlgoColumn& col : columns_) anyPlaying |= col.view->isPlaying();
        for (AlgoColumn& col : columns_)
        {
            if (anyPlaying) col.view->pause();
            else if (col.view->phase() == MazeWidget::Phase::Generating && !col.view->isFinished()) col.view->play();
        }
        // 两个都已生成完：重新开始播放生成过程
        bool allDone = true;
        for (AlgoColumn& col : columns_) allDone &= (col.view->phase() == MazeWidget::Phase::Solving);
        if (!anyPlaying && allDone)
        {
            for (int i = 0; i < static_cast<int>(columns_.size()); ++i)
                columns_[i].view->setGeneration(samples_[i], sampleSteps_[i]);
            for (AlgoColumn& col : columns_) col.view->play();
        }
        updateButtons();
    });
    connect(skipButton_, &QPushButton::clicked, this, [this]() {
        for (AlgoColumn& col : columns_)
            if (col.view->phase() == MazeWidget::Phase::Generating) col.view->skipToEnd();
        updateButtons();
    });
    connect(resetButton_, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < static_cast<int>(columns_.size()); ++i)
            columns_[i].view->setGeneration(samples_[i], sampleSteps_[i]);
        updateButtons();
    });
    connect(resampleButton, &QPushButton::clicked, this, [this]() { newSample(); });

    // ---------- 统计表 ----------
    runStatistics();
    table_ = new QTableWidget(this);
    table_->setFixedWidth(430);
    buildTable();
    right->addWidget(table_);

    summaryLabel_ = new QLabel(buildSummary(), this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextFormat(Qt::RichText);
    summaryLabel_->setFixedWidth(430);
    right->addWidget(summaryLabel_);

    auto* note = new QLabel(QString(
        "说明：统计为每种算法各 %1 个同尺寸随机迷宫（固定种子，可复现）的平均值；耗时不含动画，正式比较请用 Release 编译。<br>"
        "理论复杂度（E 为内部墙数，约 2N）：Kruskal = 随机打乱 O(E) + 并查集 O(E·α(N))；Prim = 每面墙进出二叉堆一次 O(E log E)。")
        .arg(samples_count_), this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #A08770;");
    note->setFixedWidth(430);
    right->addWidget(note);
    right->addStretch();

    auto* buttons = new QDialogButtonBox(this);
    auto* closeButton = buttons->addButton("关闭", QDialogButtonBox::RejectRole);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);

    newSample();
    applySpeed(speedSlider_->value());
}

void GenCompareDialog::newSample()
{
    samples_.assign(columns_.size(), MazeGrid{});
    sampleSteps_.assign(columns_.size(), {});
    const unsigned seed = QRandomGenerator::global()->generate();
    for (int i = 0; i < static_cast<int>(columns_.size()); ++i)
    {
        samples_[i] = generateMazeWithSteps(width_, height_, columns_[i].algo, sampleSteps_[i], seed + i, 0.0);
        columns_[i].view->setGeneration(samples_[i], sampleSteps_[i]);
    }
    updateButtons();
}

GenCompareDialog::MazeStats GenCompareDialog::measure(const MazeGrid& maze)
{
    MazeStats s;
    const int rows = static_cast<int>(maze.size());
    const int cols = static_cast<int>(maze[0].size());
    int straight = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            const Cell& cell = maze[r][c];
            const int n = openings(cell);
            if (n == 1) ++s.deadEnds;
            if (n >= 3) ++s.junctions;
            if (n == 2 && ((!cell.wallTop && !cell.wallBottom) || (!cell.wallLeft && !cell.wallRight))) ++straight;
            // 内部墙：每格只数右边和下边，避免重复
            if (c < cols - 1 && !cell.wallRight) ++s.wallsRemoved;
            if (r < rows - 1 && !cell.wallBottom) ++s.wallsRemoved;
        }
    s.straightRatio = 100.0 * straight / (rows * cols);

    std::vector<Point> path, visited, parent;
    solveMaze(maze, { 0, 0 }, { rows - 1, cols - 1 }, SolveAlgorithm::BFS, path, visited, parent);
    s.pathLength = static_cast<int>(path.size());
    for (std::size_t i = 2; i < path.size(); ++i)
    {
        const int dr1 = path[i - 1].row - path[i - 2].row, dc1 = path[i - 1].col - path[i - 2].col;
        const int dr2 = path[i].row - path[i - 1].row, dc2 = path[i].col - path[i - 1].col;
        if (dr1 != dr2 || dc1 != dc2) ++s.pathTurns;
    }
    return s;
}

void GenCompareDialog::runStatistics()
{
    using Clock = std::chrono::steady_clock;
    for (AlgoColumn& col : columns_)
    {
        MazeStats sum;
        for (int i = 0; i < samples_count_; ++i)
        {
            const unsigned seed = 1000u + static_cast<unsigned>(i);  // 固定种子：每次打开结果都一样，方便展示
            const auto t0 = Clock::now();
            const MazeGrid maze = generateMaze(width_, height_, col.algo, seed, 0.0);
            const double micros = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
            const MazeStats s = measure(maze);
            sum.genMicros += micros;
            sum.wallsRemoved += s.wallsRemoved;
            sum.deadEnds += s.deadEnds;
            sum.junctions += s.junctions;
            sum.straightRatio += s.straightRatio;
            sum.pathLength += s.pathLength;
            sum.pathTurns += s.pathTurns;
        }
        col.avg.genMicros = sum.genMicros / samples_count_;
        col.avg.wallsRemoved = sum.wallsRemoved / samples_count_;
        col.avg.deadEnds = static_cast<int>(std::lround(static_cast<double>(sum.deadEnds) / samples_count_));
        col.avg.junctions = static_cast<int>(std::lround(static_cast<double>(sum.junctions) / samples_count_));
        col.avg.straightRatio = sum.straightRatio / samples_count_;
        col.avg.pathLength = static_cast<int>(std::lround(static_cast<double>(sum.pathLength) / samples_count_));
        col.avg.pathTurns = static_cast<int>(std::lround(static_cast<double>(sum.pathTurns) / samples_count_));
    }
}

void GenCompareDialog::buildTable()
{
    const QStringList rowNames = { "平均生成耗时", "拆墙次数", "死胡同（1 个出口）",
                                   "岔路口（3~4 个出口）", "直道格占比", "起点→终点路径长度", "路径拐弯次数" };
    table_->setRowCount(rowNames.size());
    table_->setColumnCount(1 + static_cast<int>(columns_.size()));
    QStringList headers = { QString("指标（%1 个迷宫平均）").arg(samples_count_) };
    for (const AlgoColumn& col : columns_) headers << col.name;
    table_->setHorizontalHeaderLabels(headers);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);

    for (int r = 0; r < rowNames.size(); ++r) table_->setItem(r, 0, cell(rowNames[r], false));
    for (int c = 0; c < static_cast<int>(columns_.size()); ++c)
    {
        const MazeStats& s = columns_[c].avg;
        const QStringList values = {
            QString("%1 µs").arg(s.genMicros, 0, 'f', 1),
            QString("%1 次").arg(s.wallsRemoved),
            QString("%1 个").arg(s.deadEnds),
            QString("%1 个").arg(s.junctions),
            QString("%1%").arg(s.straightRatio, 0, 'f', 1),
            QString("%1 格").arg(s.pathLength),
            QString("%1 次").arg(s.pathTurns),
        };
        for (int r = 0; r < values.size(); ++r) table_->setItem(r, c + 1, cell(values[r]));
    }

    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().lineSpacing() + 8);
    int h = table_->horizontalHeader()->sizeHint().height() + 2 * table_->frameWidth();
    for (int row = 0; row < table_->rowCount(); ++row) h += table_->rowHeight(row);
    table_->setFixedHeight(h + 4);
}

QString GenCompareDialog::buildSummary() const
{
    const MazeStats& k = columns_[0].avg;
    const MazeStats& p = columns_[1].avg;
    const int n = width_ * height_;
    QStringList parts;

    // 1. 完美迷宫 = 生成树：拆墙次数恰好 N-1
    if (k.wallsRemoved == n - 1 && p.wallsRemoved == n - 1)
        parts << QString("两种算法都恰好拆墙 <b>N−1 = %1</b> 次 —— %2 个格子连成一棵生成树，"
                         "这正是\"完美迷宫：连通且只有一条路\"的证明").arg(n - 1).arg(n);

    // 2. 耗时
    const bool kFaster = k.genMicros < p.genMicros;
    const double ratio = kFaster ? p.genMicros / std::max(0.001, k.genMicros) : k.genMicros / std::max(0.001, p.genMicros);
    parts << QString("%1 生成更快（约为另一种的 %2 倍速度）").arg(kFaster ? "Kruskal" : "Prim").arg(ratio, 0, 'f', 1);

    // 3. 形状差异：相差不到 5% 视为随机波动，不下"谁更多"的结论
    auto similar = [](double a, double b) { return std::fabs(a - b) <= 0.05 * std::max(a, b); };
    auto compare = [&similar](int a, int b, const QString& what, const QString& more) {
        if (similar(a, b)) return QString("%1差不多（%2 对 %3）").arg(what).arg(a).arg(b);
        return QString("%1的%2%3（%4 对 %5）").arg(a > b ? "Kruskal" : "Prim").arg(what).arg(more)
            .arg(std::max(a, b)).arg(std::min(a, b));
    };
    parts << compare(k.deadEnds, p.deadEnds, "死胡同数量", "更多");
    // 路径长度随机波动大，门槛放宽到 10%
    if (std::fabs(k.pathLength - p.pathLength) <= 0.10 * std::max(k.pathLength, p.pathLength))
        parts << QString("起点→终点路径长度差不多（%1 对 %2）").arg(k.pathLength).arg(p.pathLength);
    else
        parts << compare(k.pathLength, p.pathLength, "起点→终点路径长度", "更长");

    QString text = "<b>结论：</b>" + parts.join("；") + "。";
    const bool shapesSimilar = similar(k.deadEnds, p.deadEnds) && similar(k.junctions, p.junctions)
                            && similar(k.straightRatio, p.straightRatio)
                            && std::fabs(k.pathLength - p.pathLength) <= 0.10 * std::max(k.pathLength, p.pathLength);
    if (shapesSimilar)
        text += "<br><b>为什么形状几乎一样？</b>两者本质上都是在\"随机权重的网格图\"上求最小生成树"
                "（Kruskal 随机打乱墙的顺序，Prim 给每面墙随机权重），得到的迷宫在统计上属于同一种分布；"
                "真正的区别在于<b>生成过程</b>（Kruskal 全图多处同时合并、靠并查集判断是否成环；"
                "Prim 从一点向外生长、靠堆选出最小边）和<b>速度</b>。";
    return text;
}

void GenCompareDialog::updateButtons()
{
    bool anyPlaying = false, allDone = true, anyStarted = false;
    for (const AlgoColumn& col : columns_)
    {
        anyPlaying |= col.view->isPlaying();
        const bool done = (col.view->phase() == MazeWidget::Phase::Solving);
        allDone &= done;
        anyStarted |= done || col.view->currentStep() > 0;
    }
    if (anyPlaying) playButton_->setText("暂停");
    else if (allDone) playButton_->setText("重新播放");
    else if (anyStarted) playButton_->setText("继续");
    else playButton_->setText("同步播放");
    skipButton_->setEnabled(!allDone);
    resetButton_->setEnabled(anyStarted);
}
