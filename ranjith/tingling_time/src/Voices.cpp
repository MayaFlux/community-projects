// NOLINTBEGIN
#include "Voices.hpp"

namespace TinglingTime {

namespace {

    void couple_chain(const std::shared_ptr<ModalNetwork>& net, size_t mode_count, double adjacent, double skip)
    {
        net->set_coupling_enabled(true);
        for (size_t i = 0; i + 1 < mode_count; ++i) {
            net->set_mode_coupling(i, i + 1, adjacent);
        }
        for (size_t i = 0; i + 2 < mode_count; ++i) {
            net->set_mode_coupling(i, i + 2, skip);
        }
        net->set_mode_coupling(0, mode_count - 1, skip * 1.5);
    }

    void build_drone(Voice& v, uint32_t channel)
    {
        constexpr int k_partials = 9;
        v.drone_partials.reserve(k_partials);
        v.drone_base_freqs.reserve(k_partials);
        v.drone_detune_hz.assign(k_partials, 0.0);
        v.drone_detune_target.assign(k_partials, 0.0);
        v.drone_balance = std::make_shared<std::vector<double>>(k_partials, 1.0);
        v.drone_balance_target.assign(k_partials, 1.0);
        v.root_current = v.root;
        v.root_target = v.root;

        std::vector<std::shared_ptr<Node>> summed;
        summed.reserve(k_partials);

        for (int p = 0; p < k_partials; ++p) {
            const double offset = get_uniform_random(-11.0, 11.0) + static_cast<double>(p) * get_uniform_random(0.3, 2.4);
            const double freq = std::max(200.0, v.root + offset);

            auto partial = std::make_shared<Sine>(static_cast<float>(freq), 1.0);

            v.drone_partials.push_back(partial);
            v.drone_base_freqs.push_back(offset);
            summed.push_back(partial);
        }

        const double scale = 1.0 / std::sqrt(static_cast<double>(k_partials));
        auto balance = v.drone_balance;
        auto composite = std::make_shared<CompositeOpNode<>>(
            summed,
            [scale, balance](std::span<const double> vals) {
                double sum = 0.0;
                for (size_t i = 0; i < vals.size(); ++i) {
                    sum += vals[i] * (i < balance->size() ? (*balance)[i] : 1.0);
                }
                return sum * scale;
            });

        v.drone = composite | Audio[channel];
    }

} // namespace

void Bank::build()
{
    for (size_t b = 0; b < k_bank_size; ++b) {
        Voice v;
        v.temperament = (b % 2 == 0) ? Temperament::CONTINUOUS : Temperament::PERCUSSIVE;
        v.journey_seconds = (24.0 + static_cast<double>(b) * 11.0) / k_pace;
        v.side = b % 2;
        const bool percussive = v.temperament == Temperament::PERCUSSIVE;
        const double pos = get_uniform_random(0.0, 1.0);
        v.root = percussive
            ? exp_map(pos, 400.0, 1400.0)
            : exp_map(pos, 200.0, 500.0);

        if (percussive) {
            const auto spectrum = ModalNetwork::Spectrum::INHARMONIC;
            const double decay = 1.2 + get_uniform_random(0.0, 2.5);
            constexpr size_t voice_modes = 5;
            v.body = m_body_count++;
            v.net = vega.ModalNetwork(voice_modes, v.root, spectrum, decay) | Audio[v.side];
            couple_chain(v.net, voice_modes, 0.30, 0.14);
        } else {
            build_drone(v, v.side);
        }

        v.entry = make_timer();
        m_voices.push_back(std::move(v));
    }
}

double Bank::strike_interval() const
{
    return envelope_lerp(9.0, 3.0, paced_now());
}

double Bank::drift_range_hz() const
{
    return envelope_lerp(0.0, 12.0, paced_now());
}

void Bank::rest(Voice& v, double seconds)
{
    if (!v.refractory) {
        v.refractory = make_timer();
    }
    if (v.refractory) {
        v.refractory->schedule(seconds, []() { });
    }
}

void Bank::enter(size_t index)
{
    auto& v = m_voices.at(index);
    if (v.entered) {
        return;
    }
    v.entered = true;
    ++m_entered;

    if (v.temperament == Temperament::CONTINUOUS) {
        v.swell_seconds = (20.0 + static_cast<double>(index) * 7.0) / k_pace;
        v.swell_level = 0.0;
        for (auto& partial : v.drone_partials) {
            partial->set_amplitude(0.0);
        }
    } else {
        v.refractory = make_timer();
        v.net->excite(0.6);
        struck(v, 0.6);
        rest(v, strike_interval());
    }
}

void Bank::struck(const Voice& v, double strength) const
{
    if (m_struck) {
        m_struck(v.body, strength);
    }
}

void Bank::admit(double seconds)
{
    const size_t next = m_entered;
    if (next >= m_voices.size()) {
        return;
    }
    m_voices[next].entry->schedule(seconds, [this, next]() { enter(next); });
}

void Bank::strike_body(size_t body, double strength)
{
    for (auto& v : m_voices) {
        if (v.net && v.entered && v.body == body) {
            v.net->excite(strength);
            struck(v, strength);
            return;
        }
    }
}

void Bank::pitch(size_t body, double ratio)
{
    for (auto& v : m_voices) {
        if (v.net && v.body == body) {
            v.net->set_fundamental(std::max(200.0, v.root * ratio));
            return;
        }
    }
}

void Bank::turn(bool away, double seconds)
{
    for (auto& v : m_voices) {
        if (!v.net) {
            continue;
        }
        const bool first_pair = v.body % 2 == 0;
        const uint32_t target = (first_pair == away) ? 1U : 0U;
        route_network(v.net, { target }, seconds);
        v.side = target;
    }
}

void Bank::set_mode(PerformMode mode)
{
    m_mode = mode;
    for (auto& v : m_voices) {
        if (v.temperament != Temperament::PERCUSSIVE) {
            continue;
        }
        if (mode == PerformMode::RESONANT || mode == PerformMode::FULL) {
            resonate(v);
        } else {
            release(v);
        }
    }
}

void Bank::resonate(Voice& v)
{
    if (v.temperament != Temperament::PERCUSSIVE || !v.net || v.resonated) {
        return;
    }
    for (auto& source : m_voices) {
        if (source.temperament == Temperament::CONTINUOUS && source.drone) {
            v.net->set_exciter_node(source.drone);
            v.net->set_exciter_type(ModalNetwork::ExciterType::CONTINUOUS);
            v.resonated = true;
            return;
        }
    }
}

void Bank::release(Voice& v)
{
    if (!v.resonated || !v.net) {
        return;
    }
    v.net->set_exciter_type(ModalNetwork::ExciterType::IMPULSE);
    v.resonated = false;
}

void Bank::nudge_register(Voice& v)
{
    if (!v.net) {
        return;
    }
    const double pull = (v.root - v.net->get_fundamental()) * 0.15;
    const double step = local_uniform(-6.0, 6.0);
    const double next = std::max(200.0, v.net->get_fundamental() + pull + step);
    v.net->set_fundamental(next);
}

void Bank::strike(double strength)
{
    const bool bypass_refractory = m_mode == PerformMode::TAP;
    for (size_t n = 0; n < m_voices.size(); ++n) {
        const size_t idx = (m_cursor + n) % m_voices.size();
        auto& v = m_voices[idx];
        if (!v.entered || v.temperament != Temperament::PERCUSSIVE) {
            continue;
        }
        const bool available = bypass_refractory || !v.refractory || !v.refractory->is_active();
        if (available) {
            v.net->excite(strength);
            struck(v, strength);
            rest(v, strike_interval());
            m_cursor = (idx + 1) % m_voices.size();
            if (m_mode == PerformMode::TAP) {
                nudge_register(v);
            }
            return;
        }
    }
}

void Bank::force_strike(double strength)
{
    for (size_t n = 0; n < m_voices.size(); ++n) {
        const size_t idx = (m_cursor + n) % m_voices.size();
        auto& v = m_voices[idx];
        if (!v.entered || v.temperament != Temperament::PERCUSSIVE) {
            continue;
        }
        v.net->excite(strength);
        struck(v, strength);
        rest(v, strike_interval());
        m_cursor = (idx + 1) % m_voices.size();
        if (m_mode == PerformMode::TAP) {
            nudge_register(v);
        }
        return;
    }
}

void Bank::burst_voice(double strength, double degrade)
{
    std::vector<size_t> entered;
    for (size_t i = 0; i < m_voices.size(); ++i) {
        if (m_voices[i].entered && m_voices[i].temperament == Temperament::PERCUSSIVE) {
            entered.push_back(i);
        }
    }
    if (entered.empty()) {
        return;
    }
    const size_t pick = entered[static_cast<size_t>(local_uniform(0.0, static_cast<double>(entered.size())))];
    auto& v = m_voices[pick];
    v.net->set_fundamental(std::max(200.0, v.root * degrade));
    v.net->excite(strength * degrade);
    struck(v, strength * degrade);

    if (!v.refractory) {
        v.refractory = make_timer();
    }
    if (v.refractory) {
        v.refractory->schedule(1.5, [this, pick]() {
            auto& target = m_voices[pick];
            if (target.net) {
                target.net->set_fundamental(target.root);
            }
        });
    }
}

void Bank::burst()
{
    const double t = paced_now();
    const int count = static_cast<int>(envelope_lerp(2.0, 15.0, t));
    const double span = envelope_lerp(18.0, 7.0, t);
    const int max_voices = static_cast<int>(envelope_lerp(1.0, 3.0, t));

    std::vector<std::pair<double, std::function<void()>>> events;
    events.reserve(static_cast<size_t>(count));

    for (int n = 0; n < count; ++n) {
        const double gap = local_uniform(0.2, span / count * 2.0);
        const int voices = static_cast<int>(local_uniform(1.0, static_cast<double>(max_voices) + 1.0));
        const double degrade = std::max(0.4, 1.0 - static_cast<double>(n) / static_cast<double>(count) * 0.7);
        events.emplace_back(gap, [this, voices, degrade]() {
            for (int k = 0; k < voices; ++k) {
                burst_voice(local_uniform(0.4, 1.0), degrade);
            }
        });
    }

    if (const auto scheduler = get_scheduler()) {
        scheduler->add_task(Kriya::sequence(std::move(events)), "", true);
    }
}

void Bank::drive(double motion)
{
    if (m_mode == PerformMode::TAP) {
        return;
    }
    const double influence = (m_mode == PerformMode::RESONANT || m_mode == PerformMode::FULL) ? 4.5 : 3.4;
    const double level = std::min(1.0, 0.55 + motion * influence);
    for (auto& v : m_voices) {
        if (v.entered && v.temperament == Temperament::CONTINUOUS) {
            for (auto& partial : v.drone_partials) {
                partial->set_amplitude(level);
            }
        }
    }
}

void Bank::accent(bool recent)
{
    const double level = recent ? 1.0 : 0.75;
    for (auto& v : m_voices) {
        if (v.entered && v.temperament == Temperament::CONTINUOUS) {
            for (auto& partial : v.drone_partials) {
                partial->set_amplitude(level);
            }
        }
    }
}

void Bank::route_leg(size_t index)
{
    auto& v = m_voices[index];
    const uint32_t target = 1U - v.side;
    if (v.net) {
        route_network(v.net, { target }, v.journey_seconds);
    } else if (v.drone) {
        route_node(v.drone, { target }, v.journey_seconds);
    }
    v.side = target;
}

void Bank::tend_journeys()
{
    for (size_t i = 0; i < m_voices.size(); ++i) {
        auto& v = m_voices[i];
        if (!v.entered || v.net) {
            continue;
        }
        const bool idle = v.drone && v.drone->get_routing_state().phase == RoutingState::NONE;
        if (idle) {
            route_leg(i);
        }
    }
}

void Bank::tend_drift()
{
    if (m_mode != PerformMode::RESONANT && m_mode != PerformMode::FULL) {
        return;
    }
    const double range = drift_range_hz();
    for (auto& v : m_voices) {
        if (!v.entered || v.temperament != Temperament::CONTINUOUS) {
            continue;
        }

        if (range > 0.0 && local_uniform(0.0, 1.0) < chance(0.0006)) {
            const double octaves = local_uniform(-1.2, 1.2);
            v.root_target = std::clamp(v.root * std::pow(2.0, octaves), 220.0, 900.0);
        }
        v.root_current += (v.root_target - v.root_current) * paced(0.0025);

        for (size_t p = 0; p < v.drone_partials.size(); ++p) {
            if (range > 0.0 && local_uniform(0.0, 1.0) < chance(0.01)) {
                v.drone_detune_target[p] = local_uniform(-range, range);
            }
            v.drone_detune_hz[p] += (v.drone_detune_target[p] - v.drone_detune_hz[p]) * paced(0.03);
            v.drone_partials[p]->set_frequency(
                static_cast<float>(std::max(200.0, v.root_current + v.drone_base_freqs[p] + v.drone_detune_hz[p])));

            if (local_uniform(0.0, 1.0) < chance(0.006)) {
                v.drone_balance_target[p] = local_uniform(0.03, 1.0);
            }
            (*v.drone_balance)[p] += (v.drone_balance_target[p] - (*v.drone_balance)[p]) * paced(0.01);
        }
    }
}

void Bank::enforce_cap()
{
    const double level = energy();
    if (level <= k_energy_cap) {
        return;
    }
    const double factor = k_energy_cap / level;
    for (auto& v : m_voices) {
        if (v.entered && v.net) {
            v.net->damp(factor);
        }
    }
}

void Bank::tend_swell()
{
    for (auto& v : m_voices) {
        if (!v.entered || v.temperament != Temperament::CONTINUOUS) {
            continue;
        }
        v.swell_level = std::min(1.0, v.swell_level + k_swell_interval / v.swell_seconds);
        for (auto& partial : v.drone_partials) {
            partial->set_amplitude(v.swell_level);
        }
    }
}

double Bank::energy() const
{
    double sum = 0.0;
    for (const auto& v : m_voices) {
        if (!v.entered || !v.net) {
            continue;
        }
        for (size_t m = 0; m < k_modes; ++m) {
            auto buf = v.net->get_node_audio_buffer(m);
            if (!buf) {
                continue;
            }
            for (double s : *buf) {
                sum += s * s;
            }
        }
    }
    return std::sqrt(sum / static_cast<double>(std::max<size_t>(1, m_entered)));
}

void Garden::build(Bank& bank)
{
    namespace D = Kinesis::Discrete;

    const auto slow = [](double hz, double amplitude) { return vega.Sine(hz, amplitude); };

    m_net = std::make_shared<RelationNetwork>() | Audio[0];

    m_net->add_slots({ { .name = "a", .node = slow(0.12, 1.0) },
        { .name = "b", .node = slow(0.19, 1.0) },
        { .name = "c", .node = slow(0.29, 1.0) },
        { .name = "drift", .node = slow(0.02, 0.5) },
        { .name = "vote" },
        { .name = "odd" },
        { .name = "window" },
        { .name = "pair" },
        { .name = "orbit", .node = slow(0.05 * k_pace, 1.0) },
        { .name = "turn" } });

    m_net->add<AdvanceOperator>();

    const std::vector<size_t> all_three { 0, 1, 2 };
    const std::vector<size_t> first_two { 0, 1 };

    m_net->add<DeriveOperator>(4, all_three, D::signals_at_least, D::signals_at_least_state(2));
    m_net->add<DeriveOperator>(5, all_three, D::input_truth_table, D::input_truth_table_state(3, 0b10010110));
    m_net->add<DeriveOperator>(7, first_two, D::comparator, D::comparator_state(0.15));
    m_net->add<DeriveOperator>(9, std::vector<size_t> { 8 }, D::input_truth_table, D::input_truth_table_state(1, 0b10, 0.0));

    auto window = m_net->add<DeriveOperator>(6, first_two, [](const std::vector<double>& in, std::span<double> coefs) { return in[0] > coefs[0] && in[1] < coefs[1]; }, std::vector<double> { 0.0, 0.4 });
    window->map("c0", Source(m_net, 3));

    auto scale = std::make_shared<std::vector<double>>(std::vector<double> { 1.0, 9.0 / 8.0, 5.0 / 4.0, 3.0 / 2.0, 5.0 / 3.0 });
    auto step = std::make_shared<size_t>(0);

    Bank* const voices = &bank;
    const auto fire = [voices](size_t body, double strength) {
        if (voices->mode() == PerformMode::RESONANT || voices->mode() == PerformMode::FULL) {
            voices->strike_body(body, strength);
        }
    };

    m_net->get_slot(4).on_enter.push_back([fire] { fire(0, 0.6); });
    m_net->get_slot(5).on_enter.push_back([fire, voices, scale, step] {
        voices->pitch(1, (*scale)[*step % scale->size()]);
        fire(1, 0.5);
        ++*step;
    });
    m_net->get_slot(6).on_enter.push_back([fire] { fire(2, 0.5); });
    m_net->get_slot(7).on_enter.push_back([fire] { fire(3, 0.4); });

    constexpr double turn_seconds = 8.0 / k_pace;
    m_net->get_slot(9).on_enter.push_back([voices] { voices->turn(true, turn_seconds); });
    m_net->get_slot(9).on_leave.push_back([voices] { voices->turn(false, turn_seconds); });
}

void Written::build()
{
    constexpr size_t modes = 14;
    constexpr double glide_seconds = 6.0;
    const double rate_hz = get_scheduler()->get_rate();

    m_body = vega.ModalNetwork(modes, 73.4, ModalNetwork::Spectrum::INHARMONIC, 3.0) | Audio[0];
    m_body->set_output_scale(0.3);

    m_control = std::make_shared<RelationNetwork>() | Audio[0];
    const auto tilt = m_control->add_slot({ .name = "tilt", .node = vega.Sine(0.021 * k_pace, 1.0) });
    const auto lean = m_control->add_slot({ .name = "lean", .node = vega.Sine(0.013 * k_pace, 1.0) });
    m_control->add<AdvanceOperator>();

    m_amp = std::make_shared<RelationNetwork>() | Audio[0];
    m_bend = std::make_shared<RelationNetwork>() | Audio[0];
    for (size_t i = 0; i < modes; ++i) {
        m_amp->add_slot({});
        m_bend->add_slot({});
    }

    auto write = m_amp->add<StepOperator>(
        [body = m_body, target = m_target, rate_hz, level = 0.0, time = 0.0](const std::shared_ptr<StepOperator>& self, std::span<RelationSlot> slots, uint32_t frames) mutable {
            const double dt = static_cast<double>(frames) / rate_hz;
            time += dt;
            level += (static_cast<double>(target->load(std::memory_order_relaxed)) - level)
                * (1.0 - std::exp(-dt * k_pace / glide_seconds));

            const double rolloff = 0.6 + 1.6 * (0.5 + 0.5 * self->parameter("tilt"));
            for (size_t i = 0; i < slots.size(); ++i) {
                const double k = static_cast<double>(i);
                const double breath = 0.5 + 0.5 * std::sin(0.31 * k_pace * (1.0 + 0.173 * k) * time + 1.9 * k);
                slots[i].level = 0.55 * breath / std::pow(1.0 + k, rolloff);
                body->excite_mode(i, slots[i].level * level);
            }
        });
    write->map("tilt", Source(m_control, tilt));

    auto warp = m_bend->add<StepOperator>([](const std::shared_ptr<StepOperator>& self, std::span<RelationSlot> slots, uint32_t) {
        for (size_t i = 0; i < slots.size(); ++i) {
            slots[i].level = 0.35 * self->parameter("lean") * std::sin(1.7 * static_cast<double>(i));
        }
    });
    warp->map("lean", Source(m_control, lean));

    m_body->map_parameter("detune", m_bend);
}

} // namespace TinglingTime
// NOLINTEND
