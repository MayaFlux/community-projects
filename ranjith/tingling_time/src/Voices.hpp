// NOLINTBEGIN
#pragma once

#include "TinglingTime.hpp"

namespace TinglingTime {

/**
 * @class Bank
 * @brief The eight voices: four sustained drones and four bells.
 *
 * The drones are stacks of slightly detuned sine partials. The bells are inharmonic and
 * coupled, so each strike rings as a cluster of modes. The voices arrive one at a time as
 * you play, drift in pitch, and travel between the two ears.
 */
class Bank {
public:
    Bank() = default;
    Bank(const Bank&) = delete;
    Bank& operator=(const Bank&) = delete;

    /** @brief Create every voice and place it in an ear. None is audible until it enters. */
    void build();

    /** @brief Bring a voice in. A drone swells over a few seconds, a bell is struck once. */
    void enter(size_t index);

    /** @brief Schedule the next unentered voice to enter after @p seconds. */
    void admit(double seconds);

    /** @brief Run @p fn with the bell and the strength each time a bell is struck. */
    void on_strike(std::function<void(size_t, double)> fn) { m_struck = std::move(fn); }

    [[nodiscard]] PerformMode mode() const { return m_mode; }

    /** @brief Change how the piece answers the pen. */
    void set_mode(PerformMode mode);

    /** @brief Strike bell @p body now, even if it is resting, provided it has entered. */
    void strike_body(size_t body, double strength);

    /** @brief Tune bell @p body to @p ratio times its home pitch. */
    void pitch(size_t body, double ratio);

    /**
     * @brief Send the bells to opposite ears over @p seconds.
     * @param away True sends the first and third bells to the second ear and the others to the first.
     */
    void turn(bool away, double seconds);

    /** @brief Strike the next bell that is not resting. In TAP a resting bell is struck anyway. */
    void strike(double strength);

    /** @brief Strike the next bell even if it is resting. */
    void force_strike(double strength);

    /** @brief Strike a random run of bells at falling pitch. Early on it is sparse and later it is dense. */
    void burst();

    /** @brief Set the drones' level from the pen's motion, according to the mode. */
    void drive(double motion);

    /** @brief Set every entered drone to an accented level. */
    void accent(bool recent);

    /** @brief Send each drone to the other ear whenever its last journey is done. */
    void tend_journeys();

    /** @brief Wander the drones' register, detune and partial balance. */
    void tend_drift();

    /** @brief Quiet the bells when together they grow too loud. */
    void enforce_cap();

    /** @brief Let each drone that has entered swell a little louder. */
    void tend_swell();

    /** @brief How loud the bells are together. */
    [[nodiscard]] double energy() const;

private:
    void route_leg(size_t index);
    void nudge_register(Voice& v);
    void burst_voice(double strength, double degrade);
    void resonate(Voice& v);
    void release(Voice& v);
    void struck(const Voice& v, double strength) const;
    void rest(Voice& v, double seconds);
    [[nodiscard]] double strike_interval() const;
    [[nodiscard]] double drift_range_hz() const;

    std::function<void(size_t, double)> m_struck;
    std::vector<Voice> m_voices;
    size_t m_body_count { 0 };
    size_t m_entered { 0 };
    size_t m_cursor { 0 };
    PerformMode m_mode { PerformMode::TAP };
};

/**
 * @class Garden
 * @brief Rules over three slow signals that strike the bells and turn them between the ears.
 *
 * Four rules read the same three slow sines. Two of the three high strikes the first
 * bell, an odd number high strikes the second bell and climbs a five note scale, a window
 * between two of them strikes the third bell, and the first two compared strike the fourth
 * bell. A fifth rule, a slow sine passing through zero, sends the first and third bells to
 * one ear and the others to the other, and back. Strikes are left to the pen until the mode
 * is RESONANT or FULL; the turns always happen.
 */
class Garden {
public:
    /** @brief Set the rules going over @p bank. */
    void build(Bank& bank);

private:
    std::shared_ptr<RelationNetwork> m_net;
};

/**
 * @class Written
 * @brief A sustained drone that is never struck, built one partial at a time.
 *
 * Fourteen partials of an inharmonic body rise and fall at unrelated slow rates. A slow
 * motion decides how quickly the upper ones fade, and another detunes them. The drone
 * glides in and out, so it arrives and leaves without a step.
 */
class Written {
public:
    /** @brief Make the drone and start it. It stays silent until settle() raises it. */
    void build();

    /** @brief Choose how present the drone is, 0 to 1. */
    void settle(double target) { m_target->store(static_cast<float>(target), std::memory_order_relaxed); }

private:
    std::shared_ptr<ModalNetwork> m_body;
    std::shared_ptr<RelationNetwork> m_control;
    std::shared_ptr<RelationNetwork> m_amp;
    std::shared_ptr<RelationNetwork> m_bend;
    std::shared_ptr<std::atomic<float>> m_target { std::make_shared<std::atomic<float>>(0.0F) };
};

} // namespace TinglingTime
// NOLINTEND
