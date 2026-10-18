// NOLINTBEGIN
#pragma once

#include "MayaFlux/MayaFlux.hpp"

/**
 * @file Score.hpp
 * @brief The rhythm of Topsoil: a four row score written by cellular automata, and ways to read it.
 *
 * The score is one phrase of k_phrase_bars bars on a grid of k_grid_hz steps per second. Each of
 * four parts (sub, tick, gate, noise) has a row of cells per bar, where `X` is an accented hit,
 * `x` a hit and `.` a rest. Most bars are one generation of an elementary automaton, listed in
 * k_strands, and a few are written by hand in make_page. Nothing is random.
 */
namespace Topsoil {

using namespace MayaFlux;

/** @brief Steps per second of the base grid. Every cell of every part lasts a whole number of steps. */
inline constexpr double k_grid_hz = 25.6;

/** @brief Steps in one bar. */
inline constexpr uint32_t k_bar_steps = 64;

/** @brief Bars in the phrase. The score repeats after this many bars. */
inline constexpr uint32_t k_phrase_bars = 16;

/** @brief Output channels. */
inline constexpr size_t k_channels = 2;

/** @brief Level of an accented hit (`X`). */
inline constexpr double k_accent = 1.0;

/** @brief Level of a plain hit (`x`). */
inline constexpr double k_plain = 0.55;

/**
 * @brief One gain on every sounding part: the sine gates, the noise and the drone.
 *
 * Hits that coincide can sum past one, where the output clamps, so raise it with care.
 */
inline constexpr double k_master = 1.6;

/** @brief Samples between amplitude updates of a gate envelope. Smaller is smoother and costs more. */
inline constexpr uint32_t k_ramp_samples = 24;

/** @brief The cells of one part, one string per bar. */
using Row = std::array<std::string, k_phrase_bars>;

/** @brief The score: the rows of the four parts, in the order sub, tick, gate, noise. */
struct Page {
    std::array<Row, 4> rows;
};

/**
 * @brief Level of a cell of a row, zero for a rest.
 * @param row The row to read.
 * @param cell Cell counted from the start of the phrase, wrapping after the last bar.
 */
[[nodiscard]] inline double level_at(const Row& row, uint64_t cell)
{
    const size_t width = row[0].size();
    const char mark = row[cell / width % k_phrase_bars][cell % width];
    return mark == 'X' ? k_accent : (mark == 'x' ? k_plain : 0.0);
}

/**
 * @brief Build the score from k_strands, the hand written bars and k_cuts.
 */
[[nodiscard]] std::shared_ptr<const Page> make_page();

/**
 * @brief The instant the score starts, in scheduler clock samples.
 *
 * Set by the first gate step to run, and shared by every part so they stay aligned.
 */
struct Origin {
    bool set { false };
    uint64_t position { 0 };
};

/**
 * @brief A sine part: a tone whose amplitude follows an envelope at every hit.
 *
 * rise, hold and fall are seconds of the envelope (Kinesis::Discrete::pulse_response). level
 * scales it. division is how many base steps one cell lasts.
 */
struct Gate {
    double frequency;
    double rise;
    double hold;
    double fall;
    double level;
    uint32_t division;
};

/**
 * @brief The noise part: a burst of white noise with the same envelope, panned per channel.
 */
struct Burst {
    double rise;
    double hold;
    double fall;
    double level;
    std::array<double, k_channels> pan;
    uint32_t division;
};

/** @brief The three sine parts: sub, tick and gate. */
inline constexpr std::array<Gate, 3> k_gates { {
    { 62.0, 0.004, 0.025, 0.05, 0.62, 8 },
    { 6000.0, 0.0002, 0.002, 0.003, 0.30, 1 },
    { 2000.0, 0.0006, 0.010, 0.004, 0.22, 2 },
} };

/** @brief The noise part. */
inline constexpr Burst k_burst { 0.0, 0.002, 0.004, 0.22, { 0.7, 0.7 }, 2 };

/**
 * @brief The hard cuts, as (bar, from_step) pairs.
 *
 * From from_step of that bar everything on the page is silent until the bar ends. Step zero
 * silences the whole bar. The drone ignores them.
 */
inline constexpr std::array<std::pair<size_t, size_t>, 4> k_cuts { { { 3, 48 }, { 7, 40 }, { 11, 56 }, { 15, 0 } } };

/** @brief The four parts, in the order of the rows of the page. */
enum class Part : size_t { Sub, Tick, Gate, Noise };

/** @brief How many base steps one cell of a part lasts. */
[[nodiscard]] constexpr uint32_t division(Part part)
{
    switch (part) {
    case Part::Sub:
        return k_gates[0].division;
    case Part::Tick:
        return k_gates[1].division;
    case Part::Gate:
        return k_gates[2].division;
    case Part::Noise:
        return k_burst.division;
    }
    return 1;
}

/** @brief Where in the bar the single live cell of an automaton starts. */
enum class Seed { First, Middle, Last };

/**
 * @brief One bar of a part written by an elementary automaton.
 *
 * rule is the Wolfram rule number, seed the starting cell and generations how many steps the
 * automaton has run when the bar is read.
 */
struct Strand {
    Part part;
    size_t bar;
    uint64_t rule;
    Seed seed;
    size_t generations;
};

/**
 * @brief The automaton bars of the score.
 *
 * Edit a rule or a generation count to change the rhythm of that bar. Bars not listed here, the
 * sub row and the noise hits of bars 6 to 9, are written by hand in make_page.
 */
inline constexpr std::array<Strand, 32> k_strands { {
    { Part::Tick, 0, 90, Seed::First, 0 },
    { Part::Tick, 1, 90, Seed::First, 2 },
    { Part::Tick, 2, 90, Seed::First, 4 },
    { Part::Tick, 3, 90, Seed::First, 6 },
    { Part::Tick, 4, 90, Seed::First, 8 },
    { Part::Tick, 5, 90, Seed::First, 10 },
    { Part::Tick, 6, 90, Seed::First, 12 },
    { Part::Tick, 7, 90, Seed::First, 14 },
    { Part::Tick, 8, 30, Seed::First, 1 },
    { Part::Tick, 9, 30, Seed::First, 3 },
    { Part::Tick, 10, 30, Seed::First, 5 },
    { Part::Tick, 11, 30, Seed::First, 7 },
    { Part::Tick, 12, 90, Seed::First, 6 },
    { Part::Tick, 13, 90, Seed::First, 4 },
    { Part::Tick, 14, 90, Seed::First, 2 },
    { Part::Gate, 2, 90, Seed::First, 0 },
    { Part::Gate, 3, 90, Seed::First, 0 },
    { Part::Gate, 4, 110, Seed::Last, 4 },
    { Part::Gate, 5, 110, Seed::Last, 8 },
    { Part::Gate, 6, 110, Seed::Last, 12 },
    { Part::Gate, 7, 110, Seed::Last, 16 },
    { Part::Gate, 8, 90, Seed::Middle, 2 },
    { Part::Gate, 9, 90, Seed::Middle, 4 },
    { Part::Gate, 10, 90, Seed::Middle, 6 },
    { Part::Gate, 11, 90, Seed::Middle, 8 },
    { Part::Gate, 12, 110, Seed::Last, 16 },
    { Part::Gate, 13, 110, Seed::Last, 8 },
    { Part::Gate, 14, 110, Seed::Last, 4 },
    { Part::Noise, 10, 22, Seed::Middle, 2 },
    { Part::Noise, 11, 22, Seed::Middle, 5 },
    { Part::Noise, 12, 22, Seed::Middle, 8 },
    { Part::Noise, 13, 22, Seed::Middle, 11 },
} };

/**
 * @brief One struck cell of the score.
 *
 * cell counts from the origin, and seconds is the time it sounds, measured from the origin.
 */
struct Hit {
    Part part;
    uint64_t cell;
    double level;
    double seconds;
};

/**
 * @class Cues
 * @brief Reads the score without sounding it, so other things can follow the same rhythm.
 *
 * It knows where the score is now, which hits have come due and which automaton wrote a bar. It
 * reads only the page, the origin and the scheduler clock, so it can be polled at any rate
 * without touching the audio. Nothing is reported before the origin is set.
 */
class Cues {
public:
    /**
     * @param scheduler Source of the sample clock.
     * @param origin The shared start of the score.
     * @param page The score to read.
     */
    Cues(std::shared_ptr<Vruta::TaskScheduler> scheduler, std::shared_ptr<const Origin> origin, std::shared_ptr<const Page> page);

