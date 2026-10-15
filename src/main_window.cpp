#include "main_window.h"
#include "gui.h"
#include "compare_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace
{
    // 速度滑块 0~100 映射到 每秒 1~1000 步（对数刻度：左半边细调慢速，右半边快速跑大迷宫）
    int sliderToStepsPerSecond(int value)
    {
        return static_cast<int>(std::lround(std::pow(10.0, value * 3.0 / 100.0)));
    }

    constexpr int kDefaultSpeedSlider = 20;  // 10^0.6 ≈ 每秒 4 步，和原来 250ms 一步的速度一样
}

MainWindow::MainWindow(QWidget* parent) : QWidget(parent)
{
    setWindowTitle("迷宫生成与求解系统");

    mazeWidget_ = new MazeWidget(this);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(buildControlPanel(), 0, Qt::AlignTop);
    layout->addWidget(mazeWidget_, 0, Qt::AlignTop);
    // 窗口大小跟着内容走：换大迷宫时窗口自动变大，换小迷宫时自动缩小
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // ---- MazeWidget 发出的信号 -> 刷新面板 ----
    connect(mazeWidget_, &MazeWidget::stepChanged, this, [this](int, int) { updateStats(); });
    connect(mazeWidget_, &MazeWidget::playingChanged, this, [this](bool) { updatePlayButton(); });
    connect(mazeWidget_, &MazeWidget::finished, this, &MainWindow::onAnimationFinished);
    connect(mazeWidget_, &MazeWidget::cellClicked, this, &MainWindow::onCellClicked);

    onSpeedChanged(speedSlider_->value());
    regenerateMaze(false);  // 启动时先生成一个默认迷宫，直接显示完整迷宫
}

