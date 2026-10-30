#ifndef THEME_H
#define THEME_H

/*
 * 界面主题：卡通、可爱的"小探险家找宝箱"风格。
 *
 * 浅色马卡龙配色 + 圆润的糖果按钮 + 草地/树篱迷宫。
 * 所有颜色和样式表（QSS）都集中在这一个文件里，想换风格只改这里。
 * 只有头文件、没有 .cpp，新增它不需要重新配置 CMake。
 *
 * 用法：main.cpp 里在创建窗口前调用一次 applyAppTheme(app);
 */

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStyleFactory>

namespace Theme
{
    // ---- 界面底色与文字 ----
    inline const QColor kWindowBg("#FFF6E5");   // 奶油色窗口底
    inline const QColor kCardBg("#FFFFFF");     // 白色卡片
    inline const QColor kInputBg("#FFFBF3");
    inline const QColor kBorder("#F0D9B5");     // 卡片描边：浅焦糖
    inline const QColor kText("#5A3E2B");       // 深棕色文字（比纯黑更柔和）
    inline const QColor kTextMuted("#A08770");
    inline const QColor kAccent("#FF9F43");     // 橙色强调
    inline const QColor kWarn("#D98E04");

    // ---- 闯关画布：森林草地 ----
    inline const QColor kGrassA("#C8EBA0");     // 草地棋盘格（两种深浅交替）
    inline const QColor kGrassB("#BEE494");
    inline const QColor kHedge("#4E9F3D");      // 树篱墙
    inline const QColor kHedgeLight("#7CC46A"); // 树篱高光
    inline const QColor kFootprint("#E2B77A");  // 走过的脚印
    inline const QColor kGoldPath("#FFC93C");   // BFS 最短路线（金色）
    inline const QColor kCanvasFrame("#8FC46F");

    // ---- 算法实验室画布（同样换成浅色卡通） ----
    inline const QColor kCanvasBg("#FFF9EE");
    inline const QColor kWall("#8B5E3C");       // 巧克力色墙
    inline const QColor kStart("#4CAF50");
    inline const QColor kEnd("#FF5E5E");
    inline const QColor kCrimson("#FF5E5E");
    inline const QColor kParentLine(139, 94, 60, 60);
    inline const QColor kCurrent("#FFE066");    // 当前探索格：柠檬黄
    inline const QColor kCarved("#FFE3B3");     // 生成阶段：已打通
    inline const QColor kJustBroken("#FF9F43"); // 生成阶段：本步拆墙

    // 实验室热力图色带：离起点越近越偏薄荷绿，越远越偏粉紫（马卡龙色系）
    inline QColor heatColor(double t)
    {
        static const QColor stops[] = {
            QColor("#A8E6CF"),   // 薄荷
            QColor("#9AD3F0"),   // 天蓝
            QColor("#C3B1E1"),   // 薰衣草
            QColor("#F7A8C4"),   // 樱花粉
        };
        constexpr int n = 4;
        if (t <= 0.0) return stops[0];
        if (t >= 1.0) return stops[n - 1];
        const double pos = t * (n - 1);
        const int i = static_cast<int>(pos);
        const double f = pos - i;
        const QColor& a = stops[i];
        const QColor& b = stops[i + 1];
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f,
                                a.greenF() + (b.greenF() - a.greenF()) * f,
                                a.blueF() + (b.blueF() - a.blueF()) * f);
    }
}

