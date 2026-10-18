// NOLINTBEGIN
#include "Response.hpp"

namespace Attrition {

void Survey::reset(Measures& out)
{
    out = Measures {};
}

void Survey::step(Mass& mass, Measures& out)
{
    const Kinesis::Lattice3D& lat = mass.lattice();
    if (m_density.size() != lat.cell_count()) {
        m_density.assign(lat.cell_count(), 0.0F);
    }

    mass.volume()->read_field("density", m_density.data(), m_density.size() * sizeof(float));

    double sum = 0.0;
    double sq = 0.0;
    double cube = 0.0;
    double bad = 0.0;
    double filled = 0.0;
    double upper = 0.0;
    double lower = 0.0;
    double rough = 0.0;
    const auto n = static_cast<double>(m_density.size());

    const glm::uvec3 res = lat.resolution;
    const size_t plane = static_cast<size_t>(res.x) * res.y;
    const uint32_t mid = res.y / 2;

    for (size_t i = 0; i < m_density.size(); ++i) {
        const float v = m_density[i];
        if (!std::isfinite(v)) {
            bad += 1.0;
            continue;
        }

        const auto d = static_cast<double>(v);
        sum += d;
        sq += d * d;
        cube += d * d * d;

        if (d > 0.045) {
            filled += 1.0;
        }

        const auto y = static_cast<uint32_t>((i % plane) / res.x);
        if (y >= mid) {
            upper += d;
        } else {
            lower += d;
        }

        if ((i % res.x) + 1 < res.x) {
            const float next = m_density[i + 1];
            if (std::isfinite(next)) {
                rough += std::abs(static_cast<double>(next) - d);
            }
        }
    }

    const double mean = sum / n;
    out.variance = std::max(sq / n - mean * mean, 0.0);
    out.broken = bad / n;
    out.occupancy = filled / n;
    out.roughness = std::min(rough / n * 14.0, 1.0);

    const double both = upper + lower;
    out.imbalance = (both > 1e-9) ? ((upper - lower) / both) : 0.0;

    const double sd = std::sqrt(out.variance);
    out.skew = (sd > 1e-6)
        ? std::clamp((cube / n - 3.0 * mean * out.variance - mean * mean * mean) / (sd * sd * sd) / 4.0,
              -1.0, 1.0)
        : 0.0;

    ++out.surveys;
    if (out.surveys <= k_opening_surveys) {
        out.opening = std::max(out.opening, out.variance);
    }

    out.structure = (out.opening > 1e-9) ? std::clamp(out.variance / out.opening, 0.0, 1.0) : 1.0;

    ++m_churn;
    const double lost = out.lost();
    const auto roll = static_cast<uint64_t>(
        std::abs(out.roughness * 6.1e5 + out.imbalance * 2.7e5 + static_cast<double>(m_churn) * 1013.0));

    if (lost > static_cast<double>(out.rung + 1) * 0.13 && (roll % 100ULL) >= k_rung_reluctance) {
        mass.step_down(out.rung);
        ++out.rung;
    }
}

void Look::reset()
{
    m_lost = 0.0;
    m_broken = 0.0;
}

void Look::colour(Mass& mass, const Mode& mode, const Feel& feel) const
{
    const auto t = static_cast<float>(std::clamp(m_lost, 0.0, 1.0));
    const auto rot = static_cast<float>(std::clamp(m_broken * 6.0, 0.0, 1.0));

    const float a = glm::clamp(t * 2.0F, 0.0F, 1.0F);
    const float b = glm::clamp(t * 2.0F - 1.0F, 0.0F, 1.0F);

    glm::vec3 low = glm::mix(glm::mix(mode.palette.low[0], mode.palette.low[1], a), mode.palette.low[2], b);
    glm::vec3 high = glm::mix(glm::mix(mode.palette.high[0], mode.palette.high[1], a), mode.palette.high[2], b);

    low = glm::mix(low, glm::vec3(0.24F, 0.02F, 0.28F), rot);
    high = glm::mix(high, glm::vec3(1.0F, 0.34F, 0.72F), rot);

    auto smoke = mass.march()->march_processor();
    smoke->set_emission_ramp(low, high);
    smoke->set_emission(mode.emission + t * 0.5F + rot * 1.4F + static_cast<float>(feel.loudness) * k_loud_glow);
}

void Look::step(Mass& mass, const Mode& mode, const Measures& measures, const Feel& feel)
{
    const double lost = std::clamp(measures.lost(), 0.0, 1.0);
    m_lost += (lost - m_lost) * (lost > m_lost ? k_look_rise : k_look_fall);

    const double rot = std::clamp(measures.broken, 0.0, 1.0);
    m_broken += (rot - m_broken) * (rot > m_broken ? k_look_rise : k_look_fall);

    const auto shown = static_cast<float>(m_lost);
    const auto broken = static_cast<float>(m_broken);

    mass.march()->march_processor()->set_density_scale(mode.smoke + shown * 1.3F + broken * 4.0F);

    if (mode.fire > 0.0F) {
        auto fire = mass.fire()->march_processor();
        fire->set_density_scale(mode.fire * (1.0F + shown * 0.6F));
        fire->set_threshold(mode.fire_threshold * (1.0F - shown * 0.6F));
        fire->set_emission(mode.fire_emission + broken * 8.0F);
    }

    colour(mass, mode, feel);
}

void Voices::open(Mass& mass, Conduct& conduct)
{
    m_outputs = std::max(1U, Config::get_global_stream_info().output.channels);

    for (size_t i = 0; i < k_voices; ++i) {
        const VoiceProfile& profile = k_profiles.at(i);

        auto stream = get_io_manager()->load_audio_bounded(k_noise.at(i), k_noise_max, true);
        if (!stream) {
            std::cout << "missing " << k_noise.at(i) << "\n";
            continue;
        }

        const uint64_t frames = stream->get_num_frames();
        const auto samples = stream->get_channel_frames(0, 0, frames);
        double energy = 0.0;
        for (const double x : samples) {
            energy += x * x;
        }
        const double level = samples.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(samples.size()));

        Voice& voice = m_voices.at(i);
        voice.trim = level > 1e-6 ? std::clamp(k_target_rms / level, 0.2, 5.0) : 1.0;
        voice.phrase = profile.phrase * 0.5 * static_cast<double>(i + 1) / static_cast<double>(k_voices);

        for (size_t t = 0; t < k_taps; ++t) {
            const auto channel = static_cast<uint32_t>((i + t) % m_outputs);
            const auto& window = k_tap_windows.at(t);
            voice.ratio.at(t) = k_sections.front().ratios.at(t) * profile.pitch;

            Kriya::TapSetBuilder builder(
                stream, *get_buffer_manager(), *get_scheduler(), Config::get_buffer_size());
            builder.loop(true)
                .region(
                    static_cast<uint64_t>(window.first * static_cast<double>(frames)),
                    static_cast<uint64_t>(window.second * static_cast<double>(frames)))
                .tap()
                .speed(voice.ratio.at(t))
                .level(0.0)
                .on_channel(channel);
            if (k_tap_polarity.at(t) < 0.0) {
                builder.backward();
            }

            voice.sets.at(t) = builder.start();
            voice.lanes.at(t) = Lane { .sampler = voice.sets.at(t).sampler(0), .channel = channel };
        }

        voice.influx = mass.volume()->influx(
            "density",
            Buffers::InfluxProcessor::Spot {
                .center = k_voice_anchors.at(i),
                .radius = k_blaze_radius * 0.8F,
                .rate = 0.0F,
                .time_step = k_dt });
    }

