#include <QApplication>
#include "src/app_window.h"
#include "src/theme.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    applyAppTheme(app);  // 卡通主题：配色和样式统一在 src/theme.h 里

    // 两个页签：闯关冒险（玩家自己走迷宫）+ 算法实验室（生成/求解动画与算法对比）
    AppWindow window;
    window.show();

    return app.exec();//事件循环
}