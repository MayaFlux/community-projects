// NOLINTBEGIN
#include "Score.hpp"

#include "MayaFlux/Kinesis/Discrete/Dynamics.hpp"
#include "MayaFlux/Kinesis/Discrete/Word.hpp"

namespace Topsoil {

namespace {

    std::vector<bool> evolve(uint64_t rule, size_t width, size_t seed, size_t generations)
    {
        const std::vector<double> table = Kinesis::Discrete::truth_table_state(3, rule);

        std::vector<bool> now(width, false);
        std::vector<bool> next(width, false);
        now[seed % width] = true;

        for (size_t g = 0; g < generations; ++g) {
            for (size_t i = 0; i < width; ++i) {
                const std::array<bool, 3> history { now[(i + 1) % width], now[i], now[(i + width - 1) % width] };
                next[i] = Kinesis::Discrete::truth_table(history, table);
            }
            now.swap(next);
        }
        return now;
    }

    std::string automaton(uint64_t rule, size_t width, size_t seed, size_t generations, uint32_t division)
    {
        const std::vector<bool> cells = evolve(rule, width, seed, generations);
        std::string bar(width, '.');
        for (size_t i = 0; i < width; ++i) {
            if (cells[i]) {
                bar[i] = (i * division) % 16 == 0 ? 'X' : 'x';
            }
        }
        return bar;
    }

    void cut(Row& row, uint32_t division, size_t bar, size_t from_step)
    {
        std::string& cells = row[bar];
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i * division >= from_step) {
                cells[i] = '.';
            }
        }
    }

}

std::shared_ptr<const Page> make_page()
{
    auto page = std::make_shared<Page>();
    auto& [sub, tick, gate, noise] = page->rows;

    const size_t sub_width = k_bar_steps / k_gates[0].division;
    const size_t tick_width = k_bar_steps / k_gates[1].division;
    const size_t gate_width = k_bar_steps / k_gates[2].division;
    const size_t noise_width = k_bar_steps / k_burst.division;

    for (size_t bar = 0; bar < k_phrase_bars; ++bar) {
        sub[bar] = std::string(sub_width, '.');
        tick[bar] = std::string(tick_width, '.');
        gate[bar] = std::string(gate_width, '.');
        noise[bar] = std::string(noise_width, '.');
    }

    for (size_t bar = 0; bar < 12; ++bar) {
        sub[bar] = "Xxxxxxxx";
    }
    sub[12] = "X.x.x.x.";
    sub[13] = "X...X...";
    sub[14] = "X.......";

    for (const Strand& strand : k_strands) {
        const uint32_t part_division = division(strand.part);
        const size_t width = k_bar_steps / part_division;
        const size_t seed = strand.seed == Seed::First ? 0 : (strand.seed == Seed::Middle ? width / 2 : width - 1);
        page->rows[static_cast<size_t>(strand.part)][strand.bar] = automaton(strand.rule, width, seed, strand.generations, part_division);
    }
    gate[2][gate_width / 2] = 'X';
    gate[3] = gate[2];

    for (size_t bar = 6; bar < 10; ++bar) {
        noise[bar][(bar * 7 + 3) % noise_width] = 'X';
    }

    for (const auto& [bar, from_step] : k_cuts) {
        cut(sub, k_gates[0].division, bar, from_step);
        cut(tick, k_gates[1].division, bar, from_step);
        cut(gate, k_gates[2].division, bar, from_step);
        cut(noise, k_burst.division, bar, from_step);
    }

    return page;
}

Cues::Cues(std::shared_ptr<Vruta::TaskScheduler> scheduler, std::shared_ptr<const Origin> origin, std::shared_ptr<const Page> page)
    : m_scheduler(std::move(scheduler))
    , m_origin(std::move(origin))
    , m_page(std::move(page))
    , m_rate(static_cast<double>(Buffers::s_registered_sample_rate))
    , m_base(static_cast<uint64_t>(static_cast<double>(Buffers::s_registered_sample_rate) / k_grid_hz))
{
}