QWidget* MainWindow::buildControlPanel()
{
    auto* panel = new QWidget(this);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);

    // ---------- 1. 迷宫设置 ----------
    auto* mazeGroup = new QGroupBox("迷宫设置", panel);
    auto* mazeForm = new QFormLayout(mazeGroup);

    widthSpin_ = new QSpinBox(mazeGroup);
    widthSpin_->setRange(2, 120);
    widthSpin_->setValue(15);
    widthSpin_->setSuffix(" 列");

    heightSpin_ = new QSpinBox(mazeGroup);
    heightSpin_->setRange(2, 120);
    heightSpin_->setValue(15);
    heightSpin_->setSuffix(" 行");

    genAlgoCombo_ = new QComboBox(mazeGroup);
    genAlgoCombo_->addItem("Kruskal（并查集）", static_cast<int>(GenAlgorithm::Kruskal));
    genAlgoCombo_->addItem("Prim（堆优化）", static_cast<int>(GenAlgorithm::Prim));

    animateGenCheck_ = new QCheckBox("播放生成过程", mazeGroup);
    animateGenCheck_->setChecked(true);
    animateGenCheck_->setToolTip("勾选后，点\"生成新迷宫\"会先从全是墙的网格开始，\n按生成算法的顺序一面一面拆墙，播完再进入求解");

    generateButton_ = new QPushButton("生成新迷宫", mazeGroup);

    mazeForm->addRow("宽度：", widthSpin_);
    mazeForm->addRow("高度：", heightSpin_);
    mazeForm->addRow("生成算法：", genAlgoCombo_);
    mazeForm->addRow(animateGenCheck_);
    mazeForm->addRow(generateButton_);

    // ---------- 2. 求解算法 ----------
    auto* solveGroup = new QGroupBox("求解算法", panel);
    auto* solveLayout = new QVBoxLayout(solveGroup);
    solveAlgoCombo_ = new QComboBox(solveGroup);
    solveAlgoCombo_->addItem("BFS 广度优先", static_cast<int>(SolveAlgorithm::BFS));
    solveAlgoCombo_->addItem("DFS 递归", static_cast<int>(SolveAlgorithm::DFS_Recursive));
    solveAlgoCombo_->addItem("DFS 非递归（显式栈）", static_cast<int>(SolveAlgorithm::DFS_Iterative));
    auto* solveHint = new QLabel("切换算法会在同一个迷宫上重新求解，\n方便直接对比。", solveGroup);
    solveHint->setStyleSheet("color: #666;");
    compareButton_ = new QPushButton("三种算法对比…", solveGroup);
    compareButton_->setToolTip("在当前迷宫、当前起终点上同时运行三种算法，\n并排播放并列出访问格数、路径长度、耗时");
    solveLayout->addWidget(solveAlgoCombo_);
    solveLayout->addWidget(solveHint);
    solveLayout->addWidget(compareButton_);

    // ---------- 3. 动画控制 ----------
    auto* playGroup = new QGroupBox("动画控制", panel);
    auto* playLayout = new QVBoxLayout(playGroup);

    auto* speedRow = new QHBoxLayout();
    speedSlider_ = new QSlider(Qt::Horizontal, playGroup);
    speedSlider_->setRange(0, 100);
    speedSlider_->setValue(kDefaultSpeedSlider);
    speedLabel_ = new QLabel(playGroup);
    speedLabel_->setMinimumWidth(80);
    speedRow->addWidget(new QLabel("速度：", playGroup));
    speedRow->addWidget(speedSlider_, 1);
    speedRow->addWidget(speedLabel_);

    playButton_ = new QPushButton("播放", playGroup);
    stepButton_ = new QPushButton("单步", playGroup);
    skipButton_ = new QPushButton("跳到结尾", playGroup);
    resetButton_ = new QPushButton("重置", playGroup);
    auto* buttonGrid = new QGridLayout();
    buttonGrid->addWidget(playButton_, 0, 0);
    buttonGrid->addWidget(stepButton_, 0, 1);
    buttonGrid->addWidget(skipButton_, 1, 0);
    buttonGrid->addWidget(resetButton_, 1, 1);

    playLayout->addLayout(speedRow);
    playLayout->addLayout(buttonGrid);

    // ---------- 4. 统计信息 ----------
    auto* statsGroup = new QGroupBox("统计信息", panel);
    auto* statsLayout = new QVBoxLayout(statsGroup);
    statsLabel_ = new QLabel(statsGroup);
    statsLabel_->setTextFormat(Qt::RichText);
    statsLabel_->setMinimumWidth(260);
    statsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // 固定能放下 10 行的高度：生成阶段行数少、求解阶段行数多，这样切换阶段时窗口不会忽高忽低
    statsLabel_->setMinimumHeight(10 * (statsLabel_->fontMetrics().lineSpacing() + 4));
    statsLayout->addWidget(statsLabel_);

    // ---------- 提示 ----------
    noticeLabel_ = new QLabel(panel);
    noticeLabel_->setWordWrap(true);
    noticeLabel_->setMaximumWidth(280);
    noticeLabel_->setStyleSheet("color: #b8860b;");
    noticeLabel_->hide();

    auto* tip = new QLabel("提示：在迷宫上 左键点击 设置起点，\n右键点击 设置终点。", panel);
    tip->setStyleSheet("color: #666;");

    panelLayout->addWidget(mazeGroup);
    panelLayout->addWidget(solveGroup);
    panelLayout->addWidget(playGroup);
    panelLayout->addWidget(statsGroup);
    panelLayout->addWidget(noticeLabel_);
    panelLayout->addWidget(tip);
    panelLayout->addStretch();

    // ---- 控件信号 -> 槽 ----
    connect(generateButton_, &QPushButton::clicked, this,
        [this]() { regenerateMaze(animateGenCheck_->isChecked()); });
    connect(solveAlgoCombo_, &QComboBox::currentIndexChanged, this, [this](int) { solveCurrentMaze(); });
    connect(compareButton_, &QPushButton::clicked, this, &MainWindow::openCompareDialog);
    connect(speedSlider_, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    connect(playButton_, &QPushButton::clicked, this, [this]() {
        if (mazeWidget_->isPlaying()) mazeWidget_->pause();
        else mazeWidget_->play();
    });
    connect(stepButton_, &QPushButton::clicked, mazeWidget_, &MazeWidget::stepOnce);
    connect(skipButton_, &QPushButton::clicked, mazeWidget_, &MazeWidget::skipToEnd);
    connect(resetButton_, &QPushButton::clicked, mazeWidget_, &MazeWidget::resetAnimation);

    return panel;
}

// ---------------- 生成与求解 ----------------

bool MainWindow::inGenerationPhase() const
{
    return mazeWidget_->phase() == MazeWidget::Phase::Generating;
}

void MainWindow::regenerateMaze(bool animate)
{
    const int width = widthSpin_->value();
    const int height = heightSpin_->value();
    const auto genAlgo = static_cast<GenAlgorithm>(genAlgoCombo_->currentData().toInt());

    std::vector<WallBreak> steps;
    try
    {
        // 统一用 generateMazeWithSteps：既拿到迷宫，也拿到拆墙顺序（不播放动画时直接丢掉即可）
        const auto t0 = std::chrono::steady_clock::now();
        maze_ = generateMazeWithSteps(width, height, genAlgo, steps);
        const auto t1 = std::chrono::steady_clock::now();
        genMicros_ = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(this, "生成失败", e.what());
        return;
    }
    genAlgo_ = genAlgo;
    genStepCount_ = static_cast<int>(steps.size());

    // 默认起点左上角、终点右下角
    start_ = { 0, 0 };
    end_ = { height - 1, width - 1 };

    // 先告诉 MazeWidget 新迷宫（进入生成阶段或求解阶段），
    // 这样后面 solveCurrentMaze 才能正确判断"现在要不要立刻把求解结果交出去"
    if (animate && !steps.empty()) mazeWidget_->setGeneration(maze_, steps);
    else mazeWidget_->setMaze(maze_);

    updateRecursiveDfsAvailability();  // 可能会切换算法下拉框（会触发一次求解），所以放在求解之前
    solveCurrentMaze();
}

