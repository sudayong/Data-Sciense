/**
 * @file gui_logic_讲解.h
 * @brief Testable geometry, interaction, and status logic used by the GUI.
 * @author Hao Guo
 * @author Junke Pu
 */

#pragma once
//保证这个头文件在同一个编译单元中只被展开一次，避免结构体和函数声明重复定义。
//Makes this header expand only once in one translation unit.

#include <optional>  //提供 std::optional，用来表示“可能有一个结果，也可能没有结果”。
#include <string>    //提供 std::string，用来保存玩家名字和状态文字。
#include <vector>    //提供 std::vector，用来保存坐标标签和获胜路径等动态数组。

#include "bruecken/board.h"  //提供 Board 棋盘类，以及 Position、GamePhase 等项目类型。
#include "move.h"            //提供 preset::Move，表示一次带坐标和玩家编号的落子。

namespace bruecken {
//下面的类型和函数都放在 bruecken 命名空间中，避免和其他库的同名内容冲突。
//The following types and functions belong to the bruecken namespace.

/** A renderer-independent point in screen coordinates. */
struct GuiPoint {
    //这个结构体只保存屏幕上的二维坐标，不依赖 Raylib。
    //所以 gui_logic.cpp 可以在不开窗口的单元测试中使用它。
    //This 2D screen point does not depend on Raylib, so it is testable.
    float x = 0.0F;
    float y = 0.0F;
    //0.0F 中的 F 表示这是 float 字面量，而不是默认的 double。
    //The F suffix makes the literal a float instead of a double.
};

/** Complete screen-space geometry of a rotated board in a fixed grid frame. */
struct GuiBoardGeometry {
    //grid_* 是固定矩形网格的四个角，棋盘旋转时这些点不移动。
    //grid_* stores the four fixed grid corners; rotation does not move them.
    GuiPoint grid_top_left;
    GuiPoint grid_top_right;
    GuiPoint grid_bottom_left;
    GuiPoint grid_bottom_right;

    //下面四个点是旋转后实际棋盘边界的四个角。
    //These four points are the corners of the rotated board boundary.
    GuiPoint top_left;
    GuiPoint top_right;
    GuiPoint bottom_left;
    GuiPoint bottom_right;

    //x_step 表示棋盘 x 坐标增加 1 时，屏幕坐标要移动多少。
    //y_step 对 y 坐标做同样的事情。
    //x_step and y_step map one board-coordinate step to the screen.
    GuiPoint x_step;
    GuiPoint y_step;

    //center 保存固定网格的中心，绘制外侧坐标标签时会用到。
    //center stores the fixed-grid center, used to place outer labels.
    GuiPoint center;
};

/**
 * Calculate fixed-grid and rotated-board corners according to the school formula.
 * @par Primary contributor
 * Hao Guo (rotation and board rendering geometry).
 */
GuiBoardGeometry calculate_board_geometry(
    const Board& board,
    //const Board& 表示只借用棋盘，不复制，也不允许在函数中修改它。
    //const Board& borrows the board without copying or modifying it.
    float screen_width,
    float screen_height);
    //函数返回一整份 GuiBoardGeometry，供绘制和鼠标坐标计算共同使用。
    //Returns all geometry shared by rendering and mouse calculations.

/** Convert a fixed integer board coordinate into a screen coordinate. */
GuiPoint board_coordinate_to_screen(
    const GuiBoardGeometry& geometry,
    int x,
    int y);
    //把整数棋盘坐标 (x, y) 转换成对应的屏幕像素坐标。
    //Converts an integer board coordinate to its screen pixel position.

/**
 * Return the board point under the mouse, if one is close enough.
 * @par Primary contributor
 * Junke Pu (mouse interaction).
 */
std::optional<Position> find_nearest_board_position(
    const GuiBoardGeometry& geometry,
    const Board& board,
    GuiPoint mouse);
    //Junke Pu：寻找鼠标附近可选择的棋盘坐标，找不到时返回空 optional。

/** Convert a clicked coordinate into a valid move for the current player. */
std::optional<preset::Move> move_for_board_position(
    const Board& board,
    Position position);
    //Junke Pu：把点击位置转换成合法落子；位置无效时不返回 Move。

/**
 * Return readable persistent axis labels, always including both endpoints.
 * Exact coordinates between these labels are exposed by the hover tooltip.
 * @par Primary contributor
 * Hao Guo (coordinate rendering).
 */
std::vector<int> coordinate_label_values(
    int coordinate_count,
    float point_spacing);
    //根据坐标总数和相邻点的像素距离，返回适合显示的标签编号。
    //Selects readable label values from the count and pixel spacing.
    //标签太密时会跳号，但一定包含 0 和最后一个坐标。
    //Dense labels are skipped, but 0 and the last coordinate stay visible.

/**
 * Reconstruct one winning bridge path for a player.
 * @par Primary contributor
 * Junke Pu (winning-state visualization).
 */
std::vector<Position> calculate_winning_path(
    const Board& board,
    int player_id);
    //Junke Pu：用棋子和桥重建一条胜利路径，供结束画面高亮。

/**
 * Build the status text shown above the board.
 * @par Primary contributor
 * Junke Pu (game-state display).
 */
std::string game_status_text(
    const Board& board,
    const std::vector<std::string>& player_names);
    //Junke Pu：根据当前回合、胜负或平局生成状态栏文字。

}  // namespace bruecken
//结束 bruecken 命名空间。
//Ends the bruecken namespace.
