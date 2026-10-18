// NOLINTBEGIN
#pragma once

#include "Drone.hpp"
#include "Grid.hpp"
#include "Score.hpp"

/**
 * @file Piece.hpp
 * @brief Topsoil: a machine of pure tones, clicks and noise whose score is written by cellular
 *        automata, over a ringing drone, with a living picture that follows it.
 *
 * Include this file and call Topsoil::topsoil() from user_project.hpp. The piece needs a window
 * and stereo output. No tablet or camera is used.
 */
namespace Topsoil {

/**
 * @class Machine
 * @brief Owns every part of Topsoil and starts them together.
 *
 * Three sine gates are driven by Kriya patterns, and the noise part is a Score on each output
 * channel. The first gate step to run sets the shared origin the score waits for. The drone, the
 * tape and the picture start alongside. Root mix normalisation is turned off on both channels, so
 * every part plays at its own level.
 */
class Machine {
public:
    /** @brief Make the voices, schedule the patterns and open the picture. */
    void build();

    /** @brief The score as something to read, valid once build has run. */
    [[nodiscard]] const std::shared_ptr<Cues>& cues() const { return m_cues; }

private:
    void gate(const Gate& spec, const Row& row, const std::string& name);

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Origin> m_origin { std::make_shared<Origin>() };
    std::shared_ptr<const Page> m_page;
    std::shared_ptr<Cues> m_cues;
    std::vector<std::shared_ptr<Buffers::AudioBuffer>> m_buffers;
    std::unique_ptr<DroneBank> m_drone;
    std::unique_ptr<Tape> m_tape;
    std::unique_ptr<Specimen> m_specimen;
};

/**
 * @brief Run Topsoil.
 *
 * The machine is kept alive by the engine, so no handle is needed. Call it from user_project.hpp.
 */
void topsoil();

} // namespace Topsoil
// NOLINTEND
