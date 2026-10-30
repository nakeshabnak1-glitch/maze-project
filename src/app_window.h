#ifndef APP_WINDOW_H
#define APP_WINDOW_H

#include <QWidget>

class QTabWidget;
class GamePage;
class MainWindow;

/*
 * AppWindow：程序的顶层窗口，两个页签：
 *   1. 闯关冒险（GamePage）：玩家自己走迷宫找宝箱，通关后和 BFS 最短路线比较
 *   2. 算法实验室（MainWindow）：生成动画、求解动画、生成算法对比、求解算法对比 —— 讲解算法用
 */
class AppWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);

private:
    QTabWidget* tabs_ = nullptr;
    GamePage* game_ = nullptr;
    MainWindow* lab_ = nullptr;
};

#endif
