// NOLINTBEGIN
#include "Grid.hpp"

#include "MayaFlux/SimulationIncludes.hpp"

#include "MayaFlux/Kinesis/Discrete/Boolean.hpp"

namespace Topsoil {

namespace {

    using Grid = Buffers::RelaxationGridBuffer;

    constexpr size_t k_rule_offset = 8;
    constexpr size_t k_phase_offset = 16;
    constexpr size_t k_sun_offset = 20;
    constexpr size_t k_event_offset = 8 + sizeof(LifeRule);

    /** @brief Strength of the relief shading of the cells in the flat picture. */
    constexpr float k_relief = 35.0F;

    /** @brief Speed of the clock that moves the territory borders, in seconds per second. */
    constexpr double k_phase_rate = 1.0;

    constexpr float k_window_aspect = 1920.0F / 1080.0F;

    /** @brief Vertical field of view of the terrain camera. */
    constexpr float k_view_fov = glm::radians(30.0F);

    /** @brief Starting tilt of the terrain, in radians. Scroll changes it. */
    constexpr float k_tilt = glm::radians(17.0F);

    /** @brief Scale of the plane at full lift, so the far edge stays in frame while tilted. */
    constexpr float k_overscan = 1.2F;

    /** @brief Seconds a voice's energy takes to rise toward a louder level. Longer is more gradual. */
    constexpr double k_energy_attack = 2.5;

    /** @brief Seconds a voice's energy takes to fall toward a quieter level. */
    constexpr double k_energy_release = 6.0;

    /** @brief Seconds the Space toggle takes to ease the terrain in or out. */
    constexpr double k_lift_tau = 1.2;

    /** @brief Tilt change per scroll notch, in radians. */
    constexpr float k_tilt_step = glm::radians(2.0F);

    /** @brief Largest tilt, in radians. */
    constexpr float k_tilt_limit = glm::radians(70.0F);

    /** @brief The ecology: see LifeRule for the meaning and order of each value. */
    constexpr LifeRule k_specimen { 1, 240.0F, 0.0F, 0.35F, 0.03F, 0.25F, 0.012F, 0.5F, 0.985F, 0.004F, 0.0002F };

    /**
     * @brief How a part of the score reaches the grid.
     *
     * lag is the base delay in seconds and wander_period the period of its slow variation. radius
     * is the size of the footprint in cells. A positive strength seeds living cells with that
     * chance, a negative strength heaves the ground.
     */
    struct Reach {
        double lag;
        double wander_period;
        float radius;
        float strength;
    };

    /** @brief The reach of each part, in the order sub, tick, gate, noise. */
    constexpr std::array<Reach, 4> k_reach { {
        { 1.4, 37.0, 600.0F, -0.35F },
        { 0.35, 41.0, 35.0F, 1.0F },
        { 2.2, 43.0, 95.0F, 0.55F },
        { 4.0, 47.0, 275.0F, 0.16F },
    } };

    /** @brief How far each lag varies, as a fraction of the lag. */
    constexpr double k_wander = 0.3;

    std::vector<glm::vec4> ground(uint32_t width, uint32_t height, uint32_t seed)
    {
        auto relief = Kinesis::Stochastic::perlin(5, 0.5);
        auto colony = Kinesis::Stochastic::perlin(3, 0.5);
        std::vector<double> field(static_cast<size_t>(width) * height);
        std::vector<double> density(field.size());
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const double u = static_cast<double>(x) / height;
                const double v = static_cast<double>(y) / height;
                const size_t i = static_cast<size_t>(y) * width + x;
                field[i] = relief.at(u * 6.0 + seed * 3.7, v * 6.0 + seed * 1.3);
                density[i] = colony.at(u * 3.0 + seed * 5.1, v * 3.0 + seed * 2.9);
            }
        }
        const auto [flo, fhi] = std::ranges::minmax(field);
        const auto [dlo, dhi] = std::ranges::minmax(density);

