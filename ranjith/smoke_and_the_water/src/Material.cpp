// NOLINTBEGIN
#include "Material.hpp"

namespace Attrition {

bool Hand::bind()
{
    const auto tools = get_tablet_devices();
    if (tools.empty()) {
        std::cout << "no tablet\n";
        return false;
    }

    using Model = Kinesis::Stochastic::EstimateModel;

    m_pos = std::make_unique<Kinesis::MotionChannel<glm::vec2>>(Model::QUIET_PERIOD_FLOOR, 8, 0.05, 8);
    m_pos->filter_order(2, 8, 3.5);
    m_pos->enable_trajectory_2d(
        Kinesis::Lattice2D {
            .resolution = glm::uvec2(k_cells_x, k_cells_y),
            .bounds = { .min = glm::vec2(0.0F), .max = glm::vec2(1.0F) } },
        k_trajectory_window);

    m_press = std::make_unique<Kinesis::MotionChannel<double>>(Model::QUIET_PERIOD_FLOOR, 6, 0.05, 8);
    m_press->filter_order(2, 6, 3.5);

    m_tilt = std::make_unique<Kinesis::MotionChannel<glm::vec2>>(Model::QUIET_PERIOD_FLOOR, 6, 0.05, 8);
    m_tilt->filter_order(1, 6, 3.0);

    m_hover = std::make_unique<Kinesis::MotionChannel<double>>(Model::QUIET_PERIOD_FLOOR, 6, 0.05, 8);
    m_rotation = std::make_unique<Kinesis::MotionChannel<double>>(Model::QUIET_PERIOD_FLOOR, 6, 0.05, 8);

    m_last = std::chrono::steady_clock::now();

    m_node = vega.read_tablet(
        Nodes::Input::TabletConfig::pressure(), bind_tablet(tools.front().id));

    m_node->on_frame([this](Nodes::Input::TabletContext& ctx) { frame(ctx); });
    return true;
}

void Hand::collect(uint64_t& cursor, std::vector<Impulse>& out) const
{
    const uint64_t head = written();
    if (head == cursor) {
        return;
    }

    const uint64_t first = (head - cursor > k_queue) ? head - k_queue : cursor;
    cursor = head;

    for (uint64_t i = first; i < head; ++i) {
        const Impulse& q = m_queue.at(i % k_queue);
        if (q.force > 0.0F) {
            out.push_back(q);
        }
    }
}

void Hand::wrap()
{
    const auto finite = [](float v) { return std::isfinite(v) ? v : 0.0F; };
    constexpr auto relaxed = std::memory_order_relaxed;

    const glm::vec2 vel = m_pos->derivative<1>();
    const glm::vec2 accel = m_pos->derivative<2>();
    auto& track = m_pos->history();

    m_gesture.speed.store(finite(glm::length(vel)), relaxed);
    m_gesture.acceleration.store(finite(glm::length(accel)), relaxed);
    m_gesture.curvature.store(finite(Kinesis::channel_curvature(*m_pos)), relaxed);
    m_gesture.heading.store(finite(Kinesis::heading(vel)), relaxed);
    m_gesture.straightness.store(finite(Kinesis::straightness(track, k_gesture_window)), relaxed);
    m_gesture.turning.store(finite(Kinesis::total_turning(track, k_gesture_window)), relaxed);
    m_gesture.spread.store(finite(Kinesis::spread_radius(track, k_gesture_window)), relaxed);
    m_gesture.path.store(finite(Kinesis::path_length(track, k_gesture_window)), relaxed);

    m_gesture.press_rate.store(finite(static_cast<float>(m_press->derivative<1>())), relaxed);
    m_gesture.press_accel.store(finite(static_cast<float>(m_press->derivative<2>())), relaxed);
    m_gesture.hover_rate.store(finite(static_cast<float>(m_hover->derivative<1>())), relaxed);
    m_gesture.rotation.store(finite(static_cast<float>(m_rotation->history().newest())), relaxed);
    m_gesture.pressure.store(finite(static_cast<float>(m_press->history().newest())), relaxed);

    const glm::vec2 lean = m_tilt->history().newest() / 60.0F;
    m_gesture.lean_x.store(finite(lean.x), relaxed);
    m_gesture.lean_y.store(finite(lean.y), relaxed);

    if (auto* trajectory = m_pos->trajectory_2d()) {
        const glm::uvec2 at = trajectory->current_cell();
        m_gesture.cell.store(at.y * k_cells_x + at.x, relaxed);
        m_gesture.crossings.store(static_cast<uint32_t>(trajectory->crossings_in_window(k_trajectory_window)), relaxed);
        m_gesture.unique_cells.store(static_cast<uint32_t>(trajectory->unique_cells_in_window(k_trajectory_window)), relaxed);
    }
}

void Hand::frame(Nodes::Input::TabletContext& ctx)
{
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - m_last).count();
    m_last = now;

