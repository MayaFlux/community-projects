// NOLINTBEGIN
#include "Senses.hpp"

#include "MayaFlux/Portal/Forma/Primitives/Picker.hpp"

namespace TinglingTime {

bool Hand::present()
{
    return !get_tablet_devices().empty();
}

void Hand::choose(std::function<void(std::optional<uint32_t>)> chosen)
{
    constexpr std::string_view eraser_suffix = " Eraser";

    std::vector<Core::InputDeviceInfo> pens;
    for (auto& tool : get_tablet_devices()) {
        if (!tool.name.ends_with(eraser_suffix)) {
            pens.push_back(std::move(tool));
        }
    }

    if (pens.empty()) {
        chosen(std::nullopt);
        return;
    }

    Portal::Forma::pick("Choose a tablet", std::move(pens),
        [](const Core::InputDeviceInfo& tool) { return tool.name; },
        [chosen = std::move(chosen)](std::optional<Core::InputDeviceInfo> tool) {
            chosen(tool ? std::optional<uint32_t>(tool->id) : std::nullopt);
        });
}

bool Hand::bind(uint32_t tool_id, Reactions reactions)
{
    using Model = Kinesis::Stochastic::EstimateModel;

    m_reactions = std::move(reactions);

    m_pos = std::make_unique<Kinesis::MotionChannel<glm::vec2>>(Model::QUIET_PERIOD_FLOOR, 8, 0.05, 8);
    m_pos->filter_order(2, 8, 3.5);

    m_press = std::make_unique<Kinesis::MotionChannel<double>>(Model::QUIET_PERIOD_FLOOR, 6, 0.05, 8);
    m_press->filter_order(2, 6, 3.5);

    m_last = std::chrono::steady_clock::now();

    m_node = vega.read_tablet(
        Nodes::Input::TabletConfig::pressure(), bind_tablet(tool_id));

    m_node->on_frame([this](Nodes::Input::TabletContext& ctx) { frame(ctx); });
    return true;
}

void Hand::frame(Nodes::Input::TabletContext& ctx)
{
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - m_last).count();
    m_last = now;

    const auto finite = [](float v) { return std::isfinite(v) ? v : 0.0F; };
    constexpr auto relaxed = std::memory_order_relaxed;

    m_pos->update(
        glm::vec2 {
            static_cast<float>(ctx.slot(Core::TabletFrame::X)),
            static_cast<float>(ctx.slot(Core::TabletFrame::Y)) },
        dt);
    m_press->update(ctx.slot(Core::TabletFrame::PRESSURE), dt);

    const float speed = finite(glm::length(m_pos->derivative<1>()));
    const float pressure = finite(static_cast<float>(m_press->history().newest()));
    const bool touching = ctx.in_contact();

    const double travel = touching ? static_cast<double>(speed) / k_pen_event_rate : 0.0;
    const double press = touching ? std::clamp(static_cast<double>(pressure), 0.0, 1.0) : 0.0;
    const double instant = travel * k_motion_travel_gain + press * k_motion_pressure_gain;
    m_motion += (instant - m_motion) * k_motion_follow;

    m_gesture.speed.store(speed, relaxed);
    m_gesture.pressure.store(pressure, relaxed);
    m_gesture.motion.store(static_cast<float>(m_motion), relaxed);
    m_gesture.touching.store(touching, relaxed);

    if (touching && !m_touching && m_reactions.contact) {
        m_reactions.contact(pressure);
    }

    if (m_reactions.frame) {
        m_reactions.frame();
    }

    if (!touching && m_touching) {
        m_motion = 0.0;
        m_gesture.motion.store(0.0F, relaxed);
    }

    m_touching = touching;
}

namespace {

    constexpr glm::vec4 k_neutral { 0.0F, 0.0F, 3.0F, 0.0F };

} // namespace

bool Eye::open(const std::shared_ptr<Core::Window>& window)
{
    auto builder = create_chimera(
        { .width = k_eye_width, .height = k_eye_height, .ring_frames = k_bodies + 1 },
        { .target_window = window, .fragment_shader = "echo.frag" });

    builder.layer_data();
    auto camera = choose_camera();
    builder.record(Kriya::BufferOperation::capture_to_stream(
        get_io_manager(),
        camera,
        k_eye_ring));

    builder.layer().from_pipeline().lag(0.0).params(k_neutral);
    for (size_t b = 0; b < k_bodies; ++b) {
        builder.layer().from_pipeline().speed(1.0).smooth().level(0.0).params(k_neutral);
    }

    auto shared = m_shared;
    auto levels = std::make_shared<std::array<double, k_bodies>>();

    builder.every(k_eye_step, [shared, levels](Kriya::Chimera& set) {
        constexpr auto relaxed = std::memory_order_relaxed;

        const double moving = std::clamp(static_cast<double>(shared->motion.load(relaxed)) / k_motion_ref, 0.0, 1.0);
        const double ratio = k_lag_speed_still + (k_lag_speed_moving - k_lag_speed_still) * moving;
        const double level_keep = std::exp(-k_eye_step / k_strike_tau);

        for (size_t b = 0; b < k_bodies; ++b) {
            double& level = (*levels)[b];
            const size_t layer = b + 1;

            const double hit = shared->strikes[b].exchange(0.0F, relaxed);
            if (hit > 0.0) {
                set.cut(layer, k_cut_near + (k_cut_far - k_cut_near) * hit);
                level = std::max(level, hit);
                set.params(layer, glm::vec4(static_cast<float>(local_uniform(0.0, 64.0)), static_cast<float>(local_uniform(0.0, 64.0)), static_cast<float>(local_uniform(1.5, 5.0)), 0.0F));
            }

            level *= level_keep;

            set.speed(layer, ratio);
            set.level(layer, level);
        }
    });

    m_chimera = builder.start();
    return m_chimera.layer_count() != 0;
}

void Eye::strike(size_t body, double strength)
{
    if (body < k_bodies) {
        m_shared->strikes[body].store(
            static_cast<float>(std::clamp(strength, 0.0, 1.0)), std::memory_order_relaxed);
    }
}

void Eye::move(double motion)
{
    m_shared->motion.store(static_cast<float>(motion), std::memory_order_relaxed);
}

} // namespace TinglingTime
// NOLINTEND
