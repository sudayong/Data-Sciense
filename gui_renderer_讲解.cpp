/**
 * @file gui_renderer_讲解.cpp
 * @brief Implements Raylib rendering, status display, and mouse input.
 * @author Hao Guo
 * @author Junke Pu
 *
 * @par Contribution breakdown
 * - Hao Guo: GUI window initialization and lifetime; board, peg, and bridge
 *   rendering.
 * - Junke Pu: GUI interaction and move placement; game-state display; the
 *   preset::PlayerGuiAccess implementation.
 */

#include "bruecken/gui_renderer.h"  //引入 GuiRenderer 类声明，保证这里的实现与头文件接口一致。

#include <algorithm>  //提供 std::min、std::max 和 std::clamp 等通用算法。
#include <cmath>      //提供 std::sqrt 和 std::abs，用于向量长度与浮点数判断。
#include <exception>  //提供 std::exception，解析颜色失败时统一捕获标准异常。
#include <optional>   //提供 std::optional，表示可能存在的鼠标位置或落子。
#include <stdexcept>  //提供 std::runtime_error，窗口初始化失败时报告错误。
#include <string>     //提供 std::string，保存颜色、玩家名、标题和反馈文本。
#include <utility>    //提供 std::move，把构造参数中的字符串容器转移给成员变量。
#include <vector>     //提供 std::vector，保存玩家颜色、名字和获胜路径。

#include "bruecken/common.h"     //提供颜色默认值、窗口大小、玩家数量以及棋子和桥等类型。
#include "bruecken/gui_logic.h"  //提供可测试的棋盘几何、坐标标签和交互辅助函数。
#include "move.h"                //提供 preset::Move，表示一次落子。
#include "raylib.h"              //提供窗口、鼠标、颜色、Vector2 和所有 Draw... 绘图函数。