    const glm::vec2 raw {
        static_cast<float>(ctx.slot(Core::TabletFrame::X)),
        static_cast<float>(ctx.slot(Core::TabletFrame::Y))
    };

    m_pos->update(raw, dt);
    m_press->update(ctx.slot(Core::TabletFrame::PRESSURE), dt);
    m_tilt->update(
        glm::vec2(static_cast<float>(ctx.slot(Core::TabletFrame::TILT_X)),
            static_cast<float>(ctx.slot(Core::TabletFrame::TILT_Y))),
        dt);
    m_hover->update(ctx.slot(Core::TabletFrame::DISTANCE), dt);
    m_rotation->update(ctx.slot(Core::TabletFrame::ROTATION), dt);
    wrap();

    const double p = ctx.slot(Core::TabletFrame::PRESSURE);
    const bool touching = ctx.in_contact() || p > 0.02;

    const double hover = ctx.slot(Core::TabletFrame::DISTANCE);
    if (!touching && hover > 0.001) {
        m_depth.store(std::clamp(1.0F - static_cast<float>(hover), 0.1F, 0.9F), std::memory_order_relaxed);
    }

    const glm::vec2 clean = m_pos->history().newest();
    const glm::vec3 tip {
        (glm::clamp(clean.x, 0.0F, 1.0F) * 2.0F - 1.0F) * k_half_x,
        (1.0F - glm::clamp(clean.y, 0.0F, 1.0F)) * 2.0F * k_half - k_half,
        (m_depth.load(std::memory_order_relaxed) * 2.0F - 1.0F) * k_half
    };

    const glm::vec2 vel = m_pos->derivative<1>();
    const float speed = glm::length(vel);
    const glm::vec2 lean = m_tilt->history().newest() / 60.0F;

    const glm::vec3 dir {
        (speed > 1e-4F ? vel.x / speed : 0.0F) + lean.x,
        (speed > 1e-4F ? -vel.y / speed : 0.0F) + lean.y,
        0.35F
    };
    const glm::vec3 axis = glm::normalize(dir + glm::vec3(1e-5F));

    const auto pressed = static_cast<float>(m_press->history().newest());
    const float bite = pressed * pressed;
    const float accent = glm::clamp(
        m_gesture.press_accel.load(std::memory_order_relaxed) / k_accent_ref, 0.0F, 1.0F);
    const float force = touching
        ? (0.08F + bite) * (1.0F + speed * k_speed_share) * (1.0F + k_accent_gain * accent)
        : 0.0F;

    const float wide = ctx.button(0) ? k_broad_factor : 1.0F;
    const float curl = glm::clamp(
        m_gesture.curvature.load(std::memory_order_relaxed) / k_curvature_ref, 0.0F, 1.0F);
    const float reach = k_ring_radius * wide * (1.0F + speed * 0.5F) * (1.0F - k_curl_tighten * curl);

