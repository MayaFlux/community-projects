// NOLINTBEGIN
#include "Piece.hpp"

namespace Attrition {

namespace {

    Mode shell()
    {
        return Mode {
            .name = "shell",
            .density = Kinesis::SpatialField { [](glm::vec3 p) {
                p *= k_form_inv;
                const float hollow = std::exp(-std::pow(glm::length(p) - 0.75F, 2.0F) / 0.09F);
                const float bands = 0.5F + 0.5F * std::sin(p.y * 9.0F);
                const float lumps = 0.55F + 0.45F * std::sin(p.x * 7.3F) * std::sin(p.z * 6.1F);
                const float hull = std::exp(-glm::dot(p, p) / 1.9F);
                return glm::clamp((hollow * 0.7F + hull * 0.5F) * bands * lumps * 1.5F, 0.0F, 1.0F);
            } },
            .velocity = Kinesis::VectorField { [](glm::vec3) { return glm::vec3(0.0F); } },
            .temperature = Kinesis::SpatialField { [](glm::vec3) { return 0.0F; } },
            .ring_scale = 1.0F,
            .blaze_rate = 0.0F,
            .fire = 0.0F,
            .smoke = 1.15F,
        };
    }

    Mode knot()
    {
        return Mode {
            .name = "knot",
            .density = Kinesis::SpatialField { [](glm::vec3 p) {
                p *= k_form_inv;
                const float tube = std::exp(
                    -std::pow(std::sqrt(p.x * p.x + p.z * p.z) - 0.8F, 2.0F) / 0.045F);
                const float twist = 0.45F
                    + 0.55F * std::sin(std::atan2(p.z, p.x) * 5.0F + p.y * 4.0F);
                const float cap = std::exp(-p.y * p.y / 1.1F);
                return glm::clamp(tube * twist * cap * 1.7F, 0.0F, 1.0F);
            } },
            .velocity = Kinesis::VectorField { [](glm::vec3 p) {
                p *= k_form_inv;
                return glm::vec3(-p.z, 0.0F, p.x) * 0.22F;
            } },
            .temperature = Kinesis::SpatialField { [](glm::vec3) { return 0.0F; } },
            .ring_scale = 0.8F,
            .blaze_rate = 3.5F,
            .fire = 2.0F,
            .smoke = 1.15F,
            .palette = Palette {
                .low = {
                    glm::vec3 { 0.02F, 0.02F, 0.05F },
                    glm::vec3 { 0.08F, 0.03F, 0.08F },
                    glm::vec3 { 0.04F, 0.05F, 0.03F } },
                .high = {
                    glm::vec3 { 0.45F, 0.42F, 0.58F },
                    glm::vec3 { 0.62F, 0.40F, 0.55F },
                    glm::vec3 { 0.50F, 0.52F, 0.36F } } },
            .buoyancy_temperature = 0.0F,
            .buoyancy_density = 0.0F,
            .temperature_dissipation = 0.998F,
            .fire_absorption = 4.0F,
            .fire_emission = 3.2F,
            .fire_threshold = 0.03F,
            .fire_cool = glm::vec3 { 0.02F, 0.22F, 0.04F },
            .fire_hot = glm::vec3 { 0.55F, 1.00F, 0.30F },
            .blaze_gated = true,
        };
    }

    Mode combustion()
    {
        return Mode {
            .name = "combustion",
            .density = Kinesis::SpatialField { [](glm::vec3 p) {
                p *= k_form_inv;
                const float soot = 0.35F
                    + 0.35F * std::sin(p.x * 11.0F) * std::sin(p.y * 9.0F) * std::sin(p.z * 10.0F);
                const float hull = std::exp(-glm::dot(p, p) / 2.6F);
                return glm::clamp(soot * hull * 0.55F, 0.0F, 0.5F);
            } },
            .velocity = Kinesis::VectorField { [](glm::vec3 p) {
                p *= k_form_inv;
                return glm::vec3(
                           std::sin(p.y * 5.1F) * std::cos(p.z * 4.3F),
                           std::sin(p.z * 4.7F) * std::cos(p.x * 5.9F),
                           std::sin(p.x * 4.1F) * std::cos(p.y * 5.3F))
                    * 0.35F;
            } },
            .temperature = Kinesis::SpatialField { [](glm::vec3 p) {
                p *= k_form_inv;
                const float veins = std::pow(
                    std::abs(std::sin(p.x * 6.1F) * std::sin(p.y * 5.3F) * std::sin(p.z * 6.7F)),
                    0.18F);
                const float grain = std::pow(
                    std::abs(std::sin(p.x * 19.0F + p.y * 14.0F)
                        * std::sin(p.z * 17.0F - p.y * 11.0F)),
                    0.4F);
                const float hull = std::exp(-glm::dot(p, p) / 2.2F);
                return glm::clamp(veins * (0.45F + 0.55F * grain) * hull * 1.3F, 0.0F, 1.3F);
            } },
            .ring_scale = 1.9F,
            .blaze_rate = 2.2F,
            .fire = 1.6F,
            .smoke = 0.22F,
            .smoke_absorption = 3.2F,
        };
    }