namespace bruecken {
namespace {
//匿名命名空间中的工具、常量和结构体只供这个 cpp 文件内部使用。
//Items in this anonymous namespace are private to this cpp file.

// Visual palette used by Hao Guo's renderer and Junke Pu's status panel.
constexpr Color kBackground{241, 245, 249, 255};
constexpr Color kBoardBackground{255, 255, 255, 255};
constexpr Color kGridColor{148, 163, 184, 130};
constexpr Color kOuterBoundaryColor{100, 116, 139, 210};
constexpr Color kTextColor{30, 41, 59, 255};
constexpr Color kSecondaryText{100, 116, 139, 255};
constexpr Color kInactivePointColor{100, 116, 139, 90};
constexpr Color kPointColor{100, 116, 139, 255};
constexpr Color kForbiddenFillColor{15, 23, 42, 34};
constexpr Color kForbiddenMarkColor{15, 23, 42, 185};
constexpr Color kWinningColor{250, 204, 21, 255};
//Color 的四个数字依次是红、绿、蓝和透明度 alpha，范围都是 0 到 255。
//A Color stores red, green, blue and alpha values, each from 0 to 255.
//constexpr 表示这些配色在编译时就确定，运行期间不会被修改。
//constexpr means the palette is fixed at compile time.
//这些名称分别代表窗口背景、棋盘白底、网格、边框、文字、点、禁区和胜利高亮。
//The names cover the background, grid, borders, text, points and highlights.

using BoardGeometry = GuiBoardGeometry;
//using 给 GuiBoardGeometry 起一个较短的本地别名，不会创建新的类型。
//using creates a shorter local alias; it does not create a new type.

struct BoardBoundary {
    Vector2 top_left;
    Vector2 top_right;
    Vector2 bottom_right;
    Vector2 bottom_left;
};
//保存某一圈棋盘边界的四个 Raylib 屏幕坐标。
//Stores four Raylib screen points for one board boundary.
//成员按绕边界一圈的顺序排列，方便后面连接和填充四边形。
//The members follow boundary order, which helps connect or fill the shape.

/** Convert renderer-independent GUI coordinates to a Raylib vector. */
Vector2 to_vector(GuiPoint point) {
    return {point.x, point.y};
}
//GuiPoint 和 Vector2 都有 x、y，这里只是把项目类型转换成 Raylib 类型。
//GuiPoint and Vector2 both have x/y; this converts to Raylib's type.

/** @brief Returns the Euclidean length of a screen-space vector. */
float length(Vector2 value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}
//根据勾股定理 sqrt(x²+y²) 计算二维向量的屏幕像素长度。
//Uses sqrt(x^2+y^2) to calculate a 2D vector's pixel length.

/**
 * @brief Converts a hexadecimal RGB string such as `#ff0015` to a color.
 * @param text Color string to parse.
 * @param fallback Color returned when parsing fails.
 * @return The parsed opaque Raylib color, or @p fallback.
 * @par Primary contributor
 * Hao Guo (rendering configuration).
 */
Color parse_color(const std::string& text, Color fallback) {
    //text 用 const 引用传入，避免复制并保证函数不会修改调用者的字符串。
    //text is passed by const reference to avoid a copy and prevent changes.
    //fallback 是解析失败时返回的备用 Raylib 颜色。
    //fallback is the Raylib color returned when parsing fails.
    std::string digits = text;
    //复制一份是因为下一步要删除 #，但不能修改原来的 text。
    //A copy is needed because '#' is removed without changing text.

    if (!digits.empty() && digits.front() == '#') {
        digits.erase(digits.begin());
    }
    //先确认字符串不为空，再检查第一个字符；&& 具有短路效果。
    //It checks non-empty first; && short-circuits before front() when empty.
    //如果颜色以 # 开头，就删除这个字符，只留下六个十六进制数字。
    //A leading # is removed, leaving six hexadecimal digits.

    if (digits.size() != 6) {
        return fallback;
    }
    //RGB 每个通道使用两位十六进制，所以总长度必须正好是 6。
    //RGB uses two hex digits per channel, so the length must be exactly 6.

    try {
        std::size_t parsed = 0;
        const unsigned long rgb = std::stoul(digits, &parsed, 16);
        //stoul 把 digits 按 16 进制转换成无符号整数，并写入已解析字符数。
        //stoul parses digits in base 16 and writes the consumed character count.
        //&parsed 取得变量地址，让 stoul 能把结果写回 parsed。
        //&parsed passes its address so stoul can write the count back.

        if (parsed != digits.size()) {
            return fallback;
        }
        //即使开头能解析，只要没有完整消费六个字符，也把输入看作无效。
        //The input is invalid unless all six characters were consumed.

        return {
            static_cast<unsigned char>((rgb >> 16U) & 0xffU),
            static_cast<unsigned char>((rgb >> 8U) & 0xffU),
            static_cast<unsigned char>(rgb & 0xffU),
            255
        };
        //第一行右移 16 位并用 0xff 保留最低 8 位，取出红色通道。
        //The first line shifts 16 bits and masks 8 bits to extract red.
        //后两行用同样方法取出绿色和蓝色；255 表示完全不透明。
        //The next lines extract green/blue; alpha 255 means fully opaque.
        //static_cast<unsigned char> 转成 Raylib Color 每个通道要求的字节类型。
        //The cast converts each channel to the byte type required by Color.
    } catch (const std::exception&) {
        return fallback;
    }
    //非法字符或数值转换错误会抛出标准异常；这里统一退回备用颜色。
    //Invalid digits throw a standard exception; this returns the fallback.
}

/**
 * @brief Calculates the fixed grid and rotated board geometry.
 * @param board Board whose dimensions and rotation define the geometry.
 * @return Corner points, grid steps, and center in screen coordinates.
 *
 * The transformation follows the formula specified in SPIELREGELN.md.
 *
 * @par Primary contributor
 * Hao Guo (board rendering).
 */
BoardGeometry make_geometry(const Board& board) {
    return calculate_board_geometry(
        board,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()));
}
//GetScreenWidth/Height 读取窗口当前尺寸，窗口缩放后下一帧会重新获取。
//GetScreenWidth/Height reads the current size again after a window resize.
//static_cast<float> 把 Raylib 返回的 int 像素值转换成几何函数需要的 float。
//static_cast<float> converts Raylib's int pixels to the required float.
//calculate_board_geometry 根据棋盘和窗口大小算出这一帧应该画在哪里。
//calculate_board_geometry decides where the board belongs in this frame.

/**
 * @brief Maps an integer fixed-grid coordinate to a screen-space point.
 * @par Primary contributor
 * Hao Guo (board rendering).
 */
Vector2 board_to_screen(
    const BoardGeometry& geometry,
    float x,
    float y) {

    return {
        geometry.grid_top_left.x + geometry.x_step.x * x +
            geometry.y_step.x * y,
        geometry.grid_top_left.y + geometry.x_step.y * x +
            geometry.y_step.y * y
    };
}
//计算公式是“固定网格左上角 + x 个 x_step + y 个 y_step”。
//Formula: fixed-grid top-left + x*x_step + y*y_step.
//分别计算结果的屏幕 x 和屏幕 y，返回 Raylib 的 Vector2。
//It calculates both screen components and returns a Raylib Vector2.
//这里允许 x、y 是小数，因为边界会使用 -0.5 和 inset 等半格位置。
//Float coordinates allow half-step boundary positions such as -0.5.

Vector2 board_to_screen(
    const BoardGeometry& geometry,
    int x,
    int y) {

    return board_to_screen(
        geometry,
        static_cast<float>(x),
        static_cast<float>(y));
}
//这是函数重载：名字相同，但参数类型从 float 版本换成 int 版本。
//This is an overload with int parameters instead of float parameters.
//它只负责转换类型，再复用上面的核心公式，避免写两份相同代码。
//It converts the types and reuses the formula to avoid duplicate code.

BoardBoundary boundary_for_inset(
    const BoardGeometry& geometry,
    const Board& board,
    float inset) {

    const float fraction =
        static_cast<float>(board.get_rotation_fraction());
    //fraction 是 Board 根据旋转角计算出的 0 到 1 的比例。
    //fraction is Board's 0-to-1 value calculated from the rotation angle.

    const float left = inset - 0.5F;
    const float top = inset - 0.5F;
    //坐标点位于格线交点，边界需要向外多半格，所以从 -0.5 开始。
    //Grid points sit on intersections, so the outer edge starts at -0.5.
    //inset 为 1 时四边会再向内收一格，用来得到内圈和目标线。
    //An inset of 1 moves all edges one grid step inward.

    const float right =
        static_cast<float>(board.get_width()) - 0.5F - inset;
    const float bottom =
        static_cast<float>(board.get_height()) - 0.5F - inset;
    //右边和下边从棋盘尺寸减半格，再减去 inset。
    //Right and bottom use board size minus half a cell and the inset.

    const float rect_width = std::max(0.0F, right - left);
    const float rect_height = std::max(0.0F, bottom - top);
    //std::max 防止极端 inset 让宽高成为负数。
    //std::max prevents an extreme inset from producing negative dimensions.

    return BoardBoundary{
        board_to_screen(
            geometry,
            left + fraction * rect_width,
            top),
        //左上角沿上边移动 fraction × 宽度，再转换到屏幕坐标。
        //Top-left moves by fraction*width along the top edge.
        board_to_screen(
            geometry,
            right,
            top + fraction * rect_height),
        //右上角：负责旋转边界的右上方向。
        //Top-right: the upper corner on the right side.
        board_to_screen(
            geometry,
            right - fraction * rect_width,
            bottom),
        //右下角：负责旋转边界的右下方向。
        //Bottom-right: the lower corner on the right side.
        board_to_screen(
            geometry,
            left,
            bottom - fraction * rect_height)
        //左下角：负责旋转边界的左下方向。
        //Bottom-left: the lower corner on the left side.
    };
}
//四个返回项依次填入 BoardBoundary 的左上、右上、右下和左下。
//The four results initialize top-left, top-right, bottom-right and bottom-left.

void draw_quad(
    Vector2 first,
    Vector2 second,
    Vector2 third,
    Vector2 fourth,
    Color color) {

    DrawTriangle(first, second, third, color);
    DrawTriangle(first, third, fourth, color);
}
//Raylib 直接提供三角形填充，因此用第一、二、三点画第一个三角形。
//Raylib fills triangles, so points 1-2-3 form the first triangle.
//再用第一、三、四点补上另一个三角形，两个三角形合成四边形。
//Points 1-3-4 form the second triangle; together they make a quad.
//两个三角形共用 first 到 third 的对角线，不会在中间留下空隙。
//They share the first-to-third diagonal, so there is no gap.

Vector2 line_intersection(
    Vector2 first_start,
    Vector2 first_end,
    Vector2 second_start,
    Vector2 second_end) {

    const float first_dx = first_end.x - first_start.x;
    const float first_dy = first_end.y - first_start.y;
    //终点减起点，得到第一条直线在 x 和 y 方向上的向量分量。
    //End minus start gives the first line's x/y direction components.

    const float second_dx = second_end.x - second_start.x;
    const float second_dy = second_end.y - second_start.y;
    //这两行用同样的方法得到第二条直线的方向向量。
    //These lines do the same for the second line.

    const float denominator =
        first_dx * second_dy - first_dy * second_dx;
    //denominator 是两个方向向量的二维叉积，也是求直线交点公式的分母。
    //denominator is the 2D cross product and the intersection denominator.

    if (std::abs(denominator) < 0.0001F) {
        return first_start;
    }
    //分母非常接近 0 时，两条线平行或几乎平行，不能稳定地相除。
    //A near-zero denominator means parallel lines and unsafe division.
    //这里返回 first_start 作为安全退路，避免除以零产生无效坐标。
    //first_start is a safe fallback that avoids invalid coordinates.

    const float t =
        ((second_start.x - first_start.x) * second_dy -
         (second_start.y - first_start.y) * second_dx) /
        denominator;
    //t 表示交点位于第一条线“起点 + t × 方向向量”的哪个位置。
    //t locates the intersection on start + t*direction of the first line.
    return {
        first_start.x + t * first_dx,
        first_start.y + t * first_dy
    };
    //把 t 代回第一条直线的参数方程，得到交点的屏幕 x、y。
    //Substituting t into the first line gives the intersection x/y.
}

void mask_corner_overlaps(
    const BoardBoundary& outer,
    const BoardBoundary& inner) {

    draw_quad(
        outer.top_left,
        line_intersection(
            outer.top_left,
            outer.top_right,
            inner.top_left,
            inner.bottom_left),
        inner.top_left,
        line_intersection(
            outer.top_left,
            outer.bottom_left,
            inner.top_left,
            inner.top_right),
        kBoardBackground);
    //第一块处理左上角：用外圈左上角、两条边的交点和内圈左上角
    //组成四边形，再用棋盘白色覆盖目标区域在角落产生的重叠颜色。
    //The first quad masks the top-left overlap with the board background.
    //两个 line_intersection 分别求外圈上边/左边与内圈相邻边的交点。
    //Two intersections find the matching points on the top and left edges.

    draw_quad(
        outer.top_right,
        line_intersection(
            outer.top_right,
            outer.bottom_right,
            inner.top_left,
            inner.top_right),
        inner.top_right,
        line_intersection(
            outer.top_left,
            outer.top_right,
            inner.top_right,
            inner.bottom_right),
        kBoardBackground);
    //第二块执行相同遮罩操作，但位置是右上角。
    //The same mask is applied to the top-right corner.

    draw_quad(
        outer.bottom_right,
        line_intersection(
            outer.bottom_right,
            outer.bottom_left,
            inner.top_right,
            inner.bottom_right),
        inner.bottom_right,
        line_intersection(
            outer.top_right,
            outer.bottom_right,
            inner.bottom_left,
            inner.bottom_right),
        kBoardBackground);
    //第三块负责右下角。
    //The third mask covers the bottom-right corner.

    draw_quad(
        outer.bottom_left,
        line_intersection(
            outer.bottom_left,
            outer.top_left,
            inner.bottom_left,
            inner.bottom_right),
        inner.bottom_left,
        line_intersection(
            outer.bottom_right,
            outer.bottom_left,
            inner.top_left,
            inner.bottom_left),
        kBoardBackground);
    //第四块负责左下角；四个角全部处理后，不会出现两名玩家颜色叠加。
    //The fourth covers bottom-left, completing all four overlap masks.
}

/**
 * @brief Finds the grid point nearest to the mouse cursor.
 * @return The clicked board position, or std::nullopt when no point is close.
 *
 * This checks visible fixed-grid points that are still inside the rotated
 * board area.
 *
 * @par Primary contributor
 * Junke Pu (mouse interaction and move placement).
 */
std::optional<Position> find_clicked_position(
    const BoardGeometry& geometry,
    const Board& board) {

    const Vector2 mouse = GetMousePosition();
    return find_nearest_board_position(
        geometry,
        board,
        GuiPoint{mouse.x, mouse.y});
}
//Junke Pu：读取鼠标屏幕坐标，再交给可测试的逻辑函数寻找最近棋盘点。

/**
 * @brief Checks whether a coordinate is an unplayable overlapping corner area.
 * @par Primary contributor
 * Hao Guo (board rendering).
 */
bool is_corner(const Board& board, int x, int y) {
    const Position position{x, y};
    //把两个整数坐标组合成项目统一使用的 Position。
    //Combines the two integer coordinates into the project's Position type.
    return board.is_in_bounds(position) &&
           !board.is_playable(position, 0) &&
           !board.is_playable(position, 1);
    //首先必须仍位于旋转棋盘边界内，然后对两名玩家都不可下棋。
    //It must be inside the rotated board and unplayable by both players.
    //&& 从左向右短路；越界时不会继续调用后面两个 is_playable。
    //&& short-circuits, so out-of-bounds points skip the later checks.
}
//整体用于识别四角重叠禁区中的坐标点。
//Overall, this identifies grid points in the forbidden corner overlaps.

/** @brief Draws text centered around the supplied screen position. */
void draw_centered_text(
    const std::string& text,
    Vector2 center,
    int font_size,
    Color color) {

    const int width = MeasureText(text.c_str(), font_size);
    //c_str() 把 std::string 转成 Raylib 接受的 C 风格字符串指针。
    //c_str() converts std::string to the C string pointer Raylib accepts.
    //MeasureText 用同一字号测量文字宽度，返回像素数。
    //MeasureText returns the string width in pixels for this font size.

    DrawText(
        text.c_str(),
        static_cast<int>(center.x) - width / 2,
        static_cast<int>(center.y) - font_size / 2,
        font_size,
        color);
    //DrawText 的位置参数是文字左上角，而调用者给的是中心点。
    //DrawText expects top-left, while the caller supplies the center.
    //x 减去文字宽度一半实现水平居中；y 减去字号一半近似垂直居中。
    //Subtracting half width/font size centers the text around the point.
    //最后两个参数分别是字号和颜色。
    //The last two arguments are font size and color.
}

void draw_forbidden_marker(Vector2 center, float size, float width) {
    //在 center 周围画两条对角线，组合成表示禁区的“X”。
    //Draws two diagonals around center to form a forbidden X marker.
    DrawLineEx(
        {center.x - size, center.y - size},
        {center.x + size, center.y + size},
        width,
        kForbiddenMarkColor);
    //第一条线从左上画到右下：两个端点都离 center 距离 size。
    //The first line runs top-left to bottom-right, size away from center.
    //DrawLineEx 四个参数依次是起点、终点、线宽和颜色。
    //DrawLineEx takes start, end, thickness and color.
    DrawLineEx(
        {center.x - size, center.y + size},
        {center.x + size, center.y - size},
        width,
        kForbiddenMarkColor);
    //第二条线负责从左下画到右上，与第一条线交叉。
    //The second line goes bottom-left to top-right.
}

Vector2 quad_center(
    Vector2 first,
    Vector2 second,
    Vector2 third,
    Vector2 fourth) {

    return {
        (first.x + second.x + third.x + fourth.x) * 0.25F,
        (first.y + second.y + third.y + fourth.y) * 0.25F
    };
    //四个顶点的 x 分量相加乘 1/4，y 分量也同样计算。
    //Adds four x values and four y values, then multiplies each by 1/4.
    //对于这里的角落四边形，四点平均值可以作为叉号的中心。
    //For these corner quads, the average is a suitable marker center.
}

void draw_forbidden_corner_markers(
    const BoardBoundary& outer,
    const BoardBoundary& inner,
    float marker_size,
    float marker_width) {

    draw_forbidden_marker(
        quad_center(
            outer.top_left,
            line_intersection(
                outer.top_left,
                outer.top_right,
                inner.top_left,
                inner.bottom_left),
            inner.top_left,
            line_intersection(
                outer.top_left,
                outer.bottom_left,
                inner.top_left,
                inner.top_right)),
        marker_size,
        marker_width);
    //第一块负责左上角：先用和遮罩相同的四个顶点求中心，
    //再把中心、叉号半尺寸和线宽传给 draw_forbidden_marker。
    //Top-left: averages the same four mask points, then draws one X.

    draw_forbidden_marker(
        quad_center(
            outer.top_right,
            line_intersection(
                outer.top_right,
                outer.bottom_right,
                inner.top_left,
                inner.top_right),
            inner.top_right,
            line_intersection(
                outer.top_left,
                outer.top_right,
                inner.top_right,
                inner.bottom_right)),
        marker_size,
        marker_width);
    //第二块用相同方法处理右上角。
    //The second block handles the top-right corner.

    draw_forbidden_marker(
        quad_center(
            outer.bottom_right,
            line_intersection(
                outer.bottom_right,
                outer.bottom_left,
                inner.top_right,
                inner.bottom_right),
            inner.bottom_right,
            line_intersection(
                outer.top_right,
                outer.bottom_right,
                inner.bottom_left,
                inner.bottom_right)),
        marker_size,
        marker_width);
    //第三块负责右下角。
    //The third block handles the bottom-right corner.

    draw_forbidden_marker(
        quad_center(
            outer.bottom_left,
            line_intersection(
                outer.bottom_left,
                outer.top_left,
                inner.bottom_left,
                inner.bottom_right),
            inner.bottom_left,
            line_intersection(
                outer.bottom_right,
                outer.bottom_left,
                inner.top_left,
                inner.bottom_left)),
        marker_size,
        marker_width);
    //第四块负责左下角。
    //The fourth block handles the bottom-left corner.
}

Vector2 outward_position(
    Vector2 point,
    Vector2 center,
    float distance) {

    Vector2 direction{
        point.x - center.x,
        point.y - center.y};
    //用“标签锚点 - 棋盘中心”得到一个从中心朝外的方向向量。
    //point minus center gives a direction vector pointing outward.
    const float direction_length = length(direction);
    //先求原方向向量的长度，后面才能把它缩放到指定距离。
    //Its original length is needed to scale it to the requested distance.

    if (direction_length > 0.001F) {
        const float factor = distance / direction_length;
        direction.x *= factor;
        direction.y *= factor;
    }
    //长度足够大时，用 distance/原长度 得到缩放倍数。
    //For a nonzero vector, distance/original-length is the scale factor.
    //缩放后 direction 的长度就等于 distance。
    //After scaling, direction has the requested length.
    //0.001F 的判断避免锚点正好在中心时除以零。
    //The 0.001 check prevents division by zero at the center.

    return {point.x + direction.x, point.y + direction.y};
    //把向外向量加到原锚点，使坐标文字离开棋盘边线一些。
    //Adds the outward vector so the label does not touch the board edge.
}

/**
 * @brief Draws the current player, round, result, and interaction feedback.
 * @par Primary contributor
 * Junke Pu (game-state display).
 */
void draw_status(
    const Board& board,
    const std::vector<std::string>& names,
    const Color colors[kNumPlayers],
    const std::string& feedback) {
    //Junke Pu：绘制当前玩家、回合、胜负结果和最近点击提示。

    int displayed_player = board.get_current_player();
    if (board.get_phase() == GamePhase::kFinished) {
        displayed_player = board.check_win(0) ? 0 : 1;
    }
    const std::string status = game_status_text(board, names);

    DrawRectangleRounded(
        Rectangle{
            14.0F,
            12.0F,
            static_cast<float>(GetScreenWidth()) - 28.0F,
            82.0F},
        0.18F,
        8,
        Color{255, 255, 255, 255});

    DrawCircle(
        38,
        38,
        11.0F,
        colors[displayed_player]);

    DrawText(
        status.c_str(),
        58,
        23,
        21,
        kTextColor);

    const std::string round =
        "Round " + std::to_string(board.get_turn() + 1);

    DrawText(
        round.c_str(),
        58,
        53,
        14,
        kSecondaryText);

    if (!feedback.empty()) {
        const int text_width =
            MeasureText(feedback.c_str(), 14);

        DrawText(
            feedback.c_str(),
            GetScreenWidth() - text_width - 28,
            54,
            14,
            kSecondaryText);
    }
}

}  // namespace

