#include "compare_dialog.h"
#include "gui.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
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

    bool samePoints(const std::vector<Point>& a, const std::vector<Point>& b)
    {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].row != b[i].row || a[i].col != b[i].col) return false;
        return true;
    }

    QTableWidgetItem* cell(const QString& text, bool center = true)
    {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        if (center) item->setTextAlignment(Qt::AlignCenter);
        return item;
    }
}

CompareDialog::CompareDialog(const MazeGrid& maze, Point start, Point end,
                             const QString& mazeDescription, bool allowRecursive, QWidget* parent)
    : QDialog(parent), maze_(maze), start_(start), end_(end), allowRecursive_(allowRecursive)
{
    setWindowTitle("求解算法对比：BFS / DFS 的递归与非递归版（同一迷宫、同一起终点）");

    const std::pair<SolveAlgorithm, QString> algos[] = {
        { SolveAlgorithm::BFS,           "BFS 非递归（队列）" },
        { SolveAlgorithm::BFS_Recursive, "BFS 递归" },
        { SolveAlgorithm::DFS_Recursive, "DFS 递归" },
        { SolveAlgorithm::DFS_Iterative, "DFS 非递归（栈）" },
    };
    for (const auto& [algo, name] : algos)
    {
        Result r;
        r.algo = algo;
        r.name = name;
        results_.push_back(r);
    }
    runAll();

    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);  // 窗口大小按内容固定，迷宫不会被挤压重叠

    auto* header = new QLabel(QString("迷宫：%1　　起点 (%2,%3) → 终点 (%4,%5)")
        .arg(mazeDescription).arg(start_.row).arg(start_.col).arg(end_.row).arg(end_.col), this);
    layout->addWidget(header);

    // ---------- 四个小迷宫并排 ----------
    auto* viewsRow = new QHBoxLayout();
    std::vector<QLabel*> captions;
    for (const Result& r : results_)
    {
        auto* column = new QVBoxLayout();
        auto* title = new QLabel("<b>" + r.name + "</b>", this);
        title->setAlignment(Qt::AlignCenter);
        column->addWidget(title);

        auto* caption = new QLabel(this);
        caption->setAlignment(Qt::AlignCenter);

        if (r.ran)
        {
            auto* view = new MazeWidget(this);
            view->setCompact(true, 250);   // 四个并排，每个小一点
            view->setMaze(maze_);
            view->setEndpoints(start_, end_);
            view->setSolution(r.path, r.visitedOrder, r.parentOf);
            column->addWidget(view, 0, Qt::AlignHCenter);
            views_.push_back(view);

            // 每走一步刷新这一列下方的进度文字
            const int pathLen = static_cast<int>(r.path.size());
            connect(view, &MazeWidget::stepChanged, caption, [caption, view, pathLen](int step, int total) {
                QString text = QString("已访问 %1 / %2 格").arg(step).arg(total);
                if (view->isFinished()) text += QString("　路径 %1 格").arg(pathLen);
                caption->setText(text);
            });
            caption->setText(QString("已访问 0 / %1 格").arg(r.visitedOrder.size()));
            connect(view, &MazeWidget::stepChanged, this, [this](int, int) { updateButtons(); });
            connect(view, &MazeWidget::playingChanged, this, [this](bool) { updateButtons(); });
            connect(view, &MazeWidget::finished, this, [this]() { updateButtons(); });
        }
        else
        {
            auto* placeholder = new QLabel("迷宫超过 50×50 格\n递归版可能栈溢出\n已跳过", this);
            placeholder->setAlignment(Qt::AlignCenter);
            placeholder->setMinimumSize(200, 200);
            placeholder->setWordWrap(true);
            placeholder->setStyleSheet("color: #D98E04; border: 1px dashed #F0D9B5; border-radius: 8px;");
            column->addWidget(placeholder);
            views_.push_back(nullptr);
        }
        column->addWidget(caption);
        column->addStretch();
        viewsRow->addLayout(column);
    }
    layout->addLayout(viewsRow);

    // ---------- 同步播放控制 ----------
    auto* controls = new QHBoxLayout();
    speedSlider_ = new QSlider(Qt::Horizontal, this);
    speedSlider_->setRange(0, 100);
    speedSlider_->setValue(40);  // 约每秒 16 步
    speedSlider_->setMaximumWidth(220);
    speedLabel_ = new QLabel(this);
    speedLabel_->setMinimumWidth(80);
    playButton_ = new QPushButton("同步播放", this);
    skipButton_ = new QPushButton("全部跳到结尾", this);
    resetButton_ = new QPushButton("重置", this);
    controls->addWidget(new QLabel("速度：", this));
    controls->addWidget(speedSlider_);
    controls->addWidget(speedLabel_);
    controls->addSpacing(20);
    controls->addWidget(playButton_);
    controls->addWidget(skipButton_);
    controls->addWidget(resetButton_);
    controls->addStretch();
    layout->addLayout(controls);

    auto applySpeed = [this](int value) {
        const int steps = sliderToStepsPerSecond(value);
        for (MazeWidget* v : views_) if (v) v->setStepsPerSecond(steps);
        speedLabel_->setText(QString("%1 步/秒").arg(steps));
    };
    connect(speedSlider_, &QSlider::valueChanged, this, applySpeed);
    applySpeed(speedSlider_->value());

    connect(playButton_, &QPushButton::clicked, this, [this]() {
        bool anyPlaying = false, allFinished = true;
        for (MazeWidget* v : views_)
        {
            if (!v) continue;
            anyPlaying |= v->isPlaying();
            allFinished &= v->isFinished();
        }
        for (MazeWidget* v : views_)
        {
            if (!v) continue;
            if (anyPlaying) v->pause();
            else if (allFinished) v->play();          // 全部播完：一起从头重播
            else if (!v->isFinished()) v->play();     // 还没播完的继续播（先播完的停在终点等其他人）
        }
        updateButtons();
    });
    connect(skipButton_, &QPushButton::clicked, this, [this]() {
        for (MazeWidget* v : views_) if (v) v->skipToEnd();
        updateButtons();
    });
    connect(resetButton_, &QPushButton::clicked, this, [this]() {
        for (MazeWidget* v : views_) if (v) v->resetAnimation();
        updateButtons();
    });

    // ---------- 数据表 ----------
    table_ = new QTableWidget(this);
    buildTable();
    layout->addWidget(table_);

    summaryLabel_ = new QLabel(buildSummary(), this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextFormat(Qt::RichText);
    layout->addWidget(summaryLabel_);

    auto* note = new QLabel(
        "说明：① 耗时为重复运行多次后的单次平均值，只计算求解算法本身，不含动画；"
        "Debug 和 Release 编译下的数值会差很多，对比时请用同一种编译方式。<br>"
        "② 完美迷宫中任意两点之间只有一条路径，所以几种算法找到的路径一定相同；"
        "要体现 BFS \"保证最短路径\"的优势，需要在带环路的迷宫上对比。", this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #A08770;");
    layout->addWidget(note);

    // 自己建"关闭"按钮：Qt 自带的标准按钮在没加载翻译时显示英文 "Close"
    auto* buttons = new QDialogButtonBox(this);
    auto* closeButton = buttons->addButton("关闭", QDialogButtonBox::RejectRole);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateButtons();
}

// 运行各个算法：先跑一次拿到结果，再按单次耗时决定重复次数，测平均耗时
void CompareDialog::runAll()
{
    using Clock = std::chrono::steady_clock;
    for (Result& r : results_)
    {
        const bool recursive = (r.algo == SolveAlgorithm::DFS_Recursive || r.algo == SolveAlgorithm::BFS_Recursive);
        if (recursive && !allowRecursive_) continue;

        const auto t0 = Clock::now();
        solveMaze(maze_, start_, end_, r.algo, r.path, r.visitedOrder, r.parentOf);
        const double once = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();

        // 让总测量时间大约 20ms：单次越快重复越多，避免计时精度不够
        r.repeats = std::clamp(static_cast<int>(20000.0 / std::max(once, 0.1)), 1, 2000);
        std::vector<Point> p, v, f;
        const auto t1 = Clock::now();
        for (int i = 0; i < r.repeats; ++i)
            solveMaze(maze_, start_, end_, r.algo, p, v, f);
        const double total = std::chrono::duration<double, std::micro>(Clock::now() - t1).count();
        r.avgMicros = total / r.repeats;
        r.ran = true;
    }
}

void CompareDialog::buildTable()
{
    const QStringList headers = { "算法", "访问格数", "访问占比", "路径长度", "是否最短路径", "平均求解耗时", "测量次数" };
    table_->setColumnCount(headers.size());
    table_->setRowCount(static_cast<int>(results_.size()));
    table_->setHorizontalHeaderLabels(headers);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);

    const int totalCells = static_cast<int>(maze_.size() * maze_[0].size());

    // 所有实际运行的算法中最短的路径长度（终点不可达的不算）
    std::size_t shortest = 0;
    for (const Result& r : results_)
        if (r.ran && !r.path.empty() && (shortest == 0 || r.path.size() < shortest)) shortest = r.path.size();

    for (int row = 0; row < static_cast<int>(results_.size()); ++row)
    {
        const Result& r = results_[row];
        table_->setItem(row, 0, cell(r.name, false));
        if (!r.ran)
        {
            for (int c = 1; c < headers.size(); ++c)
                table_->setItem(row, c, cell(c == 1 ? "已跳过（迷宫过大）" : "—"));
            continue;
        }
        const int visited = static_cast<int>(r.visitedOrder.size());
        table_->setItem(row, 1, cell(QString::number(visited)));
        table_->setItem(row, 2, cell(QString("%1%").arg(100.0 * visited / totalCells, 0, 'f', 1)));
        table_->setItem(row, 3, cell(r.path.empty() ? "不可达" : QString::number(r.path.size())));
        table_->setItem(row, 4, cell(r.path.empty() ? "—" : (r.path.size() == shortest ? "是" : "否")));
        table_->setItem(row, 5, cell(QString("%1 µs").arg(r.avgMicros, 0, 'f', 2)));
        table_->setItem(row, 6, cell(QString::number(r.repeats)));
    }

    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // 每行固定同样的高度
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().lineSpacing() + 10);
    // 表格高度刚好放下表头 + 三行，不出现滚动条
    int h = table_->horizontalHeader()->sizeHint().height() + 2 * table_->frameWidth() + 4;
    for (int row = 0; row < table_->rowCount(); ++row) h += table_->rowHeight(row);
    table_->setFixedHeight(h);
}

