/**
 * @file gui_renderer_讲解.h
 * @brief Declares the Raylib-based GUI for Knight Bridge.
 * @author Hao Guo
 * @author Junke Pu
 *
 * @par Contribution breakdown
 * - Hao Guo: window initialization and lifetime, board rendering, and the
 *   rendering of pegs and bridges.
 * - Junke Pu: mouse interaction, move placement, game-state display, and the
 *   integration of GuiRenderer with preset::PlayerGuiAccess.
 */

#pragma once
//防止这个头文件在一个编译单元中被重复包含。
//Prevents this header from being included more than once per translation unit.

#include <optional>  //提供 std::optional，用来暂存“可能存在”的鼠标落子。
#include <string>    //提供 std::string，用来保存颜色文本、玩家名、标题和反馈。
#include <vector>    //提供 std::vector，用来保存数量可变的玩家颜色和名字。

#include "bruecken/board.h"  //提供要显示的 Board，以及棋盘相关基础类型和常量。
#include "player_gui_access.h"  //提供学校预设的 GUI 输入接口 PlayerGuiAccess。

namespace bruecken {
//将 GuiRenderer 放进项目自己的命名空间，防止全局名称冲突。
//Places GuiRenderer in the project namespace to avoid global name conflicts.

/**
 * @brief Displays the game and provides mouse moves to a human player.
 *
 * The class combines two GUI concerns. Hao Guo's rendering code owns the
 * Raylib window and visualizes the board, pegs, and bridges. Junke Pu's
 * interaction code interprets mouse clicks, displays the current game state,
 * and exposes valid moves through preset::PlayerGuiAccess.
 *
 * @par Primary contributors
 * Hao Guo (rendering) and Junke Pu (interaction and PlayerGuiAccess
 * integration).
 */
class GuiRenderer final : public preset::PlayerGuiAccess {
//public 继承表示 GuiRenderer 是一种 PlayerGuiAccess，可以传给 HumanPlayer。
//Public inheritance makes GuiRenderer usable as a PlayerGuiAccess.
//final 表示不允许再创建继承 GuiRenderer 的子类。
//final prevents another class from deriving from GuiRenderer.
public:
    /**
     * @brief Creates the GUI and opens its Raylib window.
     * @param board Board whose current state is displayed. The board must
     *        outlive this renderer.
     * @param player_colors Player colors as hexadecimal RGB strings.
     * @param player_names Names shown in the game-state panel.
     * @param title Title of the operating-system window.
     *
     * @par Primary contributors
     * Hao Guo implemented window initialization and color setup; Junke Pu
     * integrated the player names used by the game-state display.
     */
    GuiRenderer(
        const Board& board,
        std::vector<std::string> player_colors,
        std::vector<std::string> player_names,
        std::string title = "Knight Bridge");
        //构造函数保存棋盘引用和显示配置，然后创建 Raylib 窗口。
        //The constructor stores the model/configuration and opens the window.
        //title 有默认值，所以调用者不传标题时使用 "Knight Bridge"。
        //title defaults to "Knight Bridge" when the caller omits it.
        //玩家名字参与状态栏显示，这是与 Junke Pu 代码交叉的输入。

    /**
     * @brief Closes the Raylib window if it is still open.
     * @par Primary contributor
     * Hao Guo.
     */
    ~GuiRenderer();
    //析构函数在对象生命周期结束时自动运行，负责释放窗口资源。
    //The destructor runs automatically and releases the window resource.

    GuiRenderer(const GuiRenderer&) = delete;
    GuiRenderer& operator=(const GuiRenderer&) = delete;
    //删除拷贝构造和拷贝赋值，防止两个对象误以为自己拥有同一个窗口。
    //Deleted copy operations stop two objects from owning the same window.

    /**
     * @brief Processes mouse input and renders one complete GUI frame.
     *
     * Junke Pu's part handles clicks, move validation, and the status panel.
     * Hao Guo's part renders the board, coordinates, pegs, and bridges.
     * The corresponding blocks are marked in gui_renderer.cpp.
     */
    void draw_frame();
    //每次调用处理一次输入并画出一帧；main 中会反复调用它。
    //Each call handles input and draws one frame; main calls it repeatedly.
    //鼠标和状态栏部分属于 Junke Pu，绘制部分属于 Hao Guo。

    /**
     * @brief Checks whether the GUI window should close.
     * @return `true` if the window is closed or a close request is pending.
     * @par Primary contributor
     * Hao Guo.
     */
    bool should_close() const;
    //函数末尾的 const 表示检查关闭状态时不会修改 GuiRenderer 成员。
    //The trailing const means this check does not modify renderer members.

    /**
     * @brief Gives the current human player's pending move to HumanPlayer.
     * @return The last valid clicked move, or std::nullopt if none is pending.
     *
     * A pending click is consumed by this call and is therefore returned at
     * most once. This override is the connection required by the school's
     * preset::PlayerGuiAccess interface.
     *
     * @par Primary contributor
     * Junke Pu.
     */
    std::optional<preset::Move>
    request_move_from_current_human_player() override;
    //Junke Pu：把尚未消费的鼠标落子交给 HumanPlayer，读取后清空。
    //override 让编译器检查它确实覆盖了基类中的虚函数。

private:
    //private 成员只能由 GuiRenderer 自己的成员函数直接访问。
    //private members can be accessed directly only by GuiRenderer itself.

    /** Shared game model read by both rendering and interaction code. */
    const Board& board_;
    //这里只保存引用，不复制棋盘；因此外部 Board 必须比 GuiRenderer 活得更久。
    //This stores a reference, not a copy, so Board must outlive GuiRenderer.
    //const 保证 GUI 只读取主棋盘，真正落子仍由游戏逻辑完成。
    //const keeps the GUI read-only; game logic applies the real moves.

    /** Colors used by Hao Guo's board, peg, and bridge rendering. */
    std::vector<std::string> player_colors_;
    //保存十六进制颜色字符串，绘制每一帧时会转换成 Raylib Color。
    //Stores hex color strings converted to Raylib Color for each frame.

    /** Names used by Junke Pu's game-state display. */
    std::vector<std::string> player_names_;
    //Junke Pu：状态栏显示用的两名玩家名字。

    /** Valid move waiting to be consumed (Junke Pu: interaction). */
    std::optional<preset::Move> pending_move_;
    //Junke Pu：暂存一次有效点击对应的落子。

    /** Message shown for the latest click (Junke Pu: interaction). */
    std::string feedback_;
    //Junke Pu：保存最近一次点击的提示文字。

    /** Turn associated with the pending click (Junke Pu: interaction). */
    int input_turn_ = -1;
    //Junke Pu：记录输入属于哪一回合，回合变化时清除旧点击。

    /** Tracks Raylib window ownership (Hao Guo: window lifetime). */
    bool window_open_ = false;
    //记录当前对象是否已经成功打开并拥有 Raylib 窗口。
    //Tracks whether this object successfully opened and owns the window.
};

}  // namespace bruecken
//结束 bruecken 命名空间。
//Ends the bruecken namespace.
