#ifndef GUI_H
#define GUI_H

#include <QWidget>
#include<QTimer>
#include<QPushButton>
#include<vector>
#include "maze_types.h"

class MazeWidget : public QWidget
//QWidget是Qt里所有可见界面元素的基类
{
    Q_OBJECT  // 这个宏是Qt信号槽机制必需的，缺了会导致MOC编译报错

public:
    explicit MazeWidget(QWidget* parent = nullptr);//防止编译器做隐式类型转换

    void setMaze(const MazeGrid& maze);  // 外部调用，把生成好的迷宫传进来显示

    void setSolution(const std::vector<Point>& path,const std::vector<Point>& visitedOrder, const std::vector<Point>& parentOf);//接收求解结果
    void startAnimation();  // 开始播放访问过程动画（现在由界面上的"开始演示"按钮触发，外部也仍可直接调用）


protected:
    void paintEvent(QPaintEvent* event) override;  // Qt会自动调用这个函数来绘制界面，回调机制

private:
    MazeGrid maze_;
    std::vector<Point> path_;
    std::vector<Point> visitedOrder_;
    std::vector<Point> parentOf_;

    QTimer* timer_;//定时器指针，用来驱动动画
    QPushButton* startButton_;//新增：手动点击触发动画开始的按钮
    int animationStep_ = 0;  // 当前动画播放到第几步。用来记录动画播放进度，每次定时器触发就+1

    int cellSize_ = 30;
    int legendHeight_ = 70;//底部图例预留的像素高度
    int topBarHeight_ = 40;//新增：顶部给按钮预留的像素高度
};

#endif