    conduct.awake.at(0).store(true, std::memory_order_relaxed);
}

double Voices::meter(Voice& voice) const
{
    double sum = 0.0;
    size_t count = 0;
    for (const Lane& lane : voice.lanes) {
        if (!lane.sampler) {
            continue;
        }
        const auto& block = lane.sampler->get_buffer()->get_data();
        for (const double x : block) {
            sum += x * x;
        }
        count += block.size();
    }
    // The output gain is undone here so loudness, influx and the glow keep their mapping.
    return count > 0 ? std::sqrt(sum / static_cast<double>(count)) / k_output_gain : 0.0;
}

void Voices::turn(Voice& voice, const VoiceProfile& profile, const Feel& feel)
{
    if (m_outputs < 2) {
        return;
    }

    for (size_t t = 0; t < k_taps; ++t) {
        Lane& lane = voice.lanes.at(t);
        lane.channel = (lane.channel + 1U + static_cast<uint32_t>(t)) % m_outputs;
        const double fade = std::clamp(
            profile.fade * (1.0 + 0.6 * static_cast<double>(t)) / std::max(feel.tempo, 0.25), 1.0, 14.0);
        route_buffer(lane.sampler->get_buffer(), lane.channel, fade);
    }
}

void Voices::step(const Measures& measures, const Gesture& pen, Conduct& conduct, Feel& feel)
{
    constexpr auto relaxed = std::memory_order_relaxed;
    constexpr double dt = k_voice_interval;
    const double lost = measures.lost();

    double loud = 0.0;
    for (Voice& voice : m_voices) {
        voice.energy += (meter(voice) - voice.energy) * 0.3;
        loud += voice.energy;
    }
    loud = std::clamp(loud / static_cast<double>(k_voices) * k_loud_gain, 0.0, 1.0);
    feel.loudness += (loud - feel.loudness) * glide(dt, 0.25);

    const double pen_speed = std::clamp(static_cast<double>(pen.speed.load(relaxed)) / k_pen_speed_ref, 0.0, 1.0);
    m_slow += (std::clamp(0.5 + 0.9 * lost + 0.5 * feel.loudness, 0.4, 2.2) - m_slow) * glide(dt, 10.0);
    m_fast += ((1.0 + 1.6 * pen_speed) - m_fast) * glide(dt, 0.4);
    feel.tempo = std::clamp(m_slow * m_fast, k_tempo_min, k_tempo_max);

    m_breath += dt;
    const double cycle = conduct.breathing.load(relaxed)
        ? 0.5 + 0.5 * std::sin(m_breath * 6.28318530718 / k_breath_period)
        : 0.5;
    const double swell = conduct.hushed.load(relaxed)
        ? 0.05
        : std::clamp(static_cast<double>(conduct.pressure.load(relaxed)) * (0.55 + 0.9 * cycle) + lost * 0.35,
              0.0, 1.6);

    const std::array<double, k_voices> measure {
        measures.imbalance,
        measures.skew,
        std::clamp(static_cast<double>(pen.curvature.load(relaxed)) / k_curvature_ref * 2.0 - 1.0, -1.0, 1.0),
        (measures.roughness - 0.5) * 2.0,
        lost * 2.0 - 1.0
    };

    size_t section = 0;
    while (section + 1 < k_sections.size() && lost >= k_sections.at(section).until) {
        ++section;
    }

    const bool frozen = conduct.frozen.load(relaxed);

    for (size_t i = 0; i < k_voices; ++i) {
        Voice& voice = m_voices.at(i);
        const VoiceProfile& profile = k_profiles.at(i);
        if (!voice.lanes.at(0).sampler) {
            continue;
        }

        if (conduct.auto_arrival.load(relaxed) && lost >= profile.arrival) {
            conduct.awake.at(i).store(true, relaxed);
        }

        const double want = conduct.awake.at(i).load(relaxed) ? 1.0 : 0.0;
        voice.gain += (want - voice.gain) * glide(dt, profile.wake);

        if (voice.influx) {
            voice.influx->set_rate(static_cast<float>(voice.energy * voice.gain) * k_voice_influx);
        }

        if (frozen) {
            continue;
        }

        for (size_t t = 0; t < k_taps; ++t) {
            const double goal = k_sections.at(section).ratios.at(t) * profile.pitch;
            voice.ratio.at(t) += (goal - voice.ratio.at(t)) * glide(dt, 12.0);
        }

        const double m = measure.at(i);
        if (m > k_flip_band) {
            voice.pending = 1.0;
        } else if (m < -k_flip_band) {
            voice.pending = -1.0;
        }

        voice.phrase -= dt * k_pace;
        if (voice.phrase <= 0.0) {
            if (voice.pending != voice.target) {
                voice.target = voice.pending;
                turn(voice, profile, feel);
            }
            voice.phrase = profile.phrase * (1.0 - 0.6 * lost);
        }

        voice.direction += (voice.target - voice.direction) * glide(dt, profile.turn);
        voice.clock += dt;

        for (size_t t = 0; t < k_taps; ++t) {
            const double wobble = 1.0
                + 0.012 * std::sin(6.28318530718 * voice.clock / profile.period + static_cast<double>(t));
            const double speed = std::clamp(
                voice.ratio.at(t) * k_tap_polarity.at(t) * feel.tempo * voice.direction * (1.0 + 0.08 * m) * wobble,
                -4.0, 4.0);
            voice.sets.at(t).speed(0, speed);

            const double lvl = std::clamp((swell - 0.3 * static_cast<double>(t)) * 2.0, 0.0, 1.0);
            voice.level.at(t) += (voice.gain * voice.trim * lvl - voice.level.at(t)) * glide(dt, 1.5);
            voice.sets.at(t).level(0, voice.level.at(t) * k_output_gain);
        }
    }
}

} // namespace Attrition
// NOLINTEND