    Mode liquid()
    {
        return Mode {
            .name = "liquid",
            .density = Kinesis::SpatialField { [](glm::vec3 p) {
                p *= k_form_inv;
                static const std::array<glm::vec4, 6> drops { {
                    { -0.9F, 0.5F, 0.3F, 0.34F },
                    { 0.8F, 0.9F, -0.4F, 0.28F },
                    { 0.1F, 1.3F, 0.6F, 0.22F },
                    { -0.3F, -0.2F, -0.8F, 0.30F },
                    { 1.1F, -0.1F, 0.5F, 0.26F },
                    { -1.2F, 1.0F, -0.7F, 0.20F },
                } };
                const float pool = glm::clamp((-0.5F - p.y) / 0.3F, 0.0F, 1.0F)
                    * std::exp(-(p.x * p.x + p.z * p.z) / 3.0F);
                float beads = 0.0F;
                for (const glm::vec4& c : drops) {
                    const glm::vec3 r = p - glm::vec3(c);
                    beads += std::exp(-glm::dot(r, r) / (c.w * c.w));
                }
                return glm::clamp(pool + beads * 1.2F, 0.0F, 1.0F);
            } },
            .velocity = Kinesis::VectorField { [](glm::vec3 p) {
                p *= k_form_inv;
                return glm::vec3(
                           std::sin(p.z * 2.1F + p.y),
                           std::cos(p.x * 1.7F) * 0.4F,
                           -std::sin(p.x * 2.3F))
                    * 0.14F;
            } },
            .temperature = Kinesis::SpatialField { [](glm::vec3 p) {
                p *= k_form_inv;
                const float sheets = std::pow(
                    std::abs(std::sin(p.y * 5.0F + 0.9F * std::sin(p.x * 2.1F) + 0.6F * std::sin(p.z * 1.7F))),
                    4.0F);
                const float hull = std::exp(-glm::dot(p, p) / 3.4F);
                return glm::clamp(sheets * hull * 1.2F, 0.0F, 1.0F);
            } },
            .ring_scale = 1.1F,
            .blaze_rate = 0.0F,
            .fire = 1.0F,
            .smoke = 1.6F,
            .palette = Palette {
                .low = {
                    glm::vec3 { 0.04F, 0.03F, 0.05F },
                    glm::vec3 { 0.05F, 0.05F, 0.02F },
                    glm::vec3 { 0.02F, 0.04F, 0.05F } },
                .high = {
                    glm::vec3 { 0.55F, 0.65F, 0.80F },
                    glm::vec3 { 0.80F, 0.62F, 0.85F },
                    glm::vec3 { 0.88F, 0.82F, 0.50F } } },
            .emission = 1.2F,
            .viscosity = 0.0008F,
            .buoyancy_temperature = 0.0F,
            .buoyancy_density = 0.9F,
            .gravity = glm::vec3 { 0.0F, 1.0F, 0.0F },
            .spin = 0.35F,
            .temperature_dissipation = 0.9995F,
            .smoke_absorption = -1.0F,
            .smoke_threshold = 0.35F,
            .fire_absorption = -2.0F,
            .fire_emission = 1.4F,
            .fire_threshold = 0.30F,
            .fire_cool = glm::vec3 { 0.02F, 0.14F, 0.16F },
            .fire_hot = glm::vec3 { 0.30F, 0.95F, 0.85F },
        };
    }

} // namespace

std::vector<Mode> make_modes()
{
    std::vector<Mode> modes;
    modes.push_back(shell());
    modes.push_back(knot());
    modes.push_back(combustion());
    modes.push_back(liquid());
    return modes;
}

void Piece::start()
{
    if (!m_hand.bind()) {
        return;
    }

    m_window = MayaFlux::create_window(
        { .title = "Smoke and the Water", .width = k_window_width, .height = k_window_height });
    m_window->show();

    m_modes = make_modes();
    m_mass.build(m_window);
    enter(0);
    m_voices.open(m_mass, m_conduct);

    bind_keys();
    schedule();
}

void Piece::enter(size_t index)
{
    if (index >= m_modes.size()) {
        return;
    }

    m_mode = index;
    const Mode& mode = m_modes.at(index);

    m_mass.mend(m_measures.rung);
    m_mass.apply(mode);
    m_survey.reset(m_measures);
    m_stirrer.reset(m_hand);
    m_look.reset();
    m_look.step(m_mass, mode, m_measures, m_feel);

    std::cout << "mode " << mode.name << "\n";
}

void Piece::schedule()
{
    auto self = shared_from_this();

    schedule_metro(k_stir_interval, [self] {
        self->m_stirrer.step(self->m_mass, self->m_hand, self->m_modes.at(self->m_mode), self->m_measures, self->m_feel);
    },
        "attrition_stir", Vruta::ProcessingToken::FRAME_ACCURATE);

    schedule_metro(k_survey_interval, [self] {
        self->m_survey.step(self->m_mass, self->m_measures);
    },
        "attrition_survey", Vruta::ProcessingToken::FRAME_ACCURATE);

    schedule_metro(k_look_interval, [self] {
        const Mode& mode = self->m_modes.at(self->m_mode);
        self->m_look.step(self->m_mass, mode, self->m_measures, self->m_feel);
        self->m_mass.turn(mode);
    },
        "attrition_look", Vruta::ProcessingToken::FRAME_ACCURATE);

    schedule_metro(k_voice_interval, [self] {
        self->m_voices.step(self->m_measures, self->m_hand.gesture(), self->m_conduct, self->m_feel);
    },
        "attrition_voices", Vruta::ProcessingToken::FRAME_ACCURATE);
}

void Piece::bind_keys()
{
    auto self = shared_from_this();

    on_key_pressed(m_window, IO::Keys::N0, [self] { self->enter(self->m_mode); });
    on_key_pressed(m_window, IO::Keys::N1, [self] { self->enter(0); });
    on_key_pressed(m_window, IO::Keys::N2, [self] { self->enter(1); });
    on_key_pressed(m_window, IO::Keys::N3, [self] { self->enter(2); });
    on_key_pressed(m_window, IO::Keys::N4, [self] { self->enter(3); });

    const std::array<IO::Keys, k_voices> voice_keys {
        IO::Keys::F1, IO::Keys::F2, IO::Keys::F3, IO::Keys::F4, IO::Keys::F5
    };

    for (size_t i = 0; i < k_voices; ++i) {
        on_key_pressed(m_window, voice_keys.at(i), [self, i] {
            const bool now = !self->m_conduct.awake.at(i).load(std::memory_order_relaxed);
            self->m_conduct.awake.at(i).store(now, std::memory_order_relaxed);
            self->m_conduct.auto_arrival.store(false, std::memory_order_relaxed);
            std::cout << "voice " << (i + 1) << (now ? " in\n" : " out\n");
        });
    }

    on_key_pressed(m_window, IO::Keys::F6, [self] {
        const bool now = !self->m_conduct.auto_arrival.load(std::memory_order_relaxed);
        self->m_conduct.auto_arrival.store(now, std::memory_order_relaxed);
        std::cout << (now ? "arrivals automatic\n" : "arrivals by hand\n");
    });

    on_key_pressed(m_window, IO::Keys::Up, [self] {
        self->m_conduct.pressure.store(
            std::min(1.4F, self->m_conduct.pressure.load(std::memory_order_relaxed) + 0.1F),
            std::memory_order_relaxed);
    });

    on_key_pressed(m_window, IO::Keys::Down, [self] {
        self->m_conduct.pressure.store(
            std::max(0.0F, self->m_conduct.pressure.load(std::memory_order_relaxed) - 0.1F),
            std::memory_order_relaxed);
    });

    on_key_pressed(m_window, IO::Keys::H, [self] {
        const bool now = !self->m_conduct.hushed.load(std::memory_order_relaxed);
        self->m_conduct.hushed.store(now, std::memory_order_relaxed);
        std::cout << (now ? "hush\n" : "released\n");
    });

    on_key_pressed(m_window, IO::Keys::B, [self] {
        const bool now = !self->m_conduct.breathing.load(std::memory_order_relaxed);
        self->m_conduct.breathing.store(now, std::memory_order_relaxed);
        std::cout << (now ? "breathing\n" : "held flat\n");
    });

    on_key_pressed(m_window, IO::Keys::F, [self] {
        const bool now = !self->m_conduct.frozen.load(std::memory_order_relaxed);
        self->m_conduct.frozen.store(now, std::memory_order_relaxed);
        std::cout << (now ? "frozen\n" : "thawed\n");
    });

    IO::VolumeWriteOptions opts {
        .field_thresholds = {
            { "density", 1e-3F },
            { "temperature", 1e-3F },
            { "velocity", 1e-4F },
        },
    };

    auto capture_id = make_persistent_shared<uint32_t>(0);

    on_key_pressed(m_window, IO::Keys::Enter, [capture_id, opts, volume = m_mass.volume()] {
        auto io = get_io_manager();

        if (*capture_id != 0) {
            io->stop_volume_capture(*capture_id);
            *capture_id = 0;
            return;
        }

        *capture_id = io->capture_volume(
            volume, "oak/smoke.{:04}.vdb",
            { "density", "temperature", "velocity" },
            opts, 120, 2);
    });
}

void attrition()
{
    auto piece = make_persistent_shared<Piece>();
    piece->start();
}

} // namespace Attrition
// NOLINTEND