QString CompareDialog::buildSummary() const
{
    auto find = [this](SolveAlgorithm a) -> const Result& {
        for (const Result& r : results_) if (r.algo == a) return r;
        return results_.front();
    };
    const Result& bfs = find(SolveAlgorithm::BFS);
    const Result& bfsRec = find(SolveAlgorithm::BFS_Recursive);
    const Result& rec = find(SolveAlgorithm::DFS_Recursive);
    const Result& iter = find(SolveAlgorithm::DFS_Iterative);
    if (!bfs.ran || !iter.ran || bfs.path.empty() || iter.path.empty())
        return "终点不可达，无法比较路径。";

    QStringList parts;

    const int vb = static_cast<int>(bfs.visitedOrder.size());
    const int vd = static_cast<int>(iter.visitedOrder.size());
    if (vd < vb)
        parts << QString("DFS 访问 %1 格，比 BFS（%2 格）少 <b>%3%</b>").arg(vd).arg(vb).arg(100.0 * (vb - vd) / vb, 0, 'f', 1);
    else if (vd > vb)
        parts << QString("DFS 访问 %1 格，比 BFS（%2 格）多 <b>%3%</b>").arg(vd).arg(vb).arg(100.0 * (vd - vb) / vb, 0, 'f', 1);
    else
        parts << QString("BFS 与 DFS 访问格数相同（%1 格）").arg(vb);

    if (bfs.path.size() == iter.path.size())
        parts << QString("路径长度相同（%1 格）").arg(bfs.path.size());
    else
        parts << QString("BFS 路径 %1 格为最短，DFS 路径 %2 格（多 %3 格）")
                     .arg(bfs.path.size()).arg(iter.path.size()).arg(iter.path.size() - bfs.path.size());

    // 递归 / 非递归两种写法应当逐格一致，区别只在于用系统调用栈还是显式的队列 / 栈
    if (bfsRec.ran && rec.ran)
    {
        const bool sameBfs = samePoints(bfs.visitedOrder, bfsRec.visitedOrder) && samePoints(bfs.path, bfsRec.path);
        const bool sameDfs = samePoints(rec.visitedOrder, iter.visitedOrder) && samePoints(rec.path, iter.path);
        parts << ((sameBfs && sameDfs)
            ? QString("BFS、DFS 各自的递归版与非递归版访问顺序<b>逐格完全一致</b>，区别只在于用系统调用栈还是显式的队列/栈")
            : QString("<span style='color:#E84A5F;'>注意：递归版与非递归版访问顺序不一致</span>"));
    }

    return "<b>结论：</b>" + parts.join("；") + "。";
}

void CompareDialog::updateButtons()
{
    bool anyPlaying = false, allFinished = true, anyStarted = false;
    for (MazeWidget* v : views_)
    {
        if (!v) continue;
        anyPlaying |= v->isPlaying();
        allFinished &= v->isFinished();
        anyStarted |= v->currentStep() > 0;
    }
    if (anyPlaying) playButton_->setText("暂停");
    else if (allFinished) playButton_->setText("重新播放");
    else if (anyStarted) playButton_->setText("继续");
    else playButton_->setText("同步播放");
    skipButton_->setEnabled(!allFinished);
    resetButton_->setEnabled(anyStarted);
}
