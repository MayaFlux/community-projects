// NOLINTBEGIN
#pragma once

#include "Senses.hpp"
#include "Voices.hpp"

namespace TinglingTime {

/**
 * @class Piece
 * @brief Brings the pen, the bells, the drones and the camera together.
 *
 * A touch of the pen strikes a bell, and a faster or harder hand lifts the drones. More
 * voices arrive as you play. Every strike pulls a moment of the camera's recent past into
 * the picture.
 *
 * Keys: N1 to N4 choose how the piece answers the pen: TAP, TAP_MOTION, RESONANT and FULL.
 * The written drone joins in RESONANT and FULL.
 */
class Piece : public std::enable_shared_from_this<Piece> {
public:
    /**
     * @brief Begin the piece: open the window, choose a camera, then choose a pen.
     *
     * A pen tablet must be connected. Without one the piece only says so and does not start.
     */
    void start();

private:
    void contact(float pressure);
    void choose(PerformMode mode);
    void frame();
    void bind_keys();
    void schedule();

    std::shared_ptr<Core::Window> m_window;
    std::shared_ptr<Kriya::Timer> m_recency;
    uint64_t m_contacts { 0 };

    Hand m_hand;
    Bank m_bank;
    Garden m_garden;
    Written m_written;
    Eye m_eye;
};

/**
 * @brief Run Tingling Time. Call it once from user_project.hpp.
 */
void tingling_time();

} // namespace TinglingTime
// NOLINTEND
