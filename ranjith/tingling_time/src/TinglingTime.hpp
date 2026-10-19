// NOLINTBEGIN
#pragma once

#include "MayaFlux/Buffers/Textures/TextureProcessor.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Input/TabletNode.hpp"

#include "MayaFlux/Kinesis/MotionChannel.hpp"

/**
 * @file TinglingTime.hpp
 * @brief Tingling Time: a pen on a tablet strikes bells and drones, and every strike pulls a
 *        moment from the last seconds of a camera back into the picture.
 *
 * Include Piece.hpp and call TinglingTime::tingling_time() from user_project.hpp. The piece needs
 * a pen tablet, a camera, a window and stereo output. This file holds the numbers that shape it:
 * how fast it unfolds, how the pen is read, and how the picture answers.
 */
namespace TinglingTime {

using namespace MayaFlux;

inline constexpr size_t k_modes = 10;
inline constexpr size_t k_bank_size = 8;
inline constexpr double k_energy_cap = 1.2;
inline constexpr double k_hard_press = 0.6;
inline constexpr double k_open_seconds = 150.0;
inline constexpr double k_pace = 4.0;
inline constexpr double k_swell_interval = 0.05;
inline constexpr double k_settle_seconds = 300.0;

inline constexpr double k_pen_event_rate = 150.0;
inline constexpr double k_motion_travel_gain = 6.0;
inline constexpr double k_motion_pressure_gain = 0.04;
inline constexpr double k_motion_follow = 0.2;

inline constexpr size_t k_bodies = k_bank_size / 2;
inline constexpr uint32_t k_eye_width = 640;
inline constexpr uint32_t k_eye_height = 360;
inline constexpr uint32_t k_eye_ring = 600;
inline constexpr double k_eye_step = 1.0 / 60.0;
inline constexpr double k_cut_near = 0.5;
inline constexpr double k_cut_far = 5.0;
inline constexpr double k_strike_tau = 2.5;
inline constexpr double k_lag_speed_still = 0.6;
inline constexpr double k_lag_speed_moving = 1.8;
inline constexpr double k_motion_ref = 0.3;

/** @brief A random value between @p start and @p end. */
[[nodiscard]] inline double local_uniform(double start, double end)
{
    thread_local Kinesis::Stochastic::Stochastic generator(Kinesis::Stochastic::Algorithm::UNIFORM);
    return generator(start, end);
}

/** @brief Seconds into the piece, run k_pace times faster than written. */
[[nodiscard]] inline double paced_now()
{
    const auto scheduler = get_scheduler();
    return scheduler ? scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_time() * k_pace : 0.0;
}

/** @brief A timer that runs on the engine's clock. */
[[nodiscard]] inline std::shared_ptr<Kriya::Timer> make_timer()
{
    const auto scheduler = get_scheduler();
    return scheduler ? std::make_shared<Kriya::Timer>(*scheduler) : nullptr;
}

/** @brief Fraction of the way to a target covered per step, restated so the glide takes k_pace times less time. */
[[nodiscard]] inline double paced(double fraction)
{
    return 1.0 - std::pow(1.0 - fraction, k_pace);
}

/** @brief Chance per step, restated so the event comes k_pace times as often. */
[[nodiscard]] inline double chance(double per_step)
{
    return std::min(1.0, per_step * k_pace);
}

/** @brief Hold @p open_v until the opening ends, glide to @p settle_v, then hold it. */
[[nodiscard]] inline double envelope_lerp(double open_v, double settle_v, double t)
{
    if (t < k_open_seconds) {
        return open_v;
    }
    if (t < k_settle_seconds) {
        return open_v + (settle_v - open_v) * (t - k_open_seconds) / (k_settle_seconds - k_open_seconds);
    }
    return settle_v;
}

/** @brief Exponential map of @p x in 0 to 1 onto @p f_min to @p f_max. */
[[nodiscard]] inline double exp_map(double x, double f_min, double f_max)
{
    return f_min * std::pow(f_max / f_min, std::clamp(x, 0.0, 1.0));
}

enum class Temperament : uint8_t { CONTINUOUS,
    PERCUSSIVE };

/**
 * @brief How the piece answers the pen. Chosen with N1 to N4.
 *
 * TAP: the pen strikes the bells alone and the drones hold a fixed level. TAP_MOTION: the
 * pen's motion also shapes the strikes and lifts the drones. RESONANT: the bells are also set
 * ringing by a drone, the drones wander, and rules strike the bells too. FULL: everything, with
 * bursts of strikes after a touch.
 */
enum class PerformMode : uint8_t { TAP,
    TAP_MOTION,
    RESONANT,
    FULL };

/**
 * @struct Voice
 * @brief One voice of the piece: a sustained drone or a bell, with its pitch and the ear it sits in.
 */
struct Voice {
    std::shared_ptr<ModalNetwork> net;
    std::shared_ptr<Node> drone;
    std::vector<std::shared_ptr<Sine>> drone_partials;
    std::vector<double> drone_base_freqs;
    std::vector<double> drone_detune_hz;
    std::vector<double> drone_detune_target;
    std::shared_ptr<std::vector<double>> drone_balance;
    std::vector<double> drone_balance_target;
    std::shared_ptr<Kriya::Timer> entry;
    std::shared_ptr<Kriya::Timer> refractory;
    Temperament temperament { Temperament::CONTINUOUS };
    double root { 55.0 };
    double root_current { 55.0 };
    double root_target { 55.0 };
    double journey_seconds { 40.0 };
    uint32_t side { 0 };
    size_t body { 0 };
    bool entered { false };
    bool resonated { false };
    double swell_level { 0.0 };
    double swell_seconds { 1.0 };
};

} // namespace TinglingTime
// NOLINTEND
