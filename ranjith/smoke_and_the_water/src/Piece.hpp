// NOLINTBEGIN
#pragma once

#include "Response.hpp"

namespace Attrition {

/**
 * @class Piece
 * @brief Brings the moving volume, the pen, and five musical voices together.
 *
 * Touch shapes the material, the voices respond to its changes, and the image
 * follows as the original forms gradually give way.
 *
 * Controls: N1-N4 choose a form and N0 restarts it; F1-F5 toggle voices; F6
 * toggles automatic arrivals; Up and Down adjust intensity; H hushes or
 * releases the sound; B toggles the breathing pulse; F freezes the voices;
 * Enter starts or stops recording the volume.
 */
class Piece : public std::enable_shared_from_this<Piece> {
public:
    /**
     * @brief Begin the work when a pen tablet is connected.
     */
    void start();

private:
    void enter(size_t index);
    void bind_keys();
    void schedule();

    std::shared_ptr<Core::Window> m_window;
    std::vector<Mode> m_modes;
    size_t m_mode { 0 };

    Hand m_hand;
    Mass m_mass;
    Survey m_survey;
    Stirrer m_stirrer;
    Look m_look;
    Voices m_voices;

    Measures m_measures;
    Feel m_feel;
    Conduct m_conduct;
};

/**
 * @brief Run Smoke and the Water.
 */
void attrition();

} // namespace Attrition
// NOLINTEND