    /** @brief Whether the score has started. */
    [[nodiscard]] bool started() const;

    /** @brief Seconds since the origin, zero before it. */
    [[nodiscard]] double seconds() const;

    /** @brief The bar of the phrase now, from zero, wrapping after the last. */
    [[nodiscard]] size_t bar() const;

    /** @brief The cell of a part now, counted from the origin. */
    [[nodiscard]] uint64_t cell(Part part) const;

    /** @brief The automaton that wrote a bar of a part, or null if the bar is written by hand. */
    [[nodiscard]] const Strand* strand(Part part, size_t bar) const;

    /**
     * @brief The hits of every part that have come due since the last call, in time order.
     *
     * The first call after the score starts returns everything from the origin. Each hit is
     * returned once.
     */
    [[nodiscard]] std::vector<Hit> advance();

private:
    [[nodiscard]] uint64_t elapsed() const;
    [[nodiscard]] uint64_t step_samples(Part part) const;

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<const Origin> m_origin;
    std::shared_ptr<const Page> m_page;
    std::array<uint64_t, 4> m_next {};
    double m_rate { 0.0 };
    uint64_t m_base { 0 };
};

/**
 * @class Score
 * @brief Renders the noise part into an audio buffer, sample exact.
 *
 * Silent until the origin is set, then every sample is computed from the scheduler clock minus
 * the origin, so its hits coincide with the sine gates. Attach one per output channel; each
 * applies that channel's pan gain from k_burst.
 */
class Score : public Buffers::BufferProcessor {
public:
    /**
     * @param channel Output channel this instance renders.
     * @param scheduler Source of the sample clock.
     * @param origin The shared start of the score.
     * @param page The score; the noise row is read.
     */
    Score(uint32_t channel, std::shared_ptr<Vruta::TaskScheduler> scheduler, std::shared_ptr<Origin> origin,
        std::shared_ptr<const Page> page);

    void processing_function(const std::shared_ptr<Buffers::Buffer>& buffer) override;

    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffers::Buffer>& buffer) const override;

private:
    [[nodiscard]] double render(uint64_t position);

    uint32_t m_channel;
    uint32_t m_noise;
    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Origin> m_origin;
    std::shared_ptr<const Page> m_page;
    uint64_t m_step_samples;
    double m_rate;
};

} // namespace Topsoil
// NOLINTEND
