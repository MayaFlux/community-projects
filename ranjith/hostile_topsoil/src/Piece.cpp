// NOLINTBEGIN
#include "Piece.hpp"

#include "MayaFlux/Kinesis/Discrete/Dynamics.hpp"

namespace Topsoil {

void Machine::build()
{
    set_mix_normalization_for_channels({ 0, 1 }, false);

    m_scheduler = get_scheduler();
    m_page = make_page();
    m_cues = std::make_shared<Cues>(m_scheduler, m_origin, m_page);

    for (uint32_t channel = 0; channel < k_channels; ++channel) {
        auto buffer = vega.AudioBuffer(channel, 512) | Audio[channel];
        buffer->set_default_processor(std::make_shared<Score>(channel, m_scheduler, m_origin, m_page));
        m_buffers.push_back(buffer);
    }

    for (size_t i = 0; i < k_gates.size(); ++i) {
        gate(k_gates[i], m_page->rows[i], "topsoil_gate_" + std::to_string(i));
    }

    m_drone = std::make_unique<DroneBank>(m_scheduler);
    m_drone->build();

    m_tape = std::make_unique<Tape>(*m_drone);
    m_tape->build();

    m_specimen = std::make_unique<Specimen>(m_cues);
    m_specimen->build(*m_drone);
}

void Machine::gate(const Gate& spec, const Row& row, const std::string& name)
{
    auto tone = vega.Sine(spec.frequency, 0.0) | Audio[{ 0, 1 }];

    const double rate = static_cast<double>(Buffers::s_registered_sample_rate);
    const auto cell_samples = static_cast<uint32_t>(rate / k_grid_hz) * spec.division;
    const double tail = std::min(spec.hold + spec.fall * 8.0, static_cast<double>(cell_samples - 2 * k_ramp_samples) / rate);
    const auto count = static_cast<uint32_t>(tail * rate / k_ramp_samples);
    const double ramp = k_ramp_samples / rate;
    const double remainder = static_cast<double>(cell_samples - count * k_ramp_samples) / rate;
    const double whole = static_cast<double>(cell_samples) / rate;

    auto cursor = std::make_shared<std::pair<uint64_t, uint32_t>>(0, 0);

    schedule_pattern(
        [&spec, &row, count, rate, cursor](uint64_t) {
            auto& [cell, slot] = *cursor;
            const double hit = level_at(row, cell);
            double value = 0.0;
            if (hit > 0.0 && slot < count) {
                const double t = static_cast<double>(slot * k_ramp_samples) / rate;
                value = spec.level * k_master * hit * Kinesis::Discrete::pulse_response(t, 0.0, spec.hold, spec.rise, spec.fall);
            }
            if (++slot >= (hit > 0.0 ? count + 1 : 1)) {
                slot = 0;
                ++cell;
            }
            return value;
        },
        [tone, origin = m_origin, scheduler = m_scheduler](double level) {
            if (!origin->set) {
                origin->set = true;
                origin->position = scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position();
            }
            tone->set_amplitude(level);
        },
        [&row, count, ramp, remainder, whole](uint64_t pass) {
            if (level_at(row, pass) <= 0.0) {
                return std::vector<double> { whole };
            }
            std::vector<double> durations(count, ramp);
            durations.push_back(remainder);
            return durations;
        },
        name);
}

void topsoil()
{
    auto machine = make_persistent_shared<Machine>();
    machine->build();
}

} // namespace Topsoil
// NOLINTEND