// 给整个程序套上卡通主题：Fusion 风格 + 浅色调色板 + QSS 细节样式
inline void applyAppTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window, Theme::kWindowBg);
    p.setColor(QPalette::WindowText, Theme::kText);
    p.setColor(QPalette::Base, Theme::kInputBg);
    p.setColor(QPalette::AlternateBase, Theme::kWindowBg);
    p.setColor(QPalette::Text, Theme::kText);
    p.setColor(QPalette::Button, Theme::kCardBg);
    p.setColor(QPalette::ButtonText, Theme::kText);
    p.setColor(QPalette::Highlight, Theme::kAccent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, Theme::kCardBg);
    p.setColor(QPalette::ToolTipText, Theme::kText);
    p.setColor(QPalette::Disabled, QPalette::Text, Theme::kTextMuted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, Theme::kTextMuted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, Theme::kTextMuted);
    app.setPalette(p);

    // "幼圆"是中文 Windows 自带的圆体字，很适合卡通风格；没有的话退回微软雅黑
    app.setStyleSheet(R"(
        QWidget {
            font-family: "YouYuan", "幼圆", "Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC", sans-serif;
            color: #5A3E2B;
        }
        QGroupBox {
            background: #FFFFFF;
            border: 2px solid #F0D9B5;
            border-radius: 16px;
            margin-top: 12px;
            padding: 6px 6px 5px 6px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 1px 8px;
            color: #FFFFFF;
            background: #FFB65C;
            border-radius: 8px;
        }

        /* 糖果按钮：圆角 + 底部加深一条，看起来像能按下去的实体按钮 */
        QPushButton {
            background: #FFF1D6;
            border: 2px solid #F0D9B5;
            border-bottom: 4px solid #E5C48F;
            border-radius: 12px;
            padding: 3px 12px;
            font-weight: bold;
        }
        QPushButton:hover    { background: #FFE8BD; }
        QPushButton:pressed  { border-bottom: 2px solid #E5C48F; padding-top: 5px; }
        QPushButton:disabled { background: #F6EFE3; color: #C9B8A3; border-color: #EFE3CF; }

        /* 主按钮：橙色 */
        QPushButton#primaryButton {
            background: #FF9F43; color: white;
            border: 2px solid #F08A28; border-bottom: 4px solid #D9701A;
        }
        QPushButton#primaryButton:hover   { background: #FFB062; }
        QPushButton#primaryButton:pressed { border-bottom: 2px solid #D9701A; }

        /* 强调按钮：天蓝 */
        QPushButton#accentButton {
            background: #5DADE2; color: white;
            border: 2px solid #4A9BD1; border-bottom: 4px solid #337FB3;
        }
        QPushButton#accentButton:hover    { background: #74BCEB; }
        QPushButton#accentButton:pressed  { border-bottom: 2px solid #337FB3; }
        QPushButton#accentButton:disabled { background: #BFDDF0; color: #EAF4FB; border-color: #BFDDF0; }

        /* 关卡按钮：圆圆的大按钮 */
        QPushButton#levelButton {
            background: #FFFFFF; min-width: 50px; min-height: 46px; padding: 2px 4px;
            border: 2px solid #F0D9B5; border-bottom: 4px solid #E5C48F; border-radius: 14px;
        }
        QPushButton#levelButton:checked {
            background: #FFE08A; border-color: #F5B82E; border-bottom-color: #D99A12;
        }

        QSpinBox, QComboBox {
            background: #FFFBF3;
            border: 2px solid #F0D9B5;
            border-radius: 8px;
            padding: 2px 6px;
        }
        QSpinBox:focus, QComboBox:focus { border-color: #FFB65C; }
        QComboBox QAbstractItemView {
            background: #FFFFFF; border: 2px solid #F0D9B5; selection-background-color: #FFE08A;
            selection-color: #5A3E2B;
        }

        QSlider::groove:horizontal { height: 8px; background: #F3E3C7; border-radius: 4px; }
        QSlider::sub-page:horizontal { background: #FFB65C; border-radius: 4px; }
        QSlider::handle:horizontal {
            background: #FFFFFF; border: 2px solid #FF9F43; width: 16px; margin: -6px 0; border-radius: 10px;
        }

        QTabWidget::pane { border: none; top: -2px; }
        QTabBar::tab {
            background: #FFE8BD; color: #8B6A4E;
            border: 2px solid #F0D9B5; border-bottom: none;
            border-top-left-radius: 14px; border-top-right-radius: 14px;
            padding: 8px 22px; margin-right: 4px; font-weight: bold; font-size: 11pt;
        }
        QTabBar::tab:selected { background: #FF9F43; color: white; border-color: #F08A28; }

        QTableWidget {
            background: #FFFFFF; gridline-color: #F3E3C7;
            border: 2px solid #F0D9B5; border-radius: 10px;
        }
        QHeaderView::section {
            background: #FFF1D6; color: #8B6A4E; border: none;
            border-right: 1px solid #F0D9B5; padding: 5px; font-weight: bold;
        }
        QToolTip { background: #FFFFFF; color: #5A3E2B; border: 2px solid #FFB65C; padding: 4px; }
    )");
}

#endif