GuiRenderer::GuiRenderer(
    const Board& board,
    std::vector<std::string> player_colors,
    std::vector<std::string> player_names,
    std::string title)
    : board_(board),
      player_colors_(std::move(player_colors)),
      player_names_(std::move(player_names)) {
    //这是构造函数；冒号后面是成员初始化列表，在进入函数体前初始化成员。
    //This is the constructor; the list after ':' initializes members first.
    //board_ 引用外部传入的 board，不创建另一份棋盘副本。
    //board_ refers to the supplied board instead of copying it.
    //std::move 把两个 vector 内部资源转移给成员，避免再复制全部字符串。
    //std::move transfers both vectors' resources instead of copying strings.
    //player_names_ 是 Junke Pu 状态栏需要的数据，属于这里的交叉部分。

    // Hao Guo: normalize the colors consumed by the visual renderer.
    if (player_colors_.size() < kNumPlayers) {
        player_colors_ = kDefaultPlayerColors;
    }
    //游戏固定有 kNumPlayers（2）名玩家，颜色少于两个时使用项目默认红蓝色。
    //The game has two players; too few colors are replaced by red/blue defaults.
    //这样 draw_frame 后面访问 player_colors_[0] 和 [1] 时不会数组越界。
    //This keeps later accesses to [0] and [1] in bounds.

    // Junke Pu: normalize names consumed by the game-state panel.
    if (player_names_.size() < kNumPlayers) {
        player_names_ = kDefaultPlayerNames;
    }
    //Junke Pu：名字不足两项时改用默认玩家名，保证状态栏访问安全。

    // Hao Guo: configure and initialize the GUI window.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    //SetConfigFlags 必须在 InitWindow 之前调用，用来设置窗口创建选项。
    //SetConfigFlags must run before InitWindow to configure window creation.
    //FLAG_WINDOW_RESIZABLE 允许用户调整窗口大小。
    //FLAG_WINDOW_RESIZABLE lets the user resize the window.
    //“|”按位组合两个标志；FLAG_VSYNC_HINT 请求按显示器刷新率垂直同步。
    //Bitwise | combines the flags; VSYNC requests display-synced updates.

    InitWindow(
        kDefaultWindowSize,
        kDefaultWindowSize,
        title.c_str());
    //InitWindow 的参数依次是初始宽度、初始高度和窗口标题。
    //InitWindow takes initial width, height and the window title.
    //kDefaultWindowSize 在 common.h 中是 720，所以默认创建 720×720 窗口。
    //kDefaultWindowSize is 720, so the initial window is 720x720.
    //title.c_str() 把 std::string 转换成 Raylib 需要的 const char*。
    //title.c_str() converts std::string to Raylib's const char*.

    if (!IsWindowReady()) {
        throw std::runtime_error(
            "Raylib-Fenster konnte nicht initialisiert werden");
    }
    //IsWindowReady 检查窗口及图形上下文是否成功初始化；! 表示取反。
    //IsWindowReady checks initialization; ! enters the error case.
    //失败时抛出运行时异常，让 main 的 catch 记录错误并结束程序。
    //Failure throws so main can log the error and stop the program.

    SetWindowMinSize(480, 480);
    //允许缩放不代表无限缩小；这里把窗口最小宽高都限制为 480 像素。
    //Resizing is allowed, but the minimum width and height are 480 pixels.

    SetTargetFPS(60);
    //把主循环目标上限设为每秒 60 帧，让动画流畅且避免无意义地占满 CPU。
    //Targets at most 60 frames per second for smooth, bounded rendering.

    window_open_ = true;
    //只有以上步骤全部成功后才记录窗口已打开；析构函数会依据它决定是否关闭。
    //The ownership flag becomes true only after successful initialization.
}

