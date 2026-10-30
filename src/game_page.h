#ifndef GAME_PAGE_H
#define GAME_PAGE_H

#include <QWidget>
#include <vector>
#include "maze_generator.h"

class GameWidget;
class QLabel;
class QPushButton;
class QButtonGroup;
class QFrame;
class QTimer;
class StarRow;

/*
 * GamePage："闯关冒险"页签 = 左侧（标题、关卡选择、冒险记录、小贴士）+ 右侧游戏画布。
 *
 * 关卡用 A 的 generateMaze(宽, 高, 算法, 种子, 环路比例) 生成：
 *   - 种子固定 → 每个人玩到的同一关是同一个迷宫，可以比成绩
 *   - 越往后越大、环路越多（岔路多，最短路线不止一条，更考验判断）
 *
 * 通关后弹出结算卡：你的步数 vs BFS 最短步数 → 探索效率 → 1~3 颗星。
 * 可以叠加显示 BFS 最短路线（金色虚线），和自己留下的脚印对比。
 */
class GamePage : public QWidget
{
    Q_OBJECT

public:
    explicit GamePage(QWidget* parent = nullptr);
    void focusGame();                         // 切到这个页签时让画布拿到键盘焦点

private:
    struct Level
    {
        QString name;
        int width;
        int height;
        double loopRate;
        unsigned seed;
        GenAlgorithm algo;
    };

    void loadLevel(int index);
    void onReachedGoal(int steps, int shortestSteps, double seconds);
    void showResultCard(bool show);
    void positionResultCard();
    void refreshLevelButtons();
    static int starsFor(int steps, int shortestSteps);

    std::vector<Level> levels_;
    std::vector<int> bestStars_;              // 每关最好成绩（本次运行内）
    int current_ = 0;

    GameWidget* game_ = nullptr;
    QButtonGroup* levelGroup_ = nullptr;
    std::vector<QPushButton*> levelButtons_;
    QLabel* levelInfo_ = nullptr;
    QLabel* stepsLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QPushButton* restartButton_ = nullptr;
    QPushButton* pathButton_ = nullptr;
    QPushButton* resultButton_ = nullptr;
    QTimer* clockTimer_ = nullptr;

    // ---- 结算卡 ----
    QFrame* resultCard_ = nullptr;
    StarRow* stars_ = nullptr;
    QLabel* resultTitle_ = nullptr;
    QLabel* resultStats_ = nullptr;
    QLabel* resultComment_ = nullptr;
    QPushButton* nextButton_ = nullptr;
};

#endif