    if (!touching) {
        return;
    }

    const uint64_t slot = m_written.load(std::memory_order_relaxed);
    m_queue.at(slot % k_queue) = Impulse { .tip = tip, .axis = axis, .force = force, .reach = reach };
    m_written.store(slot + 1, std::memory_order_release);
}

void Mass::build(const std::shared_ptr<Core::Window>& window)
{
    const Kinesis::Lattice3D lattice {
        .resolution = glm::uvec3(k_sim_x, k_sim, k_sim),
        .bounds = { .min = glm::vec3(-k_half_x, -k_half, -k_half), .max = glm::vec3(k_half_x, k_half, k_half) }
    };

    m_volume = std::make_shared<Buffers::VolumeGridBuffer>(
                   lattice,
                   std::vector<Buffers::VolumeGridBuffer::FieldDecl> {
                       { "velocity", sizeof(glm::vec4), true,
                           { .variance = Kinesis::VectorVariance::ContravariantRelative } },
                       { "velocity_prev", sizeof(glm::vec4), false,
                           { .variance = Kinesis::VectorVariance::ContravariantRelative } },
                       { "density", sizeof(float), true,
                           { .value_class = Kinesis::LatticeValueClass::FogVolume } },
                       { "temperature", sizeof(float), true,
                           { .value_class = Kinesis::LatticeValueClass::FogVolume } },
                       { "divergence", sizeof(float), false },
                       { "pressure", sizeof(float), true } })
        | Graphics;

    m_volume->seed("velocity", Kinesis::VectorField { [](glm::vec3) { return glm::vec3(0.0F); } });
    m_volume->seed("pressure", Kinesis::SpatialField { [](glm::vec3) { return 0.0F; } });

    m_blaze = std::make_shared<Buffers::InfluxProcessor>("temperature", "volume_influx_blob.comp");
    m_blaze->set_radius(k_blaze_radius);
    m_blaze->set_rate(0.0F);
    m_blaze->set_time_step(k_dt);
    m_volume->get_processing_chain()->add_processor(m_blaze, m_volume);

    m_stages = m_volume->setup_flow({
        .velocity = "velocity",
        .divergence = "divergence",
        .pressure = "pressure",
        .scratch = "velocity_prev",
        .time_step = k_dt,
        .viscosity = 0.00004F,
        .jacobi_iterations = k_jacobi,
        .walls = true,
        .carried = {
            Buffers::VolumeGridBuffer::FlowConfig::Carried {
                .field = "density", .dissipation = 1.0F },
            Buffers::VolumeGridBuffer::FlowConfig::Carried {
                .field = "temperature", .dissipation = 0.9955F },
        },
        .buoyancy = Buffers::VolumeGridBuffer::FlowConfig::Buoyancy {
            .temperature = "temperature",
            .density = "density",
            .direction = glm::vec3(0.0F, 1.0F, 0.0F),
            .temperature_gain = 2.2F,
            .density_gain = 0.5F,
            .ambient = 0.0F,
        },
    });

    const auto view = [] {
        return Kinesis::look_at_perspective(
            glm::vec3(0.0F, 0.0F, k_eye_distance), glm::vec3(0.0F),
            1.05F, k_aspect, 0.05F, 30.0F);
    };

    m_fire = std::make_shared<Buffers::RaymarchBuffer>(
                 lattice,
                 [this] { return m_volume->read_handle("temperature"); },
                 m_volume->get_field_bytes("temperature"))
        | Graphics;

    m_fire->setup_rendering({ .target_window = window, .fragment_shader = "attrition_march.frag.spv" });
    m_fire->get_render_processor()->set_view_transform_source(
        view, false, Portal::Graphics::CullMode::FRONT);

    Portal::Graphics::BlendAttachmentConfig additive;
    additive.blend_enable = true;
    additive.src_color_factor = Portal::Graphics::BlendFactor::SRC_ALPHA;
    additive.dst_color_factor = Portal::Graphics::BlendFactor::ONE;
    additive.src_alpha_factor = Portal::Graphics::BlendFactor::ZERO;
    additive.dst_alpha_factor = Portal::Graphics::BlendFactor::ONE;
    m_fire->get_render_processor()->set_blend_attachment(additive);

    auto fire = m_fire->march_processor();
    fire->set_max_steps(320);
    fire->set_step_scale(0.5F);

    m_march = std::make_shared<Buffers::RaymarchBuffer>(
                  lattice,
                  [this] { return m_volume->read_handle("density"); },
                  m_volume->get_field_bytes("density"))
        | Graphics;

    m_march->setup_rendering({ .target_window = window, .fragment_shader = "attrition_march.frag.spv" });
    m_march->get_render_processor()->set_view_transform_source(view);

    auto smoke = m_march->march_processor();
    smoke->set_max_steps(380);
    smoke->set_step_scale(0.42F);
    smoke->set_density_scale(1.15F);
}

