// NOLINTBEGIN
#pragma once

#include "TinglingTime.hpp"

namespace TinglingTime {

/**
 * @struct Gesture
 * @brief What the pen is doing: its speed, its pressure, and a smoothed motion that drives the piece.
 *
 * Position is the tablet's own 0 to 1 coordinates, so speed is in those units per second.
 * Motion blends speed and pressure and is what the drones and the picture follow.
 */
struct Gesture {
    std::atomic<float> speed { 0.0F };
    std::atomic<float> pressure { 0.0F };
    std::atomic<float> motion { 0.0F };
    std::atomic<bool> touching { false };
};

/**
 * @class Hand
 * @brief The pen, smoothed.
 *
 * Position and pressure are smoothed so a shaky hand and the tablet's own noise do not
 * reach the piece. The Hand reports the pen touching down, every frame of its motion,
 * and lifting.
 */
class Hand {
public:
    /**
     * @struct Reactions
     * @brief What happens when the pen speaks.
     *
     * Contact receives the pressure at touch down. Frame runs on every frame of pen motion.
     */
    struct Reactions {
        std::function<void(float)> contact;
        std::function<void()> frame;
    };

    Hand() = default;
    Hand(const Hand&) = delete;
    Hand& operator=(const Hand&) = delete;

    /** @brief Whether a tablet is present. */
    [[nodiscard]] static bool present();

    /**
     * @brief Open a window listing the connected pens and report the one chosen.
     *
     * @p chosen runs once with the pen's id, or with nothing when the window is closed or
     * no pen is present.
     */
    static void choose(std::function<void(std::optional<uint32_t>)> chosen);

    /**
     * @brief Start reading the pen @p tool_id.
     * @return False when the pen is not present.
     */
    bool bind(uint32_t tool_id, Reactions reactions);

    [[nodiscard]] const Gesture& gesture() const { return m_gesture; }

private:
    void frame(Nodes::Input::TabletContext& ctx);

    std::shared_ptr<Nodes::Input::TabletNode> m_node;
    std::unique_ptr<Kinesis::MotionChannel<glm::vec2>> m_pos;
    std::unique_ptr<Kinesis::MotionChannel<double>> m_press;

    Reactions m_reactions;
    Gesture m_gesture;
    double m_motion { 0.0 };
    bool m_touching { false };
    std::chrono::steady_clock::time_point m_last;
};

/**
 * @class Eye
 * @brief The camera as a stack of moments from its own recent past, called back by the strikes.
 *
 * The camera is recorded into a ring of recent frames and drawn through shaders/echo.frag.
 * The live picture is always there. Each bell owns one more layer: a strike cuts that
 * layer back into the recording, further for a harder strike, and a region of the picture,
 * shaped by noise and by the live brightness, turns into that moment. The region grows
 * with the strike and recedes as it fades, and the pen's motion sets how fast the past plays.
 */
class Eye {
public:
    /**
     * @brief Choose a camera and start drawing its past into @p window.
     * @return False when the picture could not be started.
     */
    bool open(const std::shared_ptr<Core::Window>& window);

    /** @brief Call back a moment in the picture of bell @p body, harder for a @p strength nearer 1. */
    void strike(size_t body, double strength);

    /** @brief Tell the picture how fast the pen is moving, which sets how fast the past plays. */
    void move(double motion);

private:
    struct Shared {
        std::array<std::atomic<float>, k_bodies> strikes {};
        std::atomic<float> motion { 0.0F };
    };

    std::shared_ptr<Shared> m_shared { std::make_shared<Shared>() };
    Kriya::Chimera m_chimera;
};

} // namespace TinglingTime
// NOLINTEND