        std::vector<glm::vec4> cells(field.size(), glm::vec4(0.0F));
        uint32_t state = seed * 2654435761U + 1U;
        for (size_t i = 0; i < cells.size(); ++i) {
            state = Kinesis::Discrete::xorshift32(state);
            const double d = 0.04 + 0.30 * (density[i] - dlo) / std::max(dhi - dlo, 1e-9);
            const bool alive = (static_cast<double>(state) / 4294967295.0) < d;
            const double h = (field[i] - flo) / std::max(fhi - flo, 1e-9);
            cells[i] = glm::vec4(static_cast<float>(h) * 0.7F, alive ? 1.0F : 0.0F, alive ? 1.0F : 0.0F, 0.0F);
        }
        return cells;
    }

    std::shared_ptr<Grid> build_grid(uint32_t width, uint32_t height, const LifeRule& r, const std::shared_ptr<Core::Window>& window)
    {
        return vega.evolve(
            Grid::GridConfig {
                .width = width,
                .height = height,
                .rule = {
                    .state = Kakshya::GpuDataFormat::VEC4_F32,
                    .shader = "relax_life_rule.comp",
                    .constants = { r.wrap, r.territory, r.phase, r.drift, r.deposit, r.diffuse, r.talus, r.slide, r.trail_decay, r.subside, r.spore,
                        0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F } },
                .emit = { .state = Kakshya::GpuDataFormat::VEC4_F32, .shader = "relax_life_emit.comp", .constants = { k_relief, 0.8F, 0.5F } },
                .extent = k_grid_extent,
                .point_size = k_grid_point,
            },
            [](uint32_t, uint32_t) -> glm::vec4 { return glm::vec4(0.0F); },
            { .target_window = window, .vertex_shader = "topsoil_terrain.vert" });
    }

}

Specimen::Specimen(std::shared_ptr<Cues> cues)
    : m_cues(std::move(cues))
{
}

std::array<float, 4> Specimen::record(const Hit& hit) const
{
    const size_t part = static_cast<size_t>(hit.part);
    const uint64_t per_bar = k_bar_steps / division(hit.part);
    const size_t bar = static_cast<size_t>(hit.cell / per_bar % k_phrase_bars);
    const Strand* strand = m_cues->strand(hit.part, bar);

    const double key = strand ? static_cast<double>(strand->rule) : static_cast<double>(part + 1) * 0.25;
    const double across = std::fmod(static_cast<double>(hit.cell % per_bar) * 0.6180339887 + static_cast<double>(bar) * 0.2137 + key * 0.0113, 1.0);
    const double scatter = std::fmod(static_cast<double>(hit.cell) * 0.7548776662, 1.0) - 0.5;
    const double down = std::fmod(key * 0.6180339887 + static_cast<double>(bar) * 0.0371 + static_cast<double>(part) * 0.13 + scatter * 0.3 + 1.0, 1.0);

    return {
        static_cast<float>(across * k_grid_width),
        static_cast<float>(down * k_grid_height),
        k_reach[part].radius,
        k_reach[part].strength * static_cast<float>(hit.level)
    };
}

bool Specimen::step()
{
    if (++m_cycle % 2U != 0U) {
        return false;
    }

    for (const Hit& hit : m_cues->advance()) {
        const Reach& reach = k_reach[static_cast<size_t>(hit.part)];
        const double lag = reach.lag * (1.0 + k_wander * std::sin(2.0 * std::numbers::pi * hit.seconds / reach.wander_period));
        m_pending.push_back({ hit.seconds + lag, record(hit) });
    }
    std::ranges::stable_sort(m_pending, {}, &Pending::due);

    std::array<float, 4 * k_event_slots> events {};
    const double now = m_cues->seconds();
    size_t used = 0;
    while (used < k_event_slots && !m_pending.empty() && m_pending.front().due <= now) {
        std::ranges::copy(m_pending.front().record, events.begin() + static_cast<std::ptrdiff_t>(used * 4));
        m_pending.erase(m_pending.begin());
        ++used;
    }

    std::memcpy(m_grid->step_processor()->get_push_constant_data().data() + k_event_offset, events.data(), sizeof(events));
    return true;
}