void Mass::apply(const Mode& mode)
{
    m_volume->seed("density", mode.density);
    m_volume->seed("velocity", mode.velocity);
    m_volume->seed("temperature", mode.temperature);
    m_volume->seed("pressure", Kinesis::SpatialField { [](glm::vec3) { return 0.0F; } });
    m_blaze->set_rate(mode.blaze_gated ? 0.0F : mode.blaze_rate);

    auto fire = m_fire->march_processor();
    fire->set_density_scale(mode.fire);
    fire->set_emission(mode.fire > 0.0F ? mode.fire_emission : 4.4F);
    fire->set_absorption(mode.fire_absorption);
    fire->set_threshold(mode.fire_threshold);
    fire->set_emission_ramp(mode.fire_cool, mode.fire_hot);

    auto smoke = m_march->march_processor();
    smoke->set_absorption(mode.smoke_absorption);
    smoke->set_threshold(mode.smoke_threshold);
    smoke->set_density_scale(mode.smoke);

    if (m_stages.diffuse) {
        m_stages.diffuse->set_rate(mode.viscosity);
    }
    if (m_stages.buoyancy) {
        m_stages.buoyancy->set_temperature_gain(mode.buoyancy_temperature);
        m_stages.buoyancy->set_density_gain(mode.buoyancy_density);
        m_stages.buoyancy->set_direction(glm::normalize(mode.gravity));
    }
    if (m_stages.carriers.size() > 1) {
        m_stages.carriers.at(1)->set_dissipation(mode.temperature_dissipation);
    }
    m_tilt = 0.0;
}

void Mass::step_down(size_t rung)
{
    auto chain = m_volume->get_processing_chain();

    switch (rung) {
    case 0:
        m_stages.pressure->set_iteration_count(k_jacobi / 2);
        break;
    case 1:
        m_stages.pressure->set_iteration_count(k_jacobi / 4);
        break;
    case 2:
        if (m_stages.self_advect) {
            m_stages.self_advect->set_time_step(k_dt * 2.4F);
        }
        break;
    case 3:
        m_stages.pressure->set_iteration_count(2);
        break;
    case 4:
        if (m_stages.wall_advected) {
            chain->remove_processor(m_stages.wall_advected, m_volume);
        }
        break;
    case 5:
        if (m_stages.wall_projected) {
            chain->remove_processor(m_stages.wall_projected, m_volume);
        }
        break;
    case 6:
        m_stages.pressure->set_iteration_count(1);
        break;
    case 7:
        chain->remove_processor(m_stages.solenoidal, m_volume);
        break;
    case 8:
        for (auto& carrier : m_stages.carriers) {
            carrier->set_time_step(k_dt * 5.0F);
        }
        break;
    default:
        break;
    }
}

