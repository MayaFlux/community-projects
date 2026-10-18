// NOLINTBEGIN
#pragma once

#include "Score.hpp"

#include "MayaFlux/Kinesis/Stochastic/Stochastic.hpp"

/**
 * @file Drone.hpp
 * @brief The ringing layer of Topsoil: a bank of resonating voices, and a tape that replays it.
 *
 * DroneBank makes five tight, grainy voices from click trains through resonators, on a slow clock
 * of its own that ignores the score. Tape records those voices and plays them back through many
 * taps at inharmonic ratios, so the layers together form a slowly moving cloud.
 */
namespace Topsoil {

/**
 * @brief One voice of the drone: its pitch in Hz, its click rate in Hz and its level.
 *
 * Each click excites a two-pole resonator at the pitch, so the voice sounds as a train of
 * short damped sine grains. The click rates share no common divisor, so the grains of different
 * voices interlock and never line up.
 */
struct DroneVoice {
    double frequency;
    float rate;
    double level;
};

/** @brief The five voices, from low to high. */
inline constexpr std::array<DroneVoice, 5> k_drone_voices { {
    { 220.0, 29.0F, 0.42 },
    { 329.63, 37.3F, 0.38 },
    { 440.0, 47.9F, 0.32 },
    { 659.26, 61.7F, 0.26 },
    { 1318.51, 83.1F, 0.16 },
} };

/** @brief Pitch ratio of the right channel against the left, so the two sides beat. */
inline constexpr double k_drone_detune = 1.004;

/** @brief Ring time of a voice in seconds. Longer rings smear the grains together. */
inline constexpr double k_drone_ring = 0.04;

/** @brief Amplitude of each click that excites a resonator. */
inline constexpr double k_drone_click = 0.3;

/** @brief Drive into the output limiter. Higher is denser and brighter. */
inline constexpr double k_drone_lift = 2.0;

/**
 * @brief A two-pole resonator as a feedback combiner, using taps at lags of one and two samples.
 *
 * The coefficients are sin(w), 2 r cos(w), r r, the output level and the output drive. The output
 * is level times tanh(drive times y), where y = c0 x + c1 y[-1] - c2 y[-2].
 */
[[nodiscard]] inline Buffers::FeedbackProcessor::CombineStep resonator()
{
    return [](double x, std::span<const double> taps, std::span<double> c) {
        const double y = c[0] * x + c[1] * taps[0] - c[2] * taps[1];
        return Buffers::FeedbackProcessor::Step { .out = c[3] * std::tanh(c[4] * y), .carry = y };
    };
}

/**
 * @brief How one voice changes over time, in seconds of the drone's clock.
 *
 * The voice is silent until onset and fades in over fade. It then swells in and out with period
 * swell_period (phase in cycles). Its click rate wanders by k_drone_wander with period
 * drift_period. The periods are prime or close to it, so the voices never realign with each
 * other or with the phrase of the score.
 */
struct DroneEvolution {
    double onset;
    double fade;
    double swell_period;
    double swell_phase;
    double drift_period;
};

/** @brief The evolution of each voice, in the order of k_drone_voices. */
inline constexpr std::array<DroneEvolution, k_drone_voices.size()> k_drone_evolution { {
    { 24.0, 20.0, 71.0, 0.00, 43.0 },
    { 38.0, 18.0, 53.0, 0.25, 59.0 },
    { 61.0, 18.0, 89.0, 0.50, 67.0 },
    { 83.0, 20.0, 61.0, 0.75, 79.0 },
    { 109.0, 24.0, 97.0, 0.10, 101.0 },
} };

/** @brief One chord step: the ratio the whole bank is tuned by, and how many seconds it is held. */
struct ChordStep {
    double ratio;
    double seconds;
};

/** @brief The chord sequence. It repeats, gliding between steps. */
inline constexpr std::array<ChordStep, 6> k_drone_chord { {
    { 1.0, 47.0 },
    { 9.0 / 8.0, 31.0 },
    { 6.0 / 5.0, 29.0 },
    { 1.0, 53.0 },
    { 4.0 / 3.0, 37.0 },
    { 9.0 / 8.0, 41.0 },
} };

/**
 * @brief How many times faster the drone's clock runs.
 *
 * Every onset, fade, swell, drift and chord hold above is divided by it, and the chord glide
 * follows, so the ratios between the clocks stay the same. 1 is the full length performance, 4
 * brings the whole arc into about a quarter of the time.
 */
inline constexpr double k_drone_pace = 4.0;

/** @brief Overall gain of the drone voices. */
inline constexpr double k_drone_master = 0.8;

/** @brief How far each voice's click rate wanders, as a fraction of its rate. */
inline constexpr double k_drone_wander = 0.45;

/** @brief Seconds between updates of the drone's levels, click rates and tuning. */
inline constexpr double k_drone_tick = 0.02;

/** @brief Speed of the glide between chord steps. Higher is faster. */
inline constexpr double k_drone_portamento = 0.004;

/**
 * @class DroneBank
 * @brief The drone: five resonating voices on a slow clock of their own.
 *
 * Each voice is a click train read by one buffer per channel, with a resonator on it. Every
 * k_drone_tick seconds the bank sets each voice's level (a late entry, then slow swells), its
 * click rate and the chord ratio, from the time since build alone. It does not read the score,
 * so it is neither rhythmic nor tied to the bars or the cuts.
 */
class DroneBank {
public:
    /** @param scheduler Source of the sample clock. */
    explicit DroneBank(std::shared_ptr<Vruta::TaskScheduler> scheduler);