bool Cues::started() const
{
    return m_origin->set;
}

uint64_t Cues::elapsed() const
{
    const uint64_t now = m_scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position();
    return now > m_origin->position ? now - m_origin->position : 0;
}

uint64_t Cues::step_samples(Part part) const
{
    return m_base * division(part);
}

double Cues::seconds() const
{
    return started() ? static_cast<double>(elapsed()) / m_rate : 0.0;
}

size_t Cues::bar() const
{
    return static_cast<size_t>(elapsed() / (m_base * k_bar_steps) % k_phrase_bars);
}

uint64_t Cues::cell(Part part) const
{
    return elapsed() / step_samples(part);
}

const Strand* Cues::strand(Part part, size_t bar) const
{
    const auto it = std::ranges::find_if(k_strands, [part, bar](const Strand& s) { return s.part == part && s.bar == bar; });
    return it == k_strands.end() ? nullptr : &*it;
}

std::vector<Hit> Cues::advance()
{
    std::vector<Hit> hits;
    if (!started()) {
        return hits;
    }

    for (size_t p = 0; p < m_next.size(); ++p) {
        const auto part = static_cast<Part>(p);
        const uint64_t now = cell(part);
        for (uint64_t c = m_next[p]; c <= now; ++c) {
            const double level = level_at(m_page->rows[p], c);
            if (level > 0.0) {
                hits.push_back({ part, c, level, static_cast<double>(c * step_samples(part)) / m_rate });
            }
        }
        m_next[p] = now + 1;
    }

    std::ranges::sort(hits, {}, &Hit::seconds);
    return hits;
}

Score::Score(uint32_t channel, std::shared_ptr<Vruta::TaskScheduler> scheduler, std::shared_ptr<Origin> origin,
    std::shared_ptr<const Page> page)
    : m_channel(channel)
    , m_noise(0x9E3779B9U * (channel + 1))
    , m_scheduler(std::move(scheduler))
    , m_origin(std::move(origin))
    , m_page(std::move(page))
    , m_step_samples(static_cast<uint64_t>(static_cast<double>(Buffers::s_registered_sample_rate) / k_grid_hz) * k_burst.division)
    , m_rate(static_cast<double>(Buffers::s_registered_sample_rate))
{
}

bool Score::is_compatible_with(const std::shared_ptr<Buffers::Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer) != nullptr;
}

void Score::processing_function(const std::shared_ptr<Buffers::Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
    if (!audio) {
        return;
    }

    auto& data = audio->get_data();
    std::ranges::fill(data, 0.0);
    if (!m_origin->set) {
        return;
    }

    const auto now = static_cast<int64_t>(m_scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE).current_position());
    int64_t position = now - static_cast<int64_t>(m_origin->position);

    for (double& sample : data) {
        if (position >= 0) {
            sample = render(static_cast<uint64_t>(position));
        }
        ++position;
    }
}

double Score::render(uint64_t position)
{
    m_noise = Kinesis::Discrete::xorshift32(m_noise);
    const double noise = Kinesis::Discrete::from_word(m_noise, 32);

    const double tail = k_burst.hold + k_burst.fall * 8.0;
    const auto reach = static_cast<uint64_t>(std::ceil(tail * m_rate / static_cast<double>(m_step_samples))) + 1;
    const uint64_t now = position / m_step_samples;

    double sum = 0.0;
    for (uint64_t back = 0; back <= reach && back <= now; ++back) {
        const uint64_t cell = now - back;
        const double level = level_at(m_page->rows[3], cell);
        if (level == 0.0) {
            continue;
        }

        const double t = static_cast<double>(position - cell * m_step_samples) / m_rate;
        if (t > tail) {
            continue;
        }
        sum += level * Kinesis::Discrete::pulse_response(t, 0.0, k_burst.hold, k_burst.rise, k_burst.fall);
    }
    return std::tanh(sum * k_burst.level * k_master * k_burst.pan[m_channel] * noise * 2.0);
}

} // namespace Topsoil
// NOLINTEND