GuiRenderer::~GuiRenderer() {
    // Hao Guo: release the Raylib window owned by this renderer.
    if (window_open_ && IsWindowReady()) {
        CloseWindow();
    }
    //析构函数会在 GuiRenderer 生命周期结束时自动调用。
    //The destructor runs automatically when GuiRenderer's lifetime ends.
    //同时满足“本对象记录为已打开”和“Raylib 窗口仍有效”才调用 CloseWindow。
    //CloseWindow runs only when both ownership and Raylib readiness are true.
    //&& 的短路也保证 window_open_ 为 false 时不会多余查询 Raylib 状态。
    //&& short-circuits the readiness query when no window is owned.
}

bool GuiRenderer::should_close() const {
    // Hao Guo: expose the window-lifetime state to the main game loop.
    return !window_open_ || WindowShouldClose();
    //如果窗口根本没有打开，或者用户按 Esc/点击关闭按钮，就返回 true。
    //Returns true when unopened, or when Esc/the close button requests closing.
    //“||”会短路，因此窗口未打开时不会继续调用 WindowShouldClose。
    //|| short-circuits WindowShouldClose when the window is not open.
    //末尾 const 表示这个查询不会修改 GuiRenderer 的成员状态。
    //The trailing const means this query does not modify member state.
}

