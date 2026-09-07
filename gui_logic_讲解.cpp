/**
 * @file gui_logic_讲解.cpp
 * @brief Implements testable logic shared by GuiRenderer and GUI tests.
 * @author Hao Guo
 * @author Junke Pu
 */

#include "bruecken/gui_logic.h"  //引入对应头文件中的结构体和函数声明，保证声明与实现一致。

#include <algorithm>  //提供 std::min、std::max、std::clamp、std::reverse 等算法。
#include <cmath>      //提供 std::sqrt 和 std::ceil 等数学函数。
#include <queue>      //提供 std::queue，供 Junke Pu 的胜利路径广度优先搜索使用。
#include <stdexcept>  //提供 std::invalid_argument，用于报告无效参数。

namespace bruecken {
namespace {
//匿名命名空间让下面的辅助函数只在本 cpp 文件内部可见。
//The anonymous namespace keeps these helpers private to this cpp file.

GuiPoint add(GuiPoint first, GuiPoint second) {
    return {first.x + second.x, first.y + second.y};
}
//把两个二维向量的 x、y 分量分别相加。
//Adds the x and y components of two 2D vectors.

GuiPoint subtract(GuiPoint first, GuiPoint second) {
    return {first.x - second.x, first.y - second.y};
}
//用终点减起点，可以得到从起点指向终点的向量。
//Subtracting start from end produces a direction vector.

GuiPoint scale(GuiPoint point, float factor) {
    return {point.x * factor, point.y * factor};
}
//把向量的两个分量都乘以 factor，用来按比例缩放向量。
//Multiplies both components by factor to scale a vector.

float point_length(GuiPoint point) {
    return std::sqrt(point.x * point.x + point.y * point.y);
}
//Junke Pu：用勾股定理计算二维向量的长度，供鼠标点击范围判断使用。

}  // namespace

GuiBoardGeometry calculate_board_geometry(
    const Board& board,
    float screen_width,
    float screen_height) {
    //这个函数只负责计算数据，不调用 Raylib 绘图，所以可以独立做单元测试。
    //This function only calculates data, so it can be tested without Raylib.

    if (screen_width <= 0.0F || screen_height <= 0.0F) {
        throw std::invalid_argument("Screen dimensions must be positive");
    }
    //窗口宽度或高度小于等于 0 时，后面的布局计算没有意义，因此抛出异常。
    //A non-positive screen size is meaningless, so the function throws.
    //“||”表示“或”；只要两个条件中有一个成立，就会进入 if。
    //|| means OR; either invalid dimension enters the if block.

    //学校规定默认窗口大小为 720×720 像素。我们根据这个默认尺寸设计边距。
    //The school default is 720x720, so the margins are based on that size.
    //横向边距取 54 像素和窗口宽度 7.5% 中较大的值。
    //The horizontal margin is the larger of 54 pixels and 7.5% of width.
    const float available_left =
        std::max(54.0F, screen_width * 0.075F);
    const float available_right = screen_width - available_left;

    //顶部需要给状态栏留更多空间，因此至少保留 116 像素或高度的 16%。
    //The top reserves extra status space: at least 116 pixels or 16%.
    const float available_top =
        std::max(116.0F, screen_height * 0.16F);
    const float available_bottom =
        screen_height - std::max(52.0F, screen_height * 0.07F);
    //bottom 是屏幕高度减去底部边距，所以得到的是可用区域的下边界坐标。
    //bottom is screen height minus its margin, giving the usable lower edge.

    const float fraction =
        static_cast<float>(board.get_rotation_fraction());
    //get_rotation_fraction() 根据棋盘角度返回 0 到 1 的旋转比例。
    //get_rotation_fraction() maps the board angle to a value from 0 to 1.
    //static_cast<float> 把原来的 double 明确转换成这里使用的 float。
    //static_cast<float> explicitly converts the double result to float.

    const float available_width = available_right - available_left;
    const float available_height = available_bottom - available_top;
    //右边界减左边界得到可用宽度；下边界减上边界得到可用高度。
    //Right minus left gives width; bottom minus top gives height.

    const float unit = std::min(
        available_width / static_cast<float>(board.get_width() - 1),
        available_height / static_cast<float>(board.get_height() - 1));
    //width 个坐标点之间只有 width-1 个间隔，高度也是同样道理。
    //width coordinate points contain width-1 gaps; height works the same.
    //分别算出横向和纵向最多能放多大的一格，再取较小值。
    //It calculates both possible step sizes and keeps the smaller one.
    //因此棋盘一定放得进可用区域，而且 x、y 方向每格像素距离相等。
    //Thus the board fits and x/y steps have equal pixel length.

    const float width =
        unit * static_cast<float>(board.get_width() - 1);
    const float height =
        unit * static_cast<float>(board.get_height() - 1);
    //用“每格大小 × 间隔数量”得到棋盘固定网格的实际像素宽高。
    //Step size times gap count gives the fixed grid's pixel dimensions.

    const float left =
        available_left + (available_width - width) * 0.5F;
    const float top =
        available_top + (available_height - height) * 0.5F;
    //多余空间乘 0.5F，表示左右或上下各分一半，从而让棋盘居中。
    //Half of the spare space on each side centers the board.

    const float right = left + width;
    const float bottom = top + height;
    //知道左上角和实际宽高后，就能得到右边界与下边界。
    //The top-left position plus dimensions gives the right and bottom edges.

    GuiBoardGeometry result{};
    //花括号 {} 会对结构体全部成员进行值初始化，然后再逐项填写。
    //{} value-initializes every member before the fields are assigned.

    result.grid_top_left = {left, top};
    result.grid_top_right = {right, top};
    result.grid_bottom_right = {right, bottom};
    result.grid_bottom_left = {left, bottom};
    //这四行保存固定矩形网格的左上、右上、右下、左下四个角。
    //These lines store the four corners of the fixed rectangular grid.
    //四行结构相同，理解第一行后，其余只是换到另外三个方向。
    //The other three lines repeat the first assignment for other corners.

    result.top_left = {left + fraction * width, top};
    //旋转比例越大，实际棋盘的左上角就越沿上边向右移动。
    //A larger rotation fraction moves the real top-left corner rightward.
    result.top_right = {right, top + fraction * height};
    //右上角负责实际棋盘的右侧上方位置。
    //This stores the real board's top-right corner.
    result.bottom_right = {right - fraction * width, bottom};
    //右下角负责实际棋盘的下侧右方位置。
    //This stores the real board's bottom-right corner.
    result.bottom_left = {left, bottom - fraction * height};
    //左下角负责实际棋盘的左侧下方位置。
    //This stores the real board's bottom-left corner.

    result.x_step = scale(
        subtract(result.grid_top_right, result.grid_top_left),
        1.0F / static_cast<float>(board.get_width() - 1));
    //先用“右上 - 左上”得到固定网格完整的横向向量。
    //First, top-right minus top-left gives the full horizontal vector.
    //再除以横向间隔数量，得到棋盘 x 增加 1 时的屏幕移动向量。
    //Dividing by the gap count gives one board-x step on screen.

    result.y_step = scale(
        subtract(result.grid_bottom_left, result.grid_top_left),
        1.0F / static_cast<float>(board.get_height() - 1));
    //这里同样计算棋盘 y 增加 1 时的屏幕移动向量。
    //This similarly calculates one board-y step on screen.

    result.center = scale(
        add(
            add(result.grid_top_left, result.grid_top_right),
            add(result.grid_bottom_left, result.grid_bottom_right)),
        0.25F);
    //把四个固定网格角相加再乘 1/4，得到四个角的平均值，也就是中心点。
    //Adding four corners and multiplying by 1/4 gives their center.

    return result;
    //返回完整几何数据；GuiRenderer 每一帧都会使用它来绘图。
    //Returns all geometry used by GuiRenderer for each frame.
}

GuiPoint board_coordinate_to_screen(
    const GuiBoardGeometry& geometry,
    int x,
    int y) {

    return add(
        geometry.grid_top_left,
        add(
            scale(geometry.x_step, static_cast<float>(x)),
            scale(geometry.y_step, static_cast<float>(y))));
    //计算公式是：左上角 + x 个横向步长 + y 个纵向步长。
    //Formula: top-left + x horizontal steps + y vertical steps.
    //棋盘旋转只改变可见边界，不移动固定 (x, y) 网格点。
    //Rotation changes the visible boundary, not the fixed (x, y) grid points.
}

std::optional<Position> find_nearest_board_position(
    const GuiBoardGeometry& geometry,
    const Board& board,
    GuiPoint mouse) {
    //Junke Pu：计算鼠标附近最近的、仍位于旋转棋盘内的网格坐标。

    const float step = std::min(
        point_length(geometry.x_step),
        point_length(geometry.y_step));
    const float radius = std::clamp(step * 0.48F, 5.0F, 14.0F);
    float best_distance = radius * radius;
    std::optional<Position> result;
    //使用平方距离比较，初始上限是允许点击半径的平方。

    for (int y = 0; y < board.get_height(); ++y) {
        for (int x = 0; x < board.get_width(); ++x) {
            if (!board.is_in_bounds(Position{x, y})) continue;

            const GuiPoint point =
                board_coordinate_to_screen(geometry, x, y);
            const float dx = mouse.x - point.x;
            const float dy = mouse.y - point.y;
            const float distance = dx * dx + dy * dy;

            if (distance < best_distance) {
                best_distance = distance;
                result = Position{x, y};
            }
        }
    }
    return result;
    //遍历结束后返回最近位置；没有点进入点击半径时 optional 为空。
}

std::optional<preset::Move> move_for_board_position(
    const Board& board,
    Position position) {
    //Junke Pu：把选中的坐标和当前玩家编号包装成 Move。

    preset::Move move(
        position.x,
        position.y,
        board.get_current_player() + 1);
    if (!board.is_valid_move(move)) return std::nullopt;
    return move;
    //主棋盘认为落子无效时返回空值，否则返回这一步。
}

std::vector<int> coordinate_label_values(
    int coordinate_count,
    float point_spacing) {
    //该函数决定哪些坐标编号长期显示，避免大棋盘上的数字互相挤在一起。
    //This function chooses persistent labels so large-board text does not crowd.

    if (coordinate_count <= 0 || point_spacing <= 0.0F) {
        throw std::invalid_argument(
            "Coordinate count and point spacing must be positive");
    }
    //坐标数量和点间距都必须是正数，否则后面无法安全计算步幅。
    //Both values must be positive before calculating a safe stride.

    const int stride = std::max(
        1,
        static_cast<int>(std::ceil(24.0F / point_spacing)));
    //目标是让相邻文字标签之间大约至少有 24 像素。
    //The goal is roughly at least 24 pixels between adjacent text labels.
    //例如点距为 8 像素时，ceil(24/8) 得到 3，即每三个坐标显示一次。
    //For 8-pixel spacing, ceil(24/8) is 3, so every third value is shown.
    //ceil 向上取整，static_cast<int> 再把结果明确转换成整数。
    //ceil rounds up, then static_cast<int> converts the result to int.
    //std::max(1, ...) 保证 stride 至少为 1，循环一定会向前进行。
    //std::max keeps stride at least 1, so the loop always advances.

    std::vector<int> labels;
    //建立一个动态数组，开始时为空，之后依次加入需要显示的编号。
    //Creates an empty dynamic array for the label values.

    for (int value = 0; value < coordinate_count; value += stride) {
        labels.push_back(value);
    }
    //从 0 开始，每次增加 stride，把对应坐标编号放到 labels 末尾。
    //Starts at 0 and appends every stride-th coordinate value.

    const int final_value = coordinate_count - 1;
    //下标从 0 开始，所以最后一个有效坐标是总数量减 1。
    //Indexes start at 0, so the final valid value is count minus 1.

    if (labels.back() != final_value) {
        labels.push_back(final_value);
    }
    //如果普通步幅没有正好走到终点，就手动补上最后一个坐标。
    //If the stride misses the endpoint, the last coordinate is appended.
    //这样无论棋盘多大，坐标轴的两个端点都能显示。
    //This keeps both axis endpoints visible on every board size.

    return labels;
}

std::vector<Position> calculate_winning_path(
    const Board& board,
    int player_id) {
    //Junke Pu：通过 BFS 从一侧边界搜索到对侧边界，重建获胜桥路径。

    if (player_id < 0 || player_id >= kNumPlayers) {
        throw std::invalid_argument("player_id must be 0 or 1");
    }//检查玩家编号是否合法

    std::vector<Peg> pegs;
    for (const Peg& peg : board.get_pegs()) {
        if (peg.player_id == player_id) {
            pegs.push_back(peg);
        }
    }//找出这个玩家的所有 Peg
    if (pegs.empty()) return {};

    std::vector<std::vector<int>> neighbours(pegs.size());//创建邻接表
    auto find_peg = [&](const Position& position)//创建一个叫 find_peg 的函数
     {
        for (int i = 0; i < static_cast<int>(pegs.size()); ++i)
        //从第 0 个棋子开始检查，找到相同位置后返回这个棋子的编号
         {
            if (pegs[i].pos == position) return i;
        }
        return -1;
    };

    for (const Bridge& bridge : board.get_bridges())//根据 Bridge 建立连接关系
     {
        if (bridge.player_id != player_id) continue;
        //如果这个 Bridge 不是当前玩家的，直接跳过
        const int from = find_peg(bridge.from);
        const int to = find_peg(bridge.to);
        if (from >= 0 && to >= 0) {
            neighbours[from].push_back(to);
            neighbours[to].push_back(from);
        }
    }

    std::queue<int> queue;//创建 BFS 队列
    std::vector<int> parent(pegs.size(), -1);//记录从哪个 Peg 走到当前 Peg
    std::vector<bool> visited(pegs.size(), false);//记录哪些 Peg 已经访问过
    for (int i = 0; i < static_cast<int>(pegs.size()); ++i) {
        const Direction direction = board.get_direction(pegs[i].pos);
        const bool start = player_id == 0
            ? direction == Direction::kTop
            : direction == Direction::kLeft;
        if (start) {
            queue.push(i);
            visited[i] = true;
        }
    }

    int goal = -1;
    while (!queue.empty()) {
        const int current = queue.front();
        queue.pop();//取出队列中最前面的棋子编号
        const Direction direction = board.get_direction(pegs[current].pos);
        const bool reached_goal = player_id == 0
            ? direction == Direction::kBottom
            : direction == Direction::kRight;
        if (reached_goal) {
            goal = current;
            break;
        }//到达对侧边界后，记录终点并停止搜索

        for (const int next : neighbours[current]) {
            if (!visited[next]) {
                visited[next] = true;
                parent[next] = current;
                queue.push(next);
            }
        }
        //继续访问通过桥连接且尚未搜索过的棋子
    }

    if (goal < 0) return {};//没有找到终点，说明不存在完整胜利路径
    std::vector<Position> path;//创建空路径
    for (int current = goal; current >= 0; current = parent[current]) {
        path.push_back(pegs[current].pos);
    }//根据 parent 从终点反向走回起点
    std::reverse(path.begin(), path.end());//翻转成从起点到终点的顺序
    return path;
}

std::string game_status_text(
    const Board& board,
    const std::vector<std::string>& player_names) {
    //Junke Pu：根据当前游戏阶段返回状态栏应该显示的文字。

    if (player_names.size() < kNumPlayers) {
        throw std::invalid_argument("Two player names are required");
    }
    if (board.get_phase() == GamePhase::kFinished) {
        const int winner = board.check_win(0) ? 0 : 1;
        return player_names[winner] + " wins!";
    }
    if (board.get_phase() == GamePhase::kDraw) {
        return "Draw";
    }
    return player_names[board.get_current_player()] + " to move";
}

}  // namespace bruecken