    /** @brief Make the voices and start the clock. */
    void build();

    /** @brief The chord ratio a given number of real seconds after build. */
    [[nodiscard]] double ratio_at(double seconds) const;

    /** @brief How present a voice is, from zero to one, a given number of real seconds after build. */
    [[nodiscard]] double presence(size_t voice, double seconds) const;

    /**
     * @brief The buffer a voice sounds on in one channel.
     * @param voice Index into k_drone_voices.
     * @param channel Output channel.
     */
    [[nodiscard]] std::shared_ptr<Buffers::AudioBuffer> buffer(size_t voice, uint32_t channel) const
    {
        return m_voices.at(voice).buffers.at(channel);
    }

private:
    struct Voice {
        std::shared_ptr<Nodes::Generator::Impulse> clicks;
        std::array<std::shared_ptr<Buffers::NodeBuffer>, k_channels> buffers;
        std::array<std::shared_ptr<Buffers::FeedbackProcessor>, k_channels> loops;
    };

    void advance();
    void retune(const Voice& voice, size_t index, double ratio);

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::vector<Voice> m_voices;
    uint64_t m_start { 0 };
    double m_ratio { 1.0 };
    double m_radius { 0.0 };
    double m_rate { 0.0 };
};

/** @brief Seconds of drone history the tape keeps. */
inline constexpr double k_tape_ring = 60.0;

/** @brief Seconds between updates of the tape's speeds and levels. */
inline constexpr double k_tape_tick = 0.01;

/** @brief Overall gain of the tape. */
inline constexpr double k_tape_gain = 2.0;

/** @brief Seconds for the harmonic groups to move to the bar modes and back. */
inline constexpr double k_tape_morph = 90.0;

/** @brief Layers in each group. */
inline constexpr size_t k_tape_per_group = 6;

/**
 * @brief One group of tape layers: a root in Hz, whether it morphs, when it enters, and its level.
 *
 * Layer n of a morphing group sits at the root times a ratio that moves from n (the harmonic
 * series) to the nth transverse mode of a free bar (1, 2.756, 5.404, 8.933, 13.344, 18.64) and
 * back. A fixed group sits on a golden ratio ladder. Layers of a group enter two seconds apart,
 * starting at onset.
 */
struct Group {
    double root;
    bool morphs;
    double onset;
    double level;
};

/** @brief The three groups of the tape. */
inline constexpr std::array<Group, 3> k_tape_groups { {
    { 220.0, true, 8.0, 0.9 },
    { 355.9, true, 16.0, 0.7 },
    { 440.0, false, 26.0, 0.5 },
} };

/**
 * @class Tape
 * @brief The drone's own past, replayed by many layers at inharmonic ratios.
 *
 * The five voices on both channels are recorded into one circular ring. Each layer is a tap
 * that trails the write head and reads one voice at the ratio that lands it on its partial, so
 * together the layers act as an additive bank of partials made of tape. A layer's ratio is a
 * playback speed, so its lag falls or grows, and the lag is kept inside a window of the past.
 * Each layer fades in and out with where its lag sits in the window, so it never clicks. Layers
 * enter at staggered times and swell at their own rates. Everything is built once at build, and a
 * metro only updates speeds and levels.
 */
class Tape {
public:
    /** @param drone The bank whose voices are recorded and whose chord the taps follow. */
    explicit Tape(const DroneBank& drone);

    /** @brief Start recording, make the layers and start the clock. */
    void build();

private:
    struct Reader {
        double base { 1.5 };
        double window { 4.0 };
        double velocity { 0.0 };
    };

    struct Layer {
        size_t group { 0 };
        size_t index { 0 };
        size_t voice { 0 };
        double base { 1.5 };
        double window { 4.0 };
        double travel { 0.0 };
        double onset { 0.0 };
        double swell_period { 20.0 };
        double swell_phase { 0.0 };
        std::array<std::shared_ptr<Reader>, k_channels> readers;
    };

    [[nodiscard]] double draw(double lo, double hi);
    [[nodiscard]] double target(const Layer& layer, double morph) const;
    void advance();

    const DroneBank& m_drone;
    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Kakshya::DynamicSoundStream> m_ring;
    std::vector<std::shared_ptr<Kriya::BufferPipeline>> m_recorders;
    Kriya::TapSet m_taps;
    std::vector<Layer> m_layers;
    std::array<std::array<double, k_tape_per_group>, k_tape_groups.size()> m_ladders {};
    Kinesis::Stochastic::Stochastic m_dice { Kinesis::Stochastic::Algorithm::UNIFORM };
    uint64_t m_start { 0 };
    double m_rate { 0.0 };
    double m_last { 0.0 };
};

} // namespace Topsoil
// NOLINTEND