void MainWindow::solveCurrentMaze()
{
    if (maze_.empty()) return;

    const SolveAlgorithm algo = currentSolveAlgorithm();
    try
    {
        const auto t0 = std::chrono::steady_clock::now();
        solveMaze(maze_, start_, end_, algo, path_, visitedOrder_, parentOf_);
        const auto t1 = std::chrono::steady_clock::now();
        solveMicros_ = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }
    catch (const std::invalid_argument& e)
    {
        QMessageBox::warning(this, "输入有误", e.what());
        return;
    }

    setWindowTitle(QString("迷宫生成与求解系统 - %1").arg(algorithmName(algo)));

    // 生成动画还没播完：结果先存着，等生成阶段播完（onAnimationFinished）再交给 MazeWidget。
    // 所以生成过程中切换求解算法、点击改起终点都没问题，不会打断生成动画。
    if (inGenerationPhase())
    {
        updateStats();
        return;
    }
    pushSolutionToWidget();
}

void MainWindow::pushSolutionToWidget()
{
    mazeWidget_->setEndpoints(start_, end_);
    mazeWidget_->setSolution(path_, visitedOrder_, parentOf_);
    updatePlayButton();
    updateStats();
}

void MainWindow::onAnimationFinished()
{
    if (inGenerationPhase())
        pushSolutionToWidget();  // 生成播完 -> 进入求解阶段（暂停在第 0 步，等用户点"播放求解"）
    else
    {
        updatePlayButton();
        updateStats();
    }
}

void MainWindow::openCompareDialog()
{
    if (maze_.empty()) return;
    mazeWidget_->pause();  // 主窗口的动画先停下，避免两边同时在跑

    const int cells = static_cast<int>(maze_.size() * maze_[0].size());
    const QString description = QString("%1 行 × %2 列，%3 生成")
        .arg(maze_.size()).arg(maze_[0].size()).arg(algorithmName(genAlgo_));

    CompareDialog dialog(maze_, start_, end_, description, cells <= kMaxCellsForRecursiveDfs, this);
    dialog.exec();  // 模态窗口：关闭后才回到主窗口
}

void MainWindow::onCellClicked(Point cell, Qt::MouseButton button)
{
    if (button == Qt::LeftButton) start_ = cell;
    else if (button == Qt::RightButton) end_ = cell;
    else return;
    solveCurrentMaze();  // 起终点变了，立刻重新求解
}

// 迷宫太大时把"DFS 递归"选项变灰，如果当前正选着它就自动切到非递归版
void MainWindow::updateRecursiveDfsAvailability()
{
    // 用迷宫的实际大小判断（而不是输入框的值：用户可能改了输入框但还没点"生成新迷宫"）
    const int cells = maze_.empty() ? 0 : static_cast<int>(maze_.size() * maze_[0].size());
    const bool allowed = cells <= kMaxCellsForRecursiveDfs;

    const int recursiveIndex = solveAlgoCombo_->findData(static_cast<int>(SolveAlgorithm::DFS_Recursive));
    if (auto* model = qobject_cast<QStandardItemModel*>(solveAlgoCombo_->model()))
    {
        if (QStandardItem* item = model->item(recursiveIndex))
        {
            item->setEnabled(allowed);
            item->setToolTip(allowed ? QString() : "迷宫超过 50×50 格时递归深度可能超出系统栈，已禁用");
        }
    }

    if (!allowed && solveAlgoCombo_->currentIndex() == recursiveIndex)
    {
        // 这里改下拉框会触发 currentIndexChanged -> solveCurrentMaze，
        // regenerateMaze 随后还会再求解一次，多算一次不影响结果
        solveAlgoCombo_->setCurrentIndex(
            solveAlgoCombo_->findData(static_cast<int>(SolveAlgorithm::DFS_Iterative)));
        noticeLabel_->setText("迷宫超过 50×50 格，递归 DFS 可能栈溢出，已自动切换为 DFS 非递归。");
        noticeLabel_->show();
    }
    else if (!allowed)
    {
        noticeLabel_->setText("迷宫超过 50×50 格，已禁用递归 DFS（递归深度可能超出系统栈）。");
        noticeLabel_->show();
    }
    else
    {
        noticeLabel_->hide();
    }
}