void Specimen::build(const DroneBank& drone)
{
    auto window = create_window({ .title = "Topsoil specimen - MayaFlux", .width = 3840, .height = 2160 });
    window->show();

    m_grid = build_grid(k_grid_width, k_grid_height, k_specimen, window);
    if (!m_grid) {
        return;
    }

    const auto seed = ground(k_grid_width, k_grid_height, 1U);
    m_grid->seed_state(seed.data(), m_grid->get_state_bytes());

    auto render = m_grid->get_render_processor();

    render->set_view_transform(
        Kinesis::look_at_perspective(
            glm::vec3(0.0F, 0.0F, 1.0F / std::tan(k_view_fov * 0.5F)), glm::vec3(0.0F),
            k_view_fov, k_window_aspect, 0.1F, 50.0F),
        true, Portal::Graphics::CullMode::NONE);
    auto tilt = std::make_shared<float>(k_tilt);
    auto active = std::make_shared<bool>(false);
    auto lift = std::make_shared<float>(0.0F);
    render->set_geometry_transform_source([tilt, lift] {
        const float scale = std::lerp(1.0F, k_overscan, *lift);
        return glm::rotate(glm::mat4(1.0F), -*tilt * *lift, glm::vec3(1.0F, 0.0F, 0.0F))
            * glm::scale(glm::mat4(1.0F), glm::vec3(k_window_aspect * scale, scale, 1.0F));
    });

    on_scroll(window, [tilt](double, double dy) {
        *tilt = std::clamp(*tilt + static_cast<float>(dy) * k_tilt_step, 0.0F, k_tilt_limit);
    });

    on_key_pressed(window, IO::Keys::Space, [active] { *active = !*active; });

    render->feed(
        "lift",
        [active, lift, last = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now())] {
            const auto now = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(now - *last).count();
            *last = now;
            *lift += ((*active ? 1.0F : 0.0F) - *lift) * static_cast<float>(1.0 - std::exp(-dt / k_lift_tau));
            return static_cast<double>(*lift);
        },
        static_cast<uint32_t>(sizeof(float) * 6), sizeof(float));

    render->feed(
        "time",
        [origin = std::chrono::steady_clock::now()] {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - origin).count();
        },
        0, sizeof(float));

    for (size_t voice = 0; voice < k_drone_voices.size(); ++voice) {
        const std::string name = "drone_" + std::to_string(voice);
        render->add_binding(name, Buffers::ShaderBinding(1, static_cast<uint32_t>(voice), Portal::Graphics::DescriptorRole::STORAGE));
        render->feed(name, [buffer = drone.buffer(voice, 0)]() -> Buffers::ShaderProcessor::FeedValue {
            const auto& samples = buffer->get_data();
            return Kakshya::DataVariant { std::vector<float>(samples.begin(), samples.end()) };
        });

        render->feed(
            "energy_" + std::to_string(voice),
            [lift, buffer = drone.buffer(voice, 0), level = std::make_shared<double>(0.0), last = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now())] {
                const auto& samples = buffer->get_data();
                double sum = 0.0;
                for (const double sample : samples) {
                    sum += sample * sample;
                }
                const double rms = samples.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(samples.size()));

                const auto now = std::chrono::steady_clock::now();
                const double dt = std::chrono::duration<double>(now - *last).count();
                *last = now;

                const double tau = rms > *level ? k_energy_attack : k_energy_release;
                *level += (rms - *level) * (1.0 - std::exp(-dt / tau));
                return *level * static_cast<double>(*lift);
            },
            static_cast<uint32_t>(sizeof(float) * (1 + voice)), sizeof(float));
    }

    m_grid->step_processor()->set_step_predicate([this] { return step(); });

    m_grid->step_processor()->feed(
        "phase",
        [origin = std::chrono::steady_clock::now()] {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - origin).count() * k_phase_rate;
        },
        k_phase_offset, sizeof(float));

    schedule_metro(
        0.02,
        [grid = m_grid, origin = std::chrono::steady_clock::now()] {
            const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - origin).count();
            const float sun[2] = { std::cos(t * 0.05F), std::sin(t * 0.05F) };
            std::memcpy(grid->emit_processor()->get_push_constant_data().data() + k_sun_offset, sun, sizeof(sun));
        },
        "topsoil_grid_sun", Vruta::ProcessingToken::FRAME_ACCURATE);
}

} // namespace Topsoil
// NOLINTEND