std::optional<preset::Move>
GuiRenderer::request_move_from_current_human_player() {
    // Junke Pu: implement the school's PlayerGuiAccess hand-off. Copying the
    // optional and then clearing it ensures that each click is consumed once.
    auto result = pending_move_;
    pending_move_.reset();
    return result;
}
//Junke Pu：复制待处理落子后立刻清空，保证一次点击最多只被读取一次。

void GuiRenderer::draw_frame() {
    if (should_close()) {
        return;
    }
    //窗口已经关闭或收到关闭请求时立即返回，后面不再调用任何绘图函数。
    //Returns immediately after a close request, so no drawing API is called.

    // Hao Guo: derive all geometry and drawing sizes for this frame.
    const BoardGeometry geometry = make_geometry(board_);
    //窗口可以缩放，所以每一帧都根据当前窗口和棋盘重新计算完整几何数据。
    //The window is resizable, so geometry is recalculated for every frame.

    // Junke Pu: resolve the exact coordinate currently under the mouse. This
    // keeps every coordinate discoverable even on the largest supported board.
    const auto hovered_position =
        find_clicked_position(geometry, board_);
    //Junke Pu：找出鼠标附近的棋盘坐标，供点击落子和提示使用。

    const Color colors[kNumPlayers] = {
        parse_color(player_colors_[0], RED),
        parse_color(player_colors_[1], BLUE)
    };
    //把两名玩家的十六进制字符串转换成 Raylib Color，并放进固定数组。
    //Converts both players' hex strings into a fixed Raylib Color array.
    //如果某项解析失败，玩家 1 使用 RED，玩家 2 使用 BLUE 作为备用颜色。
    //Invalid values fall back to RED for player 1 and BLUE for player 2.

    const float step = std::min(
        length(to_vector(geometry.x_step)),
        length(to_vector(geometry.y_step)));
    //把两个网格步长转成 Vector2 并计算像素长度，再取较小值作为绘制尺度。
    //Measures both grid steps and uses the smaller one as the drawing scale.
    //因此窗口和棋盘尺寸变化时，后面的点、棋子和桥会一起缩放。
    //Points, pegs and bridges therefore scale with board/window size.

    const float peg_radius =
        std::clamp(step * 0.29F, 2.2F, 9.5F);
    //棋子半径通常取格距的 29%，但限制在 2.2 到 9.5 像素之间。
    //Peg radius is 29% of the step, clamped to 2.2-9.5 pixels.

    const float bridge_width =
        std::clamp(step * 0.18F, 1.5F, 5.0F);
    //桥宽通常取格距的 18%，同时限制最细 1.5、最粗 5 像素。
    //Bridge width is 18% of the step, clamped to 1.5-5 pixels.

    // Junke Pu: discard interaction state left over from the previous turn.
    if (input_turn_ != board_.get_turn()) {
        pending_move_.reset();
        input_turn_ = board_.get_turn();
    }
    //Junke Pu：回合改变时清除上一回合遗留的点击。

    // Junke Pu: translate a click into a valid game move and queue it for
    // request_move_from_current_human_player(). This is the piece-placement
    // interaction; Board applies the move later through HumanPlayer.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        board_.get_phase() != GamePhase::kFinished &&
        board_.get_phase() != GamePhase::kDraw) {

        const auto& position = hovered_position;

        if (!position.has_value()) {
            feedback_ = "Bitte einen Rasterpunkt anklicken.";
        } else {
            const auto move =
                move_for_board_position(board_, *position);

            if (move.has_value()) {
                pending_move_ = *move;

                feedback_ =
                    "Selected: (" +
                    std::to_string(position->x) + ", " +
                    std::to_string(position->y) + ")";
            } else {
                feedback_ = "This field is not playable.";
            }
        }
    }
    //Junke Pu：只在游戏未结束时把一次鼠标左键点击转换成合法落子和反馈。

    // Hao Guo: begin the visual rendering pass.
    BeginDrawing();
    //通知 Raylib 开始记录这一帧的绘图命令；它必须和 EndDrawing 成对出现。
    //Tells Raylib to begin this frame; it must be paired with EndDrawing.
    ClearBackground(kBackground);
    //清除上一帧的内容，并用 kBackground 填满整个窗口背景。
    //Clears the previous frame and fills the window with kBackground.

    // Junke Pu: display the current turn, game result, and click feedback.
    draw_status(
        board_,
        player_names_,
        colors,
        feedback_);
    //Junke Pu：先绘制顶部状态栏；后面的棋盘区域从状态栏下方开始。

    // Hao Guo: render the board background and the visible goal areas.
    const BoardBoundary outer_boundary = boundary_for_inset(
        geometry,
        board_,
        0.0F);
    //inset=0 得到最外圈边界，它包围完整棋盘和外侧半格空间。
    //inset=0 creates the outer boundary around the full board.

    const BoardBoundary inner_boundary = boundary_for_inset(
        geometry,
        board_,
        1.0F);
    //inset=1 得到向内缩一格的边界，用来分隔中间区域和四周目标区域。
    //inset=1 creates the inner boundary one grid step inward.

    const BoardBoundary target_line_boundary = boundary_for_inset(
        geometry,
        board_,
        1.0F);
    //目标线目前与 inner_boundary 使用相同几何；单独命名表达绘制用途。
    //The goal lines currently share the inset-1 geometry but have a clear name.

    draw_quad(
        outer_boundary.top_left,
        outer_boundary.top_right,
        outer_boundary.bottom_right,
        outer_boundary.bottom_left,
        kBoardBackground);
    //用外圈四个角组成整个棋盘底板，颜色是白色 kBoardBackground。
    //Fills the whole outer board quad with the white board background.
    //draw_quad 内部会把四边形拆成两个填充三角形。
    //draw_quad internally fills it as two triangles.

    draw_quad(
        outer_boundary.top_left,
        outer_boundary.top_right,
        inner_boundary.top_right,
        inner_boundary.top_left,
        Color{colors[0].r, colors[0].g, colors[0].b, 46});
    //第一块是玩家 0 的上方目标带：外圈上边与内圈上边之间的四边形。
    //The first band is player 0's top goal area between outer and inner edges.
    //颜色沿用玩家 0 的 RGB，但 alpha=46，让白底透出来形成淡色。
    //It uses player 0's RGB with alpha 46 for a light transparent fill.

    draw_quad(
        inner_boundary.bottom_left,
        inner_boundary.bottom_right,
        outer_boundary.bottom_right,
        outer_boundary.bottom_left,
        Color{colors[0].r, colors[0].g, colors[0].b, 46});
    //第二块负责玩家 0 的下方目标带。
    //The second band is player 0's bottom goal area.

    draw_quad(
        outer_boundary.top_left,
        inner_boundary.top_left,
        inner_boundary.bottom_left,
        outer_boundary.bottom_left,
        Color{colors[1].r, colors[1].g, colors[1].b, 46});
    //第三块负责玩家 1 的左侧目标带。
    //The third band is player 1's left goal area.

    draw_quad(
        inner_boundary.top_right,
        outer_boundary.top_right,
        outer_boundary.bottom_right,
        inner_boundary.bottom_right,
        Color{colors[1].r, colors[1].g, colors[1].b, 46});
    //第四块负责玩家 1 的右侧目标带。
    //The fourth band is player 1's right goal area.

    mask_corner_overlaps(outer_boundary, inner_boundary);
    //四条半透明目标带会在角落相遇；这里先用白色消除重叠颜色。
    //The goal bands meet at corners; white masks remove the color overlap.

    draw_quad(
        outer_boundary.top_left,
        line_intersection(
            outer_boundary.top_left,
            outer_boundary.top_right,
            inner_boundary.top_left,
            inner_boundary.bottom_left),
        inner_boundary.top_left,
        line_intersection(
            outer_boundary.top_left,
            outer_boundary.bottom_left,
            inner_boundary.top_left,
            inner_boundary.top_right),
        kForbiddenFillColor);
    //第一块画左上禁区：四个点来自外角、内角和两组边线交点。
    //The first forbidden quad covers top-left using outer/inner intersections.
    //kForbiddenFillColor 的 alpha 较低，只给白色角落加一层深色阴影。
    //Its low alpha adds a subtle dark shade over the white corner.

    draw_quad(
        outer_boundary.top_right,
        line_intersection(
            outer_boundary.top_right,
            outer_boundary.bottom_right,
            inner_boundary.top_left,
            inner_boundary.top_right),
        inner_boundary.top_right,
        line_intersection(
            outer_boundary.top_left,
            outer_boundary.top_right,
            inner_boundary.top_right,
            inner_boundary.bottom_right),
        kForbiddenFillColor);
    //第二块用相同方法画右上禁区。
    //The second forbidden quad covers top-right.

    draw_quad(
        outer_boundary.bottom_right,
        line_intersection(
            outer_boundary.bottom_right,
            outer_boundary.bottom_left,
            inner_boundary.top_right,
            inner_boundary.bottom_right),
        inner_boundary.bottom_right,
        line_intersection(
            outer_boundary.top_right,
            outer_boundary.bottom_right,
            inner_boundary.bottom_left,
            inner_boundary.bottom_right),
        kForbiddenFillColor);
    //第三块画右下禁区。
    //The third forbidden quad covers bottom-right.

    draw_quad(
        outer_boundary.bottom_left,
        line_intersection(
            outer_boundary.bottom_left,
            outer_boundary.top_left,
            inner_boundary.bottom_left,
            inner_boundary.bottom_right),
        inner_boundary.bottom_left,
        line_intersection(
            outer_boundary.bottom_right,
            outer_boundary.bottom_left,
            inner_boundary.top_left,
            inner_boundary.bottom_left),
        kForbiddenFillColor);
    //第四块画左下禁区。
    //The fourth forbidden quad covers bottom-left.

    draw_quad(
        inner_boundary.top_left,
        inner_boundary.top_right,
        inner_boundary.bottom_right,
        inner_boundary.bottom_left,
        kBoardBackground);
    //最后用内圈四边形把中间可玩主区域重新盖成纯白色。
    //Finally, the inner quad restores the playable center to solid white.

    // Hao Guo: render the fixed coordinate grid.画网格线
    for (int x = 0; x < board_.get_width(); ++x) {
        DrawLineEx(
            board_to_screen(geometry, x, 0),
            board_to_screen(
                geometry,
                x,
                board_.get_height() - 1),
            1.0F,
            kGridColor);
        //固定 x，起点 y=0，终点 y=height-1，所以这一条贯穿棋盘纵向。
        //Fixed x with y from 0 to height-1 draws one vertical grid line.
        //board_to_screen 把两个棋盘端点转换成 Raylib 屏幕坐标。
        //board_to_screen converts both board endpoints to screen points.
        //DrawLineEx 的后两个参数是 1.0 像素线宽和网格颜色。
        //The last arguments are 1.0-pixel thickness and grid color.
    }
    //for 让 x 从 0 走到 width-1，因此会画出所有竖向网格线。
    //The loop covers x=0 through width-1, drawing every vertical line.

    for (int y = 0; y < board_.get_height(); ++y) {
        DrawLineEx(
            board_to_screen(geometry, 0, y),
            board_to_screen(
                geometry,
                board_.get_width() - 1,
                y),
            1.0F,
            kGridColor);
    }
    //第二个循环固定 y，从 x=0 连接到 x=width-1，画出所有横向网格线。
    //The second loop fixes y and draws every horizontal grid line.

    // Hao Guo: render the neutral outer boundary and the inset winning lines.
    DrawLineEx(
        outer_boundary.top_left,
        outer_boundary.top_right,
        2.0F,
        kOuterBoundaryColor);
    //第一条外边框从左上角连接到右上角，线宽 2 像素，使用中性边框色。
    //The first outer edge connects top-left to top-right at 2-pixel width.
    //这里详细说明第一条即可；下面三条只改变起点和终点来围成一圈。
    //The next three only change endpoints to complete the boundary.

    DrawLineEx(
        outer_boundary.top_right,
        outer_boundary.bottom_right,
        2.0F,
        kOuterBoundaryColor);
    //第二条负责右侧外边框。
    //The second line is the right outer edge.

    DrawLineEx(
        outer_boundary.bottom_right,
        outer_boundary.bottom_left,
        2.0F,
        kOuterBoundaryColor);
    //第三条负责下侧外边框。
    //The third line is the bottom outer edge.

    DrawLineEx(
        outer_boundary.bottom_left,
        outer_boundary.top_left,
        2.0F,
        kOuterBoundaryColor);
    //第四条负责左侧外边框，并连接回起点。
    //The fourth line is the left outer edge and closes the boundary.

    DrawLineEx(
        target_line_boundary.top_left,
        target_line_boundary.top_right,
        6.0F,
        colors[0]);
    //第一条目标线沿内圈上边绘制，宽 6 像素，颜色属于玩家 0。
    //The first goal line follows the inner top edge in player 0's color.
    //玩家 0 的胜利方向是上到下，所以需要上、下两条目标线。
    //Player 0 connects top to bottom, so it owns the top and bottom goals.

    DrawLineEx(
        target_line_boundary.bottom_left,
        target_line_boundary.bottom_right,
        6.0F,
        colors[0]);
    //第二条是玩家 0 的下方目标线。
    //The second line is player 0's bottom goal.

    DrawLineEx(
        target_line_boundary.top_left,
        target_line_boundary.bottom_left,
        6.0F,
        colors[1]);
    //第三条是玩家 1 的左侧目标线。
    //The third line is player 1's left goal.

    DrawLineEx(
        target_line_boundary.top_right,
        target_line_boundary.bottom_right,
        6.0F,
        colors[1]);
    //第四条是玩家 1 的右侧目标线。
    //The fourth line is player 1's right goal.

    // Hao Guo: render playable coordinates and keep corner overlap points faint.
    const float point_radius =
        std::clamp(step * 0.065F, 0.9F, 2.2F);
    //正常网格点半径取格距的 6.5%，并限制在 0.9 到 2.2 像素。
    //Normal point radius is 6.5% of step, clamped to 0.9-2.2 pixels.

    const float inactive_point_radius =
        std::clamp(step * 0.045F, 0.7F, 1.8F);
    //禁区点使用更小的 4.5%，而且颜色也更淡，用来降低视觉强调。
    //Forbidden points use a smaller 4.5% radius and a fainter color.

    for (int y = 0; y < board_.get_height(); ++y) {
        for (int x = 0; x < board_.get_width(); ++x) {
            const Position position{x, y};
            //双层循环依次访问每一行中的每一列，并组合成 Position{x, y}。
            //The nested loops visit every row/column and form Position{x,y}.

            if (!board_.is_in_bounds(position)) {
                continue;
            }
            //固定矩形网格中有些点位于旋转后棋盘外；continue 直接跳过这些点。
            //continue skips fixed-grid points outside the rotated board.

            const Vector2 point =
                board_to_screen(geometry, x, y);
            //把当前合法棋盘坐标转换成 DrawCircleV 需要的屏幕 Vector2。
            //Converts the valid board coordinate to a screen Vector2.

            if (is_corner(board_, x, y)) {
                DrawCircleV(
                    point,
                    inactive_point_radius,
                    kInactivePointColor);
                //角落禁区点画成半径更小、透明度更高的灰色圆点。
                //Forbidden corner points are smaller, fainter gray circles.
            } else {
                DrawCircleV(
                    point,
                    point_radius,
                    kPointColor);
                //其余棋盘内坐标使用正常半径和较清晰的点颜色。
                //Other in-board coordinates use the normal size and color.
            }
        }
    }
    //DrawCircleV 的参数依次是圆心 Vector2、半径和填充颜色。
    //DrawCircleV takes a Vector2 center, radius and fill color.

    draw_forbidden_corner_markers(
        outer_boundary,
        inner_boundary,
        std::clamp(step * 0.22F, 3.5F, 8.0F),
        std::clamp(step * 0.055F, 1.0F, 1.6F));
    //在四个角落禁区中心画叉号，明确告诉用户这些区域不能下棋。
    //Draws X markers in all four forbidden corners.
    //叉号半尺寸取格距 22%，限制为 3.5~8；线宽取 5.5%，限制为 1~1.6。
    //Marker size/width scale with step but stay inside readable limits.

    // Hao Guo: render normal bridges below the pegs.
    for (const Bridge& bridge : board_.get_bridges()) {
        //范围 for 循环遍历主棋盘当前保存的每一座桥；bridge 是只读引用，不复制。
        //The range-for visits every bridge by const reference without copying.
        const int player =
            std::clamp(bridge.player_id, 0, kNumPlayers - 1);
        //把 player_id 限制到 0 和 1，保证访问 colors[player] 时不会越界。
        //Clamps player_id to 0 or 1 so colors[player] stays in bounds.

        DrawLineEx(
            board_to_screen(
                geometry,
                bridge.from.x,
                bridge.from.y),
            board_to_screen(
                geometry,
                bridge.to.x,
                bridge.to.y),
            bridge_width,
            colors[player]);
        //前两个参数把桥的 from 和 to 棋盘坐标分别转换成屏幕端点。
        //The first arguments convert bridge.from and bridge.to to screen points.
        //再用之前计算的桥宽和该桥所属玩家的颜色连接两个端点。
        //Then the endpoints are joined using bridge width and owner color.
    }
    //桥先画，后面再画棋子，所以棋子会自然覆盖桥的端部。
    //Bridges are drawn first so pegs can naturally cover their endpoints.

    // Junke Pu: visualize the final game state by highlighting the winner's
    // connected bridge path.
    if (board_.get_phase() == GamePhase::kFinished) {
        const int winner = board_.check_win(0) ? 0 : 1;
        const auto path = calculate_winning_path(board_, winner);

        for (std::size_t i = 1; i < path.size(); ++i) {
            DrawLineEx(
                board_to_screen(
                    geometry,
                    path[i - 1].x,
                    path[i - 1].y),
                board_to_screen(
                    geometry,
                    path[i].x,
                    path[i].y),
                bridge_width + 4.0F,
                kWinningColor);
        }
    }
    //Junke Pu：游戏获胜后，用黄色粗线覆盖一条已连接的胜利桥路径。

    // Hao Guo: render the players' pegs above the bridges.
    for (const Peg& peg : board_.get_pegs()) {
        //遍历棋盘上所有已经放下的棋子，const 引用避免复制 Peg。
        //Visits every placed peg by const reference to avoid copies.
        const int player =
            std::clamp(peg.player_id, 0, kNumPlayers - 1);
        //同样把玩家编号限制在合法范围，安全选择棋子颜色。
        //Again clamps the player index before selecting a color.

        const Vector2 point =
            board_to_screen(
                geometry,
                peg.pos.x,
                peg.pos.y);
        //把 Peg 保存的棋盘位置转换成圆心屏幕坐标。
        //Converts the peg's board position into its screen center.

        DrawCircleV(
            point,
            peg_radius + 1.5F,
            kBoardBackground);
        //先画半径大 1.5 像素的白圆，作为棋子的白色底托和描边。
        //First draws a white circle 1.5 pixels larger as a border/backing.

        DrawCircleV(
            point,
            peg_radius,
            colors[player]);
        //再在同一圆心画较小的玩家色圆，形成真正棋子。
        //Then draws the smaller player-colored peg at the same center.
    }
    //棋子最后覆盖在桥上并带白边，不会和下面的桥线混在一起。
    //Pegs cover bridges and keep a white rim, so both remain visually clear.

    // Hao Guo: render readable board-coordinate labels outside the frame.
    const auto x_labels = coordinate_label_values(
        board_.get_width(),
        length(to_vector(geometry.x_step)));
    //把棋盘宽度和 x 方向像素间距交给逻辑函数，得到要显示的 x 编号。
    //Uses board width and x pixel spacing to choose readable x labels.
    //auto 让编译器推导返回类型为 std::vector<int>。
    //auto lets the compiler infer std::vector<int>.

    const auto y_labels = coordinate_label_values(
        board_.get_height(),
        length(to_vector(geometry.y_step)));
    //这里用相同方法得到 y 坐标标签。
    //This chooses y labels in the same way.

    for (const int x : x_labels) {
        //依次处理逻辑函数选出的每一个 x 标签值。
        //Visits every selected x label value.
        const Vector2 anchor = board_to_screen(
            geometry,
            static_cast<float>(x),
            -0.5F);
        //x 使用当前编号，y=-0.5F 表示锚点位于第一行网格点上方半格。
        //x is the label value; y=-0.5 places the anchor half a cell above.

        draw_centered_text(
            std::to_string(x),
            outward_position(
                anchor,
                to_vector(geometry.center),
                18.0F),
            12,
            kSecondaryText);
        //to_string 把整数编号转换成文字；outward_position 再向棋盘外移 18 像素。
        //to_string makes text; outward_position shifts it 18 pixels outward.
        //最后以 12 号字和次要文字颜色围绕该位置居中绘制。
        //Finally it draws centered text at size 12 in the secondary color.
    }

    for (const int y : y_labels) {
        if (y == 0) continue;
        //y=0 与左上附近的 x=0 标签太接近，因此跳过一个，避免两个“0”重叠。
        //Skips y=0 so two zero labels do not overlap near the top-left.

        const Vector2 anchor = board_to_screen(
            geometry,
            -0.5F,
            static_cast<float>(y));
        //这次把 x 设为 -0.5F，让 y 标签锚点位于网格左侧半格。
        //Here x=-0.5 places each y anchor half a cell to the left.
        draw_centered_text(
            std::to_string(y),
            outward_position(
                anchor,
                to_vector(geometry.center),
                18.0F),
            12,
            kSecondaryText);
        //其余步骤与 x 标签相同：向外移动后居中绘制当前 y 编号。
        //The remaining steps match x labels: shift outward and draw centered.
    }

    // Hao Guo and Junke Pu: render a readable coordinate tooltip for every
    // grid point. Persistent labels provide orientation; this tooltip provides
    // the exact non-skipped coordinate on boards up to 96 x 96.
    if (hovered_position.has_value()) {
        //Junke Pu：只有鼠标附近找到有效棋盘坐标时，才显示精确坐标提示。
        const Vector2 point = board_to_screen(
            geometry,
            hovered_position->x,
            hovered_position->y);
        //把鼠标选中的棋盘坐标转换成黄色高亮圈的屏幕圆心。
        //Converts the hovered board coordinate to the highlight's screen center.

        DrawCircleLines(
            static_cast<int>(point.x),
            static_cast<int>(point.y),
            std::max(6.0F, step * 0.42F),
            kWinningColor);
        //DrawCircleLines 使用圆心 x、y、半径和颜色画一个不填充的圆环。
        //DrawCircleLines takes center x/y, radius and color for an outline.
        //半径取格距 42%，但至少 6 像素，保证大、小棋盘上都看得清。
        //Radius is 42% of step but at least 6 pixels for visibility.

        const std::string coordinate =
            "X: " + std::to_string(hovered_position->x) +
            "  Y: " + std::to_string(hovered_position->y);
        //把选中位置组合成类似 "X: 3  Y: 5" 的提示文字。
        //Builds tooltip text such as "X: 3  Y: 5".

        constexpr int kTooltipFontSize = 16;
        const int tooltip_width =
            MeasureText(coordinate.c_str(), kTooltipFontSize) + 18;
        //文字框宽度等于实际文字宽度再加 18 像素，左右各留约 9 像素内边距。
        //Tooltip width is text width plus 18 pixels, about 9 per side.

        constexpr int kTooltipHeight = 28;
        const Vector2 mouse = GetMousePosition();
        //提示框固定高 28 像素，并再次读取鼠标位置作为显示参考。
        //The tooltip is 28 pixels high and is positioned from the mouse.

        const float tooltip_x = std::clamp(
            mouse.x + 14.0F,
            8.0F,
            static_cast<float>(GetScreenWidth() - tooltip_width - 8));
        //通常把提示框放在鼠标右边 14 像素；clamp 保证左右至少留 8 像素。
        //Normally 14 pixels right of mouse; clamp keeps 8-pixel side margins.

        const float tooltip_y = std::clamp(
            mouse.y + 14.0F,
            8.0F,
            static_cast<float>(GetScreenHeight() - kTooltipHeight - 8));
        //y 使用相同方法，避免提示框超出窗口上边或下边。
        //The same calculation keeps it inside the top and bottom edges.

        DrawRectangleRounded(
            Rectangle{
                tooltip_x,
                tooltip_y,
                static_cast<float>(tooltip_width),
                static_cast<float>(kTooltipHeight)},
            0.25F,
            6,
            kTextColor);
        //Rectangle 四项是左上 x、左上 y、宽、高；后面是圆角比例、段数和颜色。
        //Rectangle stores x, y, width, height; then come roundness, segments, color.

        DrawText(
            coordinate.c_str(),
            static_cast<int>(tooltip_x) + 9,
            static_cast<int>(tooltip_y) + 6,
            kTooltipFontSize,
            kBoardBackground);
        //文字从框左侧加 9、顶部加 6 的位置开始，以白色画在深色背景上。
        //Text starts at +9/+6 padding and uses white on the dark background.
    }

    // Hao Guo: present the completed frame.
    EndDrawing();
    //结束本帧绘制并交换前后缓冲区，让刚才的全部内容显示到窗口。
    //Ends the frame and swaps buffers so all drawing becomes visible.
}

}  // namespace bruecken