// ---------------- 面板刷新 ----------------

void MainWindow::onSpeedChanged(int sliderValue)
{
    const int steps = sliderToStepsPerSecond(sliderValue);
    mazeWidget_->setStepsPerSecond(steps);
    speedLabel_->setText(QString("%1 步/秒").arg(steps));
}

void MainWindow::updatePlayButton()
{
    // 按钮文字带上阶段，演示时一眼看出现在播的是"生成"还是"求解"
    const QString what = inGenerationPhase() ? "生成" : "求解";
    if (mazeWidget_->isPlaying()) playButton_->setText("暂停");
    else if (mazeWidget_->isFinished()) playButton_->setText("重新播放" + what);
    else if (mazeWidget_->currentStep() > 0) playButton_->setText("继续" + what);
    else playButton_->setText("播放" + what);

    const bool hasData = mazeWidget_->totalSteps() > 0;
    const bool finished = mazeWidget_->isFinished();
    playButton_->setEnabled(hasData);
    stepButton_->setEnabled(hasData && !finished);
    skipButton_->setEnabled(hasData && !finished);
    resetButton_->setEnabled(hasData && mazeWidget_->currentStep() > 0);
}

void MainWindow::updateStats()
{
    if (maze_.empty())
    {
        statsLabel_->setText("（尚未生成迷宫）");
        return;
    }

    const int rows = static_cast<int>(maze_.size());
    const int cols = static_cast<int>(maze_[0].size());
    const int totalCells = rows * cols;
    const int step = mazeWidget_->currentStep();
    const bool generating = inGenerationPhase();

    QString html = "<table cellspacing='3'>";
    auto row = [&html](const QString& key, const QString& value) {
        html += QString("<tr><td>%1</td><td>%2</td></tr>").arg(key, value);
    };
    const QString gray = "<span style='color:#888;'>%1</span>";

    row("当前阶段：", generating ? "<b>生成迷宫</b>" : "<b>求解迷宫</b>");
    row("迷宫规模：", QString("%1 行 × %2 列（共 %3 格）").arg(rows).arg(cols).arg(totalCells));
    row("生成算法：", QString("%1，拆墙 %2 次").arg(algorithmName(genAlgo_)).arg(genStepCount_));
    row("生成耗时：", QString("%1 µs ").arg(genMicros_, 0, 'f', 1) + gray.arg("（不含动画）"));

    if (generating)
    {
        row("拆墙进度：", QString("%1 / %2 步").arg(step).arg(mazeWidget_->totalSteps()));
        row("求解算法：", algorithmName(currentSolveAlgorithm()) + " " + gray.arg("（生成播完后开始）"));
    }
    else
    {
        const int visitedTotal = static_cast<int>(visitedOrder_.size());
        const double percent = totalCells > 0 ? 100.0 * visitedTotal / totalCells : 0.0;
        const QString pathText = path_.empty()
            ? QString("<span style='color:#b22222;'>终点不可达</span>")
            : QString("%1 格").arg(path_.size());

        row("求解算法：", "<b>" + algorithmName(currentSolveAlgorithm()) + "</b>");
        row("起点 → 终点：", QString("(%1,%2) → (%3,%4)").arg(start_.row).arg(start_.col).arg(end_.row).arg(end_.col));
        row("动画进度：", QString("%1 / %2 步").arg(step).arg(visitedTotal));
        row("总访问格数：", QString("%1 格（占 %2%）").arg(visitedTotal).arg(percent, 0, 'f', 1));
        row("路径长度：", pathText);
        row("求解耗时：", QString("%1 µs ").arg(solveMicros_, 0, 'f', 1) + gray.arg("（不含动画）"));
    }
    html += "</table>";
    statsLabel_->setText(html);

    updatePlayButton();
}

SolveAlgorithm MainWindow::currentSolveAlgorithm() const
{
    return static_cast<SolveAlgorithm>(solveAlgoCombo_->currentData().toInt());
}

QString MainWindow::algorithmName(SolveAlgorithm algo)
{
    switch (algo)
    {
    case SolveAlgorithm::BFS:           return "BFS";
    case SolveAlgorithm::DFS_Recursive: return "DFS 递归";
    case SolveAlgorithm::DFS_Iterative: return "DFS 非递归";
    }
    return "未知";
}

QString MainWindow::algorithmName(GenAlgorithm algo)
{
    switch (algo)
    {
    case GenAlgorithm::Kruskal: return "Kruskal";
    case GenAlgorithm::Prim:    return "Prim";
    }
    return "未知";
}
