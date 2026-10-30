#include "game_page.h"
#include "game_widget.h"
#include "theme.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace { constexpr double kPi = 3.14159265358979323846; }  // MSVC 默认不提供圆周率常量

// 三颗星：亮几颗由 setStars 决定（金色实心 / 浅色空心）
class StarRow : public QWidget
{
public:
    explicit StarRow(QWidget* parent = nullptr) : QWidget(parent) { setFixedSize(190, 62); }
    void setStars(int n) { stars_ = n; update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 3; ++i)
        {
            const bool lit = i < stars_;
            const double outer = (i == 1) ? 28 : 22;           // 中间那颗大一点
            const QPointF c(35 + i * 60, (i == 1) ? 30 : 36);
            QPainterPath path;
            for (int k = 0; k < 10; ++k)
            {
                const double r = (k % 2 == 0) ? outer : outer * 0.46;
                const double a = -kPi / 2 + k * kPi / 5;
                const QPointF pt(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
                if (k == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            path.closeSubpath();
            p.setPen(QPen(lit ? QColor("#E0A800") : QColor("#E8DCC8"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(lit ? QColor("#FFD93D") : QColor("#F6EFE3"));
            p.drawPath(path);
        }
    }

private:
    int stars_ = 0;
};

GamePage::GamePage(QWidget* parent) : QWidget(parent)
{
    // 关卡表：名字、宽、高、环路比例、种子、生成算法
    levels_ = {
        { "新手营地", 8, 8, 0.00, 11, GenAlgorithm::Kruskal },
        { "林间小径", 10, 10, 0.00, 23, GenAlgorithm::Prim },
        { "蘑菇山谷", 12, 12, 0.06, 37, GenAlgorithm::Kruskal },
        { "古树迷阵", 15, 15, 0.08, 41, GenAlgorithm::Prim },
        { "遗迹深处", 18, 18, 0.10, 59, GenAlgorithm::Kruskal },
        { "宝藏神殿", 22, 22, 0.12, 73, GenAlgorithm::Prim },
    };
    bestStars_.assign(levels_.size(), 0);

    game_ = new GameWidget(this);

    // ================= 左侧面板 =================
    auto* panel = new QWidget(this);
    panel->setFixedWidth(270);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel(
        "<div style='font-size:20pt; font-weight:bold; color:#E07B1A;'>宝藏探险家</div>"
        "<div style='color:#A08770;'>带小探险家穿过迷宫，找到宝箱！</div>", panel);
    title->setTextFormat(Qt::RichText);

    // ---- 关卡选择 ----
    auto* levelBox = new QGroupBox("选择关卡", panel);
    auto* levelGrid = new QGridLayout(levelBox);
    levelGrid->setSpacing(8);
    levelGroup_ = new QButtonGroup(this);
    levelGroup_->setExclusive(true);
    for (int i = 0; i < static_cast<int>(levels_.size()); ++i)
    {
        auto* b = new QPushButton(levelBox);
        b->setObjectName("levelButton");
        b->setCheckable(true);
        levelGrid->addWidget(b, i / 3, i % 3);
        levelGroup_->addButton(b, i);
        levelButtons_.push_back(b);
    }
    levelInfo_ = new QLabel(levelBox);
    levelInfo_->setWordWrap(true);
    levelGrid->addWidget(levelInfo_, 2, 0, 1, 3);

    // ---- 冒险记录（步数 / 用时 大号数字） ----
    auto* hudBox = new QGroupBox("冒险记录", panel);
    auto* hud = new QGridLayout(hudBox);
    auto bigNumber = [hudBox](const QString& caption, QLabel*& valueOut) {
        auto* w = new QWidget(hudBox);
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(0);
        valueOut = new QLabel("0", w);
        valueOut->setAlignment(Qt::AlignCenter);
        valueOut->setStyleSheet("font-size: 24pt; font-weight: bold; color: #5DADE2;");
        auto* cap = new QLabel(caption, w);
        cap->setAlignment(Qt::AlignCenter);
        cap->setStyleSheet("color: #A08770;");
        l->addWidget(valueOut);
        l->addWidget(cap);
        return w;
    };
    hud->addWidget(bigNumber("步数", stepsLabel_), 0, 0);
    hud->addWidget(bigNumber("用时（秒）", timeLabel_), 0, 1);

    restartButton_ = new QPushButton("重新开始（R）", hudBox);
    pathButton_ = new QPushButton("显示最短路线", hudBox);
    pathButton_->setObjectName("accentButton");
    pathButton_->setCheckable(true);
    resultButton_ = new QPushButton("再看结算", hudBox);
    hud->addWidget(restartButton_, 1, 0, 1, 2);
    hud->addWidget(pathButton_, 2, 0);
    hud->addWidget(resultButton_, 2, 1);

    // ---- 小贴士 ----
    auto* tipBox = new QGroupBox("小贴士", panel);
    auto* tipLayout = new QVBoxLayout(tipBox);
    auto* tip = new QLabel(
        "方向键 / WASD 移动，撞墙不算步数。<br>"
        "通关后可以打开 <b style='color:#E0A800;'>最短路线</b>：那条金色虚线是 "
        "<b>BFS 广度优先搜索</b> 算出来的，看看你比它多走了几步？<br>"
        "后面的关卡有环路，岔路更多哦～", tipBox);
    tip->setWordWrap(true);
    tip->setTextFormat(Qt::RichText);
    tipLayout->addWidget(tip);

    panelLayout->addWidget(title);
    panelLayout->addWidget(levelBox);
    panelLayout->addWidget(hudBox);
    panelLayout->addWidget(tipBox);
    panelLayout->addStretch();

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(panel, 0, Qt::AlignTop);
    layout->addWidget(game_, 0, Qt::AlignTop);

    // ================= 结算卡（盖在游戏画布上面） =================
    resultCard_ = new QFrame(game_);
    resultCard_->setObjectName("resultCard");
    resultCard_->setStyleSheet(
        "QFrame#resultCard { background: #FFFFFF; border: 3px solid #FFB65C; border-radius: 22px; }");
    auto* shadow = new QGraphicsDropShadowEffect(resultCard_);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(90, 62, 43, 90));
    resultCard_->setGraphicsEffect(shadow);
    resultCard_->setFixedWidth(330);

    auto* card = new QVBoxLayout(resultCard_);
    card->setContentsMargins(22, 18, 22, 18);
    card->setSpacing(8);
    resultTitle_ = new QLabel("找到宝箱啦！", resultCard_);
    resultTitle_->setAlignment(Qt::AlignCenter);
    resultTitle_->setStyleSheet("font-size: 19pt; font-weight: bold; color: #E07B1A; border: none;");
    stars_ = new StarRow(resultCard_);
    resultStats_ = new QLabel(resultCard_);
    resultStats_->setTextFormat(Qt::RichText);
    resultStats_->setAlignment(Qt::AlignCenter);
    resultStats_->setStyleSheet("border: none;");
    resultComment_ = new QLabel(resultCard_);
    resultComment_->setWordWrap(true);
    resultComment_->setAlignment(Qt::AlignCenter);
    resultComment_->setStyleSheet("color: #A08770; border: none;");

    auto* cardButtons = new QHBoxLayout();
    auto* viewPathButton = new QPushButton("看最短路线", resultCard_);
    viewPathButton->setObjectName("accentButton");
    auto* againButton = new QPushButton("再玩一次", resultCard_);
    nextButton_ = new QPushButton("下一关 ▶", resultCard_);
    nextButton_->setObjectName("primaryButton");
    cardButtons->addWidget(viewPathButton);
    cardButtons->addWidget(againButton);
    cardButtons->addWidget(nextButton_);

    card->addWidget(resultTitle_);
    card->addWidget(stars_, 0, Qt::AlignHCenter);
    card->addWidget(resultStats_);
    card->addWidget(resultComment_);
    card->addLayout(cardButtons);
    resultCard_->hide();

    // ================= 信号连接 =================
    connect(levelGroup_, &QButtonGroup::idClicked, this, &GamePage::loadLevel);
    connect(game_, &GameWidget::stepsChanged, this, [this](int s) { stepsLabel_->setText(QString::number(s)); });
    connect(game_, &GameWidget::reachedGoal, this, &GamePage::onReachedGoal);
    connect(game_, &GameWidget::restartRequested, this, [this]() { loadLevel(current_); });
    connect(restartButton_, &QPushButton::clicked, this, [this]() { loadLevel(current_); });
    connect(pathButton_, &QPushButton::toggled, this, [this](bool on) {
        game_->setShowShortestPath(on);
        pathButton_->setText(on ? "隐藏最短路线" : "显示最短路线");
        if (on) showResultCard(false);
        game_->setFocus();
    });
    connect(resultButton_, &QPushButton::clicked, this, [this]() { showResultCard(true); });
    connect(viewPathButton, &QPushButton::clicked, this, [this]() { pathButton_->setChecked(true); });
    connect(againButton, &QPushButton::clicked, this, [this]() { loadLevel(current_); });
    connect(nextButton_, &QPushButton::clicked, this, [this]() {
        loadLevel(std::min(current_ + 1, static_cast<int>(levels_.size()) - 1));
    });

    // 用时每 0.1 秒刷新一次
    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(100);
    connect(clockTimer_, &QTimer::timeout, this, [this]() {
        timeLabel_->setText(QString::number(game_->elapsedSeconds(), 'f', 1));
    });
    clockTimer_->start();

    loadLevel(0);
}

void GamePage::focusGame() { game_->setFocus(); }

void GamePage::loadLevel(int index)
{
    current_ = index;
    const Level& lv = levels_[index];
    // A 的带种子 + 环路比例接口：同一关永远是同一个迷宫
    const MazeGrid maze = generateMaze(lv.width, lv.height, lv.algo, lv.seed, lv.loopRate);
    game_->loadLevel(maze, { 0, 0 }, { lv.height - 1, lv.width - 1 });

    levelButtons_[index]->setChecked(true);
    levelInfo_->setText(QString("<b>第 %1 关 · %2</b><br>"
                                "<span style='color:#A08770;'>%3 × %4 格，%5</span>")
                            .arg(index + 1).arg(lv.name).arg(lv.width).arg(lv.height)
                            .arg(lv.loopRate > 0 ? QString("有环路（岔路更多）") : QString("没有环路")));

    pathButton_->setChecked(false);
    pathButton_->setEnabled(false);   // 通关后才能看最短路线
    resultButton_->setEnabled(false);
    showResultCard(false);
    stepsLabel_->setText("0");
    timeLabel_->setText("0.0");
    refreshLevelButtons();
    game_->setFocus();
}

int GamePage::starsFor(int steps, int shortestSteps)
{
    if (steps <= 0 || shortestSteps <= 0) return 3;
    const double efficiency = static_cast<double>(shortestSteps) / steps;
    if (efficiency >= 0.95) return 3;
    if (efficiency >= 0.70) return 2;
    return 1;
}

void GamePage::onReachedGoal(int steps, int shortestSteps, double seconds)
{
    const int stars = starsFor(steps, shortestSteps);
    bestStars_[current_] = std::max(bestStars_[current_], stars);
    refreshLevelButtons();

    const double efficiency = steps > 0 ? 100.0 * shortestSteps / steps : 100.0;
    stars_->setStars(stars);
    resultStats_->setText(QString(
        "<table cellspacing='4' align='center'>"
        "<tr><td align='right'>你走了：</td><td><b style='font-size:13pt;'>%1</b> 步</td></tr>"
        "<tr><td align='right'>最短路线：</td><td><b style='font-size:13pt; color:#E0A800;'>%2</b> 步"
        "<span style='color:#A08770;'>（BFS 算出）</span></td></tr>"
        "<tr><td align='right'>探索效率：</td><td><b style='font-size:13pt; color:#5DADE2;'>%3%</b></td></tr>"
        "<tr><td align='right'>用时：</td><td>%4 秒</td></tr>"
        "</table>")
        .arg(steps).arg(shortestSteps).arg(efficiency, 0, 'f', 0).arg(seconds, 0, 'f', 1));

    static const char* comments[] = {
        "",
        "成功通关！打开金色的最短路线看看，下次争取少走点弯路～",
        "很棒！离最短路线只差一点点了～",
        "完美！你走的路和 BFS 找到的最短路线一样短！",
    };
    resultComment_->setText(comments[stars]);
    nextButton_->setEnabled(current_ + 1 < static_cast<int>(levels_.size()));
    nextButton_->setText(nextButton_->isEnabled() ? "下一关 ▶" : "全部通关！");

    pathButton_->setEnabled(true);
    resultButton_->setEnabled(true);
    // 等撒花飞一会儿再弹出结算卡
    QTimer::singleShot(700, this, [this]() { if (game_->hasWon()) showResultCard(true); });
}

void GamePage::showResultCard(bool show)
{
    if (show)
    {
        if (pathButton_->isChecked()) pathButton_->setChecked(false);
        resultCard_->adjustSize();
        positionResultCard();
        resultCard_->show();
        resultCard_->raise();
    }
    else
    {
        resultCard_->hide();
        game_->setFocus();
    }
}

void GamePage::positionResultCard()
{
    resultCard_->move((game_->width() - resultCard_->width()) / 2,
                      (game_->height() - resultCard_->height()) / 2);
}

void GamePage::refreshLevelButtons()
{
    for (int i = 0; i < static_cast<int>(levelButtons_.size()); ++i)
    {
        QString stars;
        for (int k = 0; k < 3; ++k) stars += (k < bestStars_[i]) ? QString::fromUtf8("★") : QString::fromUtf8("☆");
        levelButtons_[i]->setText(QString("%1\n%2").arg(i + 1).arg(stars));
    }
}