void Mass::mend(size_t rung)
{
    auto chain = m_volume->get_processing_chain();

    if (rung > 5) {
        chain->add_processor(m_stages.solenoidal, m_volume);
    }
    if (rung > 2 && m_stages.wall_projected) {
        chain->add_processor(m_stages.wall_projected, m_volume);
    }
    if (rung > 0 && m_stages.wall_advected) {
        chain->add_processor(m_stages.wall_advected, m_volume);
    }

    m_stages.pressure->set_iteration_count(k_jacobi);
    if (m_stages.self_advect) {
        m_stages.self_advect->set_time_step(k_dt);
    }
    for (auto& carrier : m_stages.carriers) {
        carrier->set_time_step(k_dt);
    }
}

void Mass::turn(const Mode& mode)
{
    if (mode.spin == 0.0F || !m_stages.buoyancy) {
        return;
    }

    m_tilt += static_cast<double>(mode.spin) * k_look_interval;
    const auto a = static_cast<float>(m_tilt);
    m_stages.buoyancy->set_direction(glm::normalize(
        glm::vec3(std::sin(a), std::cos(a), 0.35F * std::sin(a * 0.37F))));
}

void Stirrer::reset(const Hand& hand)
{
    m_echoes.clear();
    m_cursor = hand.written();
    m_answer = 0.0;
}

void Stirrer::inject(Mass& mass, const Hand& hand, const Mode& mode, const Impulse& last) const
{
    mass.blaze()->set_center(last.tip);
    mass.blaze()->set_radius(k_blaze_radius * last.reach / k_ring_radius);

    if (!mode.blaze_gated) {
        return;
    }

    const Gesture& pen = hand.gesture();
    const float pressure = glm::clamp(pen.pressure.load(std::memory_order_relaxed), 0.0F, 1.0F);
    const glm::vec2 lean {
        pen.lean_x.load(std::memory_order_relaxed),
        pen.lean_y.load(std::memory_order_relaxed)
    };
    const float leaning = glm::clamp(glm::length(lean), 0.0F, 1.0F);
    const float activity = glm::clamp(
        static_cast<float>(pen.crossings.load(std::memory_order_relaxed)) / k_inject_crossings, 0.0F, 1.0F);
    const float spread = glm::mix(k_inject_wide, k_inject_tight, activity);

    mass.blaze()->set_center(last.tip + glm::vec3(lean.x, lean.y, 0.0F) * k_inject_throw);
    mass.blaze()->set_radius(k_blaze_radius * spread);
    mass.blaze()->set_rate(mode.blaze_rate * std::min(1.0F, pressure * (0.6F + 0.8F * leaning)));
}

void Stirrer::apply(const Mass& mass, const Impulse& q, float strength)
{
    const Kinesis::Lattice3D& lat = mass.lattice();
    const glm::uvec3 res = lat.resolution;
    const glm::vec3 cell = lat.cell_size();

    const float sigma2 = q.reach * q.reach;
    const float span = q.reach * 2.3F;

    const glm::vec3 lo = glm::floor((q.tip - glm::vec3(span) - lat.bounds.min) / cell);
    const glm::vec3 hi = glm::ceil((q.tip + glm::vec3(span) - lat.bounds.min) / cell);

    const glm::uvec3 a = glm::uvec3(glm::max(lo, glm::vec3(0.0F)));
    const glm::uvec3 b = glm::uvec3(glm::min(hi, glm::vec3(res) - 1.0F));

    if (a.x > b.x || a.y > b.y || a.z > b.z) {
        return;
    }

    for (uint32_t z = a.z; z <= b.z; ++z) {
        for (uint32_t y = a.y; y <= b.y; ++y) {
            for (uint32_t x = a.x; x <= b.x; ++x) {
                const glm::vec3 r = lat.cell_center({ x, y, z }) - q.tip;
                const float d2 = glm::dot(r, r);
                if (d2 > sigma2 * 5.0F) {
                    continue;
                }

                const float fall = std::exp(-d2 / sigma2);
                const glm::vec3 radial = r - q.axis * glm::dot(r, q.axis);
                const float rl = glm::length(radial);

                const glm::vec3 swirl = (rl > 1e-5F)
                    ? glm::cross(q.axis, radial / rl) * k_swirl_share
                    : glm::vec3(0.0F);

                const glm::vec3 add = (swirl + q.axis * k_jet_share) * fall * q.force * strength;

                glm::vec4& v = m_flow[lat.index({ x, y, z })];
                v.x += add.x;
                v.y += add.y;
                v.z += add.z;
            }
        }
    }
}

