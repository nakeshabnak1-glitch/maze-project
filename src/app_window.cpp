#include "app_window.h"
#include "game_page.h"
#include "main_window.h"

#include <QTabWidget>
#include <QVBoxLayout>

AppWindow::AppWindow(QWidget* parent) : QWidget(parent)
{
    setWindowTitle("宝藏探险家 · 迷宫生成与求解");

    tabs_ = new QTabWidget(this);
    game_ = new GamePage(tabs_);
    lab_ = new MainWindow(tabs_);
    tabs_->addTab(game_, "闯关冒险");
    tabs_->addTab(lab_, "算法实验室");
    tabs_->setFocusPolicy(Qt::NoFocus);  // 方向键留给游戏，不要被页签栏抢走

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addWidget(tabs_);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // 切回游戏页签时，让游戏画布重新拿到键盘焦点
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 0) game_->focusGame();
    });
}
