// NOLINTBEGIN
#pragma once

#include "Drone.hpp"
#include "Score.hpp"

/**
 * @file Grid.hpp
 * @brief The picture of Topsoil: many species of Life on a relaxing ground, which can rise into terrain.
 *
 * The picture is a grid of cells on a torus. Each cell holds a height, an age, a trail and a
 * species. Territories drift across the grid, and each territory runs its own Life-like rule and
 * builds or erodes the ground in its own way. The ground relaxes by diffusing and sliding off
 * steep slopes. The score reaches the picture through Cues, delayed and placed by the rule that
 * wrote each bar, and the drone shapes the terrain when it is switched on.
 */
namespace Topsoil {

/**
 * @brief The ecology of the grid, in the order the rule shader reads it.
 *
 * - wrap: 1 for a torus, 0 for clamped edges.
 * - territory: size of a territory in cells. Zero or less gives a single Conway species.
 * - phase: written every step from the clock, leave at zero.
 * - drift: how far, in territories, the borders wander.
 * - deposit: how much the ground rises under living cells.
 * - diffuse: how fast the ground blends toward its neighbours.
 * - talus: the slope above which the ground slides.
 * - slide: how much slides per step.
 * - trail_decay: how slowly the trail of a dead cell fades, per step.
 * - subside: the fraction of height returned to zero per step.
 * - spore: the chance per step that an empty cell with no trail is born.
 */
struct LifeRule {
    uint32_t wrap;
    float territory;
    float phase;
    float drift;
    float deposit;
    float diffuse;
    float talus;
    float slide;
    float trail_decay;
    float subside;
    float spore;
};

/** @brief Grid width in cells. Keep the grid and window aspect ratios equal. */
inline constexpr uint32_t k_grid_width = 3840;

/** @brief Grid height in cells. */
inline constexpr uint32_t k_grid_height = 2160;

/** @brief Fraction of the window the grid fills. */
inline constexpr float k_grid_extent = 1.0F;

/** @brief Size in pixels of the point drawn for each cell. */
inline constexpr float k_grid_point = 1.6F;

/** @brief Score events applied to the grid per step. Extra events wait their turn. */
inline constexpr size_t k_event_slots = 4;

/**
 * @class Specimen
 * @brief The picture: Life on a relaxing ground, answering the score and rising into terrain.
 *
 * Hits from the score reach the grid after a delay that differs by part and wanders slowly, so
 * the picture follows the rhythm without moving in step with it. Each hit lands where its place
 * in the bar and the automaton rule of that bar put it, so the same rule always plays on the same
 * part of the ground. The sub heaves the ground and the other parts seed colonies of growing
 * size.
 *
 * Press Space to raise the picture into terrain and press it again to lower it. Scroll tilts the
 * view. In terrain, five features, one per drone voice, rise and sink as the voices enter,
 * following their smoothed energy.
 */
class Specimen {
public:
    /** @param cues The score to follow. */
    explicit Specimen(std::shared_ptr<Cues> cues);

    /**
     * @brief Open the window, make the grid and start it.
     * @param drone The drone whose voices shape the terrain.
     */
    void build(const DroneBank& drone);

private:
    struct Pending {
        double due;
        std::array<float, 4> record;
    };

    [[nodiscard]] bool step();
    [[nodiscard]] std::array<float, 4> record(const Hit& hit) const;

    std::shared_ptr<Cues> m_cues;
    std::shared_ptr<Buffers::RelaxationGridBuffer> m_grid;
    std::vector<Pending> m_pending;
    uint32_t m_cycle { 0 };
};

} // namespace Topsoil
// NOLINTEND