void Stirrer::step(Mass& mass, const Hand& hand, const Mode& mode, const Measures& measures, const Feel& feel)
{
    const Kinesis::Lattice3D& lat = mass.lattice();
    if (m_flow.size() != lat.cell_count()) {
        m_flow.assign(lat.cell_count(), glm::vec4(0.0F));
    }

    std::vector<Impulse> pending;
    hand.collect(m_cursor, pending);

    ++m_stirred;

    const bool answering = !m_echoes.empty() && m_echoes.front().due <= m_stirred;
    if (pending.empty() && !answering) {
        if (mode.blaze_gated) {
            mass.blaze()->set_rate(0.0F);
        }
        return;
    }

    std::array<Impulse, k_max_impulses> batch {};
    size_t count = 0;

    if (pending.size() <= k_max_impulses) {
        for (const Impulse& q : pending) {
            batch.at(count++) = q;
        }
    } else {
        const float weight = static_cast<float>(pending.size()) / static_cast<float>(k_max_impulses);
        for (size_t k = 0; k < k_max_impulses; ++k) {
            Impulse merged = pending.at(
                std::min(static_cast<size_t>((static_cast<float>(k) + 0.5F) * weight), pending.size() - 1));
            merged.force *= weight;
            batch.at(count++) = merged;
        }
    }

    for (size_t k = 0; k < count; ++k) {
        m_echoes.push_back(Echo { .due = m_stirred + k_unwind_delay, .impulse = batch.at(k) });
    }

    const auto onset = static_cast<float>(std::clamp(measures.lost() / k_unwind_onset, 0.0, 1.0));
    const double wanted = static_cast<double>(k_unwind * onset * onset * (3.0F - 2.0F * onset));
    m_answer += (wanted - m_answer) * 0.08;
    const auto answer = static_cast<float>(m_answer);

    std::array<Impulse, k_max_impulses> reply {};
    size_t replies = 0;
    while (!m_echoes.empty() && m_echoes.front().due <= m_stirred && replies < k_max_impulses) {
        Impulse q = m_echoes.front().impulse;
        m_echoes.pop_front();
        q.axis = -q.axis;
        q.force *= answer;
        if (q.force > 1e-3F) {
            reply.at(replies++) = q;
        }
    }

    if (mode.blaze_rate > 0.0F && count > 0) {
        inject(mass, hand, mode, batch.at(count - 1));
    }

    const float tempo = static_cast<float>(std::clamp(feel.tempo, 0.5, 2.0));
    const float base = k_ring_strength * mode.ring_scale * tempo;
    const float share = base / std::sqrt(static_cast<float>(std::max<size_t>(count, 1)));
    const float reply_share = base / std::sqrt(static_cast<float>(std::max<size_t>(replies, 1)));

    mass.volume()->read_field("velocity", m_flow.data(), m_flow.size() * sizeof(glm::vec4));

    for (size_t k = 0; k < count; ++k) {
        apply(mass, batch.at(k), share);
    }
    for (size_t k = 0; k < replies; ++k) {
        apply(mass, reply.at(k), reply_share);
    }

    mass.volume()->seed_raw("velocity", m_flow.data(), m_flow.size() * sizeof(glm::vec4));
}

} // namespace Attrition
// NOLINTEND
