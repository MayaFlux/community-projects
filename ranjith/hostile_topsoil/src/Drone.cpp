// NOLINTBEGIN
#include "Drone.hpp"

#include "MayaFlux/Kinesis/Discrete/Series.hpp"
#include "MayaFlux/Kinesis/Tendency/TimeMap.hpp"

namespace Topsoil {

namespace {

    double smooth(double x)
    {
        const double c = std::clamp(x, 0.0, 1.0);
        return c * c * (3.0 - 2.0 * c);
    }

    double chord_ratio(double seconds)
    {
        double total = 0.0;
        for (const ChordStep& step : k_drone_chord) {
            total += step.seconds;
        }

        double at = std::fmod(seconds, total);
        for (const ChordStep& step : k_drone_chord) {
            if (at < step.seconds) {
                return step.ratio;
            }
            at -= step.seconds;
        }
        return k_drone_chord.back().ratio;
    }

    constexpr std::array<double, k_tape_per_group> k_bar { 1.0, 2.756, 5.404, 8.933, 13.344, 18.64 };
    constexpr double k_golden = 1.6180339887;
    constexpr double k_enter = 8.0;

    Kinesis::TimeMap drifting(const std::shared_ptr<double>& base, const std::shared_ptr<double>& window, const std::shared_ptr<const double>& velocity)
    {
        Kinesis::TimeMap drift = Kinesis::TimeMaps::integrated(0.0, velocity);
        return { .fn = [drift, base, window](const double& t) -> double {
            return Kinesis::wrap<double>(drift(t), *base, *base + *window);
        } };
    }

}

DroneBank::DroneBank(std::shared_ptr<Vruta::TaskScheduler> scheduler)
    : m_scheduler(std::move(scheduler))
    , m_radius(std::exp(-1.0 / (k_drone_ring * static_cast<double>(Buffers::s_registered_sample_rate))))
    , m_rate(static_cast<double>(Buffers::s_registered_sample_rate))
{
}

void DroneBank::build()
{
    m_start = m_scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position();

    for (const DroneVoice& spec : k_drone_voices) {
        Voice voice;
        voice.clicks = vega.Impulse(spec.rate, k_drone_click) | Audio;
        voice.clicks->exclude_output_from_graph();

        for (uint32_t channel = 0; channel < k_channels; ++channel) {
            voice.buffers[channel] = vega.NodeBuffer(channel, 512, voice.clicks) | Audio[channel];
            voice.loops[channel] = std::make_shared<Buffers::FeedbackProcessor>(
                resonator(), std::vector<double> { 1.0, 2.0 },
                std::vector<double> { 0.0, 0.0, m_radius * m_radius, 0.0, k_drone_lift });
            voice.buffers[channel]->get_processing_chain()->add_processor(voice.loops[channel], voice.buffers[channel]);
        }
        m_voices.push_back(std::move(voice));
    }

    for (size_t i = 0; i < m_voices.size(); ++i) {
        retune(m_voices[i], i, m_ratio);
    }

    schedule_metro(k_drone_tick, [this] { advance(); }, "topsoil_drone");
}

double DroneBank::ratio_at(double seconds) const
{
    return chord_ratio(std::max(seconds, 0.0) * k_drone_pace);
}

double DroneBank::presence(size_t voice, double seconds) const
{
    const DroneEvolution& life = k_drone_evolution[voice];
    const double t = std::max(seconds, 0.0) * k_drone_pace;
    const double wave = 0.5 + 0.5 * std::sin(2.0 * std::numbers::pi * (t / life.swell_period + life.swell_phase));
    return smooth((t - life.onset) / life.fade) * smooth((wave - 0.2) / 0.5);
}

void DroneBank::advance()
{
    const uint64_t now = m_scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position();
    const double seconds = static_cast<double>(now - m_start) / m_rate * k_drone_pace;

    m_ratio += (chord_ratio(seconds) - m_ratio) * k_drone_portamento * k_drone_pace;

    for (size_t i = 0; i < m_voices.size(); ++i) {
        const Voice& voice = m_voices[i];
        const DroneVoice& spec = k_drone_voices[i];
        const DroneEvolution& life = k_drone_evolution[i];

        const double presence = smooth((seconds - life.onset) / life.fade);
        const double wave = 0.5 + 0.5 * std::sin(2.0 * std::numbers::pi * (seconds / life.swell_period + life.swell_phase));
        const double swell = smooth((wave - 0.2) / 0.5);
        const double level = spec.level * k_drone_master * k_master * presence * swell;

        const double drift = 1.0 + k_drone_wander * std::sin(2.0 * std::numbers::pi * seconds / life.drift_period + static_cast<double>(i));
        voice.clicks->set_frequency(static_cast<float>(spec.rate * drift));

        for (size_t channel = 0; channel < k_channels; ++channel) {
            auto coefficients = voice.loops[channel]->coefficients(voice.buffers[channel]);
            if (coefficients.size() >= 5) {
                coefficients[3] = level;
            }
        }
        retune(voice, i, m_ratio);
    }
}

void DroneBank::retune(const Voice& voice, size_t index, double ratio)
{
    for (size_t channel = 0; channel < k_channels; ++channel) {
        const double pitch = k_drone_voices[index].frequency * ratio * (channel == 0 ? 1.0 : k_drone_detune);
        const double omega = 2.0 * std::numbers::pi * pitch / m_rate;

        auto coefficients = voice.loops[channel]->coefficients(voice.buffers[channel]);
        if (coefficients.size() >= 5) {
            coefficients[0] = std::sin(omega);
            coefficients[1] = 2.0 * m_radius * std::cos(omega);
            coefficients[2] = m_radius * m_radius;
        }
    }
}

Tape::Tape(const DroneBank& drone)
    : m_drone(drone)
{
}

double Tape::draw(double lo, double hi)
{
    return m_dice(lo, hi);
}

double Tape::target(const Layer& layer, double morph) const
{
    const Group& group = k_tape_groups[layer.group];
    if (!group.morphs) {
        return m_ladders[layer.group][layer.index];
    }
    const double harmonic = static_cast<double>(layer.index + 1);
    return group.root * (harmonic + (k_bar[layer.index] - harmonic) * morph);
}

void Tape::build()
{
    m_scheduler = get_scheduler();
    m_rate = static_cast<double>(Buffers::s_registered_sample_rate);
    m_dice.seed(0x7a9e);

    m_ring = create_ring(k_tape_ring, static_cast<uint32_t>(k_drone_voices.size() * k_channels));

    std::vector<Kriya::CaptureBuilder> sources;
    for (size_t voice = 0; voice < k_drone_voices.size(); ++voice) {
        for (uint32_t channel = 0; channel < k_channels; ++channel) {
            sources.push_back(Kriya::BufferOperation::capture_from(m_drone.buffer(voice, channel)));
        }
    }
    m_recorders = record_into(m_ring, std::move(sources));

    auto builder = create_tap_set_from_stream(m_ring);

    for (size_t g = 0; g < k_tape_groups.size(); ++g) {
        const Group& group = k_tape_groups[g];
        if (!group.morphs) {
            const auto ladder = Kinesis::Discrete::power_series(k_tape_per_group, k_golden, 0.22, group.root);
            std::ranges::copy(ladder, m_ladders[g].begin());
        }

        for (size_t n = 0; n < k_tape_per_group; ++n) {
            Layer layer;
            layer.group = g;
            layer.index = n;
            layer.onset = group.onset + 2.0 * static_cast<double>(n);
            layer.base = draw(1.5, 6.0);
            layer.window = draw(1.5, 5.0);
            layer.swell_period = draw(8.0, 21.0);
            layer.swell_phase = draw(0.0, 6.2831853);

            const double middle = target(layer, 0.5);
            double best = 1e9;
            for (size_t voice = 0; voice < k_drone_voices.size(); ++voice) {
                const double ratio = middle / k_drone_voices[voice].frequency;
                if (ratio < 0.35 || ratio > 6.0) {
                    continue;
                }
                if (std::abs(std::log(ratio)) < best) {
                    best = std::abs(std::log(ratio));
                    layer.voice = voice;
                }
            }

            for (uint32_t ear = 0; ear < k_channels; ++ear) {
                auto reader = std::make_shared<Reader>();
                reader->base = layer.base + 0.011 * static_cast<double>(ear);
                reader->window = layer.window;
                layer.readers[ear] = reader;

                builder.tap()
                    .from_channel(static_cast<uint32_t>(layer.voice * k_channels + ear))
                    .on_channel(ear)
                    .lag(drifting(
                        std::shared_ptr<double>(reader, &reader->base),
                        std::shared_ptr<double>(reader, &reader->window),
                        std::shared_ptr<const double>(reader, &reader->velocity)))
                    .level(0.0);
            }

            m_layers.push_back(std::move(layer));
        }
    }

    m_taps = builder.start();
    m_start = m_scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position();

    schedule_metro(k_tape_tick, [this] { advance(); }, "topsoil_tape");
}

void Tape::advance()
{
    const uint64_t position = m_scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position();
    const double seconds = static_cast<double>(position - m_start) / m_rate;
    const double dt = seconds - m_last;
    m_last = seconds;

    const double morph = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * seconds / k_tape_morph);

    for (size_t i = 0; i < m_layers.size(); ++i) {
        Layer& layer = m_layers[i];
        const Group& group = k_tape_groups[layer.group];

        const double lag = Kinesis::wrap<double>(layer.travel, layer.base, layer.base + layer.window);
        const double chord = m_drone.ratio_at(seconds - lag);
        const double ratio = std::clamp(target(layer, morph) / (k_drone_voices[layer.voice].frequency * chord), 0.25, 8.0);
        const double velocity = 1.0 - ratio;
        layer.travel += velocity * dt;

        const double place = (lag - layer.base) / layer.window;
        const double window = std::sin(std::numbers::pi * place) * std::sin(std::numbers::pi * place);
        const double entry = smooth((seconds - layer.onset) / k_enter);
        const double swell = 0.65 + 0.35 * std::sin(2.0 * std::numbers::pi * seconds / layer.swell_period + layer.swell_phase);
        const double level = k_tape_gain * group.level * entry * swell * window * 0.5;

        for (size_t ear = 0; ear < k_channels; ++ear) {
            layer.readers[ear]->velocity = velocity;
            m_taps.level(i * k_channels + ear, level);
        }
    }
}

} // namespace Topsoil
// NOLINTEND
