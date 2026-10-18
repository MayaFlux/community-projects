#include "Culture.hpp"

#include "MayaFlux/MayaFlux.hpp"

namespace Culture {

using namespace MayaFlux;
using Nodes::Generator::Polynomial;
using Nodes::Network::ResonatorNetwork;
namespace Discrete = Kinesis::Discrete;

namespace {

    constexpr size_t positions = k_positions;
    constexpr size_t per_tier = 8;
    constexpr size_t banks = 2;
    constexpr size_t slots = 16;
    constexpr double pitch = 715.0;
    constexpr double grid = 0.25;
    constexpr double web_level = 0.6;
    constexpr double entry_seconds = 12.0;
    constexpr double speed_up_seconds = 240.0;
    constexpr double drift_seconds = 300.0;
    constexpr double fugue_seconds = 300.0;
    constexpr double leave_seconds = 10.0;

    constexpr std::array<int, 12> steps { 0, 2, 4, 7, 5, 4, 2, 0, -1, 3, 7, 5 };

    enum class Kind : uint8_t { Strike,
        Tone,
        Train };

    using Word = std::array<double, slots>;

    struct Cell {
        Kind mode { Kind::Strike };
        std::shared_ptr<double> clock;
        Word bits {};
        double dt { 0.0 };
        double age { 0.0 };
        double length { 0.0 };
        double edge { 1.0 };
        double level { 0.0 };
        double period { 16.0 };
        double phase { 0.0 };
        double step { 0.0 };
    };

    struct Web {
        std::array<std::shared_ptr<ResonatorNetwork>, banks> bank;
        std::vector<std::shared_ptr<Cell>> cells;
        std::vector<double> rest;
        std::vector<double> mode;
        std::vector<double> walk;
        std::vector<double> hz;
        std::vector<double> alpha;
        std::array<std::vector<double>, banks> tuned;
        std::array<std::vector<double>, banks> decay;
        double rate { 48000.0 };
        uint32_t rng { 7U };
    };

    struct Walker {
        Kind kind { Kind::Strike };
        std::vector<int> partners;
        size_t id { 0 };
        size_t base { 0 };
        size_t phase { 0 };
        size_t shift { 0 };
        size_t pulses { 4 };
        size_t rotation { 0 };
        size_t colour { 4 };
        size_t count { 0 };
        bool complement { false };
        bool announced { false };
        double slot { grid };
        double hold { 1.0 };
        double edge { 96.0 };
        double level { 0.5 };
        double partner_level { 0.5 };
        double ends_at { 0.0 };
    };

    double unit(uint32_t& state)
    {
        state = state * 1664525U + 1013904223U;
        return static_cast<double>(state >> 8) / 16777216.0;
    }

    double pace(double time)
    {
        return 1.0 + time / speed_up_seconds;
    }

    double speed(double time, const Link* link)
    {
        return pace(time) * (link != nullptr ? static_cast<double>(link->tempo.load(std::memory_order_relaxed)) : 1.0);
    }

    Word word(size_t pulses, size_t rotation, size_t form)
    {
        auto sequence = Discrete::maximally_even_sequence(pulses, slots, rotation);

        switch (form) {
        case 1:
            std::ranges::reverse(sequence);
            break;
        case 2:
            for (auto& bit : sequence) {
                bit = 1.0 - bit;
            }
            break;
        case 3:
            std::ranges::rotate(sequence, sequence.begin() + slots / 2);
            break;
        default:
            break;
        }

        Word out {};
        std::ranges::copy(sequence, out.begin());
        return out;
    }

    double excite(Cell& cell)
    {
        if (cell.clock) {
            *cell.clock += cell.dt;
        }
        if (cell.age >= cell.length) {
            return 0.0;
        }

        const double age = cell.age;
        cell.age += 1.0;

        switch (cell.mode) {
        case Kind::Strike:
            return cell.level * (cell.bits[static_cast<size_t>(age)] > 0.5 ? 1.0 : -1.0);
        case Kind::Train: {
            const double position = std::fmod(age, cell.period);
            return position < static_cast<double>(slots)
                ? cell.level * (cell.bits[static_cast<size_t>(position)] > 0.5 ? 1.0 : -1.0)
                : 0.0;
        }
        default: {
            const double envelope = std::min({ 1.0, age / cell.edge, (cell.length - age) / cell.edge });
            cell.phase += cell.step;
            return cell.level * envelope * std::sin(cell.phase);
        }
        }
    }

    std::shared_ptr<Web> make_web(size_t channels, double rate, const std::shared_ptr<double>& time)
    {
        auto web = std::make_shared<Web>();
        web->rate = rate;
        web->rest.resize(positions);
        web->mode.resize(positions);
        web->walk.assign(positions, 0.0);
        web->hz.assign(positions * banks, 110.0);
        web->alpha.assign(positions * banks, 1e-4);

        std::array<std::vector<double>, banks> decays;
        for (size_t b = 0; b < banks; ++b) {
            web->tuned[b].assign(positions, 110.0);
            decays[b].assign(positions, 1.0);
        }

        for (size_t p = 0; p < positions; ++p) {
            const double step = static_cast<double>(p % per_tier) / static_cast<double>(per_tier);
            web->rest[p] = 6.5 * std::pow(2.0, -step) * (0.97 + 0.06 * unit(web->rng));
            web->mode[p] = std::pow(2.0, static_cast<double>(p / per_tier)) * 0.5;

            const double roll = unit(web->rng);
            const double thickness = roll < 0.70 ? 0.22 + 0.28 * unit(web->rng)
                : roll < 0.94                    ? 0.45 + 0.45 * unit(web->rng)
                                                 : 0.9 + 0.7 * unit(web->rng);
            const double ring = 0.35 + 6.65 * (thickness - 0.22) / 1.38;
            const double bass = p < per_tier ? 2.0 : 1.0;
            decays[0][p] = ring * bass;
            decays[1][p] = ring * bass;
        }

        for (size_t b = 0; b < banks; ++b) {
            web->bank[b] = vega.ResonatorNetwork(
                               std::vector<double>(positions, 110.0),
                               std::vector<double>(positions, 200.0))
                | Audio[static_cast<uint32_t>(b % channels)];
            web->bank[b]->set_decay(decays[b]);
        }
        web->decay = decays;

        for (size_t i = 0; i < positions * banks; ++i) {
            auto cell = std::make_shared<Cell>();
            cell->dt = 1.0 / rate;
            cell->age = 0.0;
            cell->length = 0.0;
            if (i == 0) {
                cell->clock = time;
            }
            web->bank[i % banks]->set_resonator_exciter(i / banks,
                std::make_shared<Polynomial>([cell](double) { return excite(*cell); }));
            web->cells.push_back(cell);
        }
        return web;
    }

    void tune(Web& web, double time, const Link* link)
    {
        const double drift = std::min(1.0, time / drift_seconds);
        const double breath = 1.0 + 0.04 * std::sin(2.0 * std::numbers::pi * time / 90.0);
        const double beat = 0.2 + 8.0 * drift * drift;

        for (size_t p = 0; p < positions; ++p) {
            web.walk[p] = std::clamp(web.walk[p] + 0.008 * (unit(web.rng) - 0.5), -0.03, 0.03);
            const double freq = pitch * web.mode[p] / (web.rest[p] * breath * (1.0 + web.walk[p]));
            const double spread = link != nullptr ? static_cast<double>(link->bond[p].beat.load(std::memory_order_relaxed)) : 1.0;
            for (size_t b = 0; b < banks; ++b) {
                web.tuned[b][p] = freq + (b == 0 ? 0.0 : beat * spread);
                web.hz[p * banks + b] = web.tuned[b][p];
            }
        }

        for (size_t b = 0; b < banks; ++b) {
            web.bank[b]->retune(web.tuned[b]);

            if (link != nullptr) {
                std::vector<double> decays(positions);
                for (size_t p = 0; p < positions; ++p) {
                    decays[p] = web.decay[b][p] * static_cast<double>(link->bond[p].decay.load(std::memory_order_relaxed));
                }
                web.bank[b]->set_decay(decays);
            }

            std::vector<double> gains(positions);
            for (size_t p = 0; p < positions; ++p) {
                const double q = web.bank[b]->get_resonators().at(p).q;
                const double alpha = std::sin(2.0 * std::numbers::pi * web.tuned[b][p] / web.rate) / (2.0 * q);
                web.alpha[p * banks + b] = alpha;
                gains[p] = web_level / (4.0 * alpha);
            }
            web.bank[b]->set_resonator_gain(gains);
        }
    }

    size_t pick(Walker& walker, size_t cycle)
    {
        const size_t form = (cycle / 2 + walker.id) % 4;
        const size_t m = (walker.count++ + walker.phase) % steps.size();
        int delta = form == 1 ? steps[steps.size() - 1 - m] : steps[m];
        if (form == 2) {
            delta = -delta;
        } else if (form == 3) {
            delta *= 2;
        }
        return static_cast<size_t>(std::clamp(static_cast<int>(walker.base) + delta, 0, static_cast<int>(positions) - 1));
    }

    void place(const Walker& walker, const Web& web, size_t position, size_t form, double slot_samples, double gain, size_t colour)
    {
        for (size_t b = 0; b < banks; ++b) {
            const size_t strand = position * banks + b;
            Cell& cell = *web.cells[strand];
            const double ear = (b == walker.count % 2) ? 1.0 : 0.55;
            const double level = walker.level * gain * ear;

            cell.mode = walker.kind;
            cell.age = 0.0;

            switch (walker.kind) {
            case Kind::Strike:
                cell.bits = word(colour, position, form);
                cell.length = static_cast<double>(slots);
                cell.level = level;
                break;
            case Kind::Train:
                cell.bits = word(colour, position, form);
                cell.period = std::max(static_cast<double>(slots), web.rate / web.hz[strand]);
                cell.length = static_cast<double>(8 + position % 12) * cell.period;
                cell.level = level * web.alpha[strand] * cell.period * 0.5;
                break;
            default:
                cell.phase = 0.0;
                cell.step = 2.0 * std::numbers::pi * web.hz[strand] / web.rate;
                cell.length = walker.hold * slot_samples;
                cell.edge = walker.edge;
                cell.level = level * 4.0 * web.alpha[strand];
                break;
            }
        }
    }

    size_t flavour(const Walker& walker, size_t position, const Link* link)
    {
        if (link == nullptr) {
            return walker.colour;
        }
        const int shift = static_cast<int>(link->bond[position].colour.load(std::memory_order_relaxed)) - 8;
        return static_cast<size_t>(std::clamp(static_cast<int>(walker.colour) + shift, 1, 15));
    }

    void fire(const Walker& walker, const Web& web, size_t position, size_t form, double slot_samples, const Link* link)
    {
        place(walker, web, position, form, slot_samples, 1.0, flavour(walker, position, link));
        for (const int offset : walker.partners) {
            const auto other = static_cast<size_t>(std::clamp(static_cast<int>(position) + offset, 0, static_cast<int>(positions) - 1));
            place(walker, web, other, form, slot_samples, walker.partner_level, flavour(walker, other, link));
        }
    }

}

void culture(std::shared_ptr<Link> link)
{
    const size_t channels = std::max<size_t>(Config::get_num_out_channels(), 1);
    const auto rate = static_cast<double>(seconds_to_samples(1.0));

    auto scheduler = get_scheduler();
    auto time = std::make_shared<double>(0.0);
    auto web = make_web(channels, rate, time);
    MF_LOG(Journal::Component::USER, Journal::Context::UserCode,
        "Culture: web built, {} channels, rate {}", channels, rate);

    tune(*web, 0.0, link.get());
    MF_LOG(Journal::Component::USER, Journal::Context::UserCode,
        "Culture: tuned, position 0 {:.2f} Hz, position {} {:.2f} Hz, q0 {:.1f}, gain0 {:.1f}",
        web->tuned[0][0], positions - 1, web->tuned[0][positions - 1],
        web->bank[0]->get_resonators().at(0).q,
        web->bank[0]->get_resonators().at(0).gain);

    auto motion = Kriya::pattern<uint64_t>(
        [](uint64_t i) { return i; },
        [web, time, link](uint64_t) {
            tune(*web, *time, link.get());
            if (link) {
                link->drift.store(static_cast<float>(std::min(1.0, *time / drift_seconds)), std::memory_order_relaxed);
            } },
        [time](uint64_t) { return std::vector<double> { static_cast<double>(slots) * 2.0 * grid / pace(*time) }; },
        Vruta::ProcessingToken::SAMPLE_ACCURATE);
    scheduler->add_task(motion, "culture_web", true);

    std::vector<std::shared_ptr<Walker>> walkers;
    auto add = [&](Kind kind, size_t base, double slot_multiple, size_t pulses, size_t rotation) {
        auto walker = std::make_shared<Walker>();
        walker->kind = kind;
        walker->id = walkers.size();
        walker->base = base;
        walker->slot = grid * slot_multiple;
        walker->pulses = pulses;
        walker->rotation = rotation;
        walker->ends_at = fugue_seconds + leave_seconds * static_cast<double>(walker->id);
        walkers.push_back(walker);
        return walker;
    };

    auto bass = add(Kind::Tone, 2, 4.0, 3, 0);
    bass->hold = 1.6;
    bass->edge = 1500.0;
    bass->level = 0.7;
    bass->partners = { 8, 13 };
    bass->shift = 1;

    auto cantus = add(Kind::Strike, 10, 2.0, 7, 0);
    cantus->colour = 4;
    cantus->level = 0.9;

    auto line = add(Kind::Tone, 28, 1.0, 4, 3);
    line->hold = 0.35;
    line->edge = 96.0;
    line->level = 0.5;
    line->shift = 2;

    auto answer = add(Kind::Strike, 18, 2.0, 7, 0);
    answer->complement = true;
    answer->colour = 9;
    answer->phase = 6;
    answer->level = 0.8;

    auto tick = add(Kind::Train, 22, 1.0, 6, 1);
    tick->colour = 6;
    tick->level = 0.35;
    tick->shift = 3;

    auto elaboration = add(Kind::Strike, 26, 1.0, 3, 5);
    elaboration->colour = 13;
    elaboration->partners = { 3 };
    elaboration->partner_level = 0.6;
    elaboration->level = 0.5;
    elaboration->shift = 2;

    auto entries = make_persistent_shared<Kriya::EventChain>(*scheduler, "culture_entries");

    for (const auto& walker : walkers) {
        entries->then([scheduler, walker, web, time, rate, link] {
            auto routine = Kriya::pattern<uint64_t>(
                [](uint64_t i) { return i; },
                [walker, web, time, rate, link](uint64_t i) {
                    if (*time >= walker->ends_at) {
                        return;
                    }

                    const size_t slot = i % slots;
                    const size_t cycle = i / slots;
                    const size_t form = (cycle + walker->shift) % 4;
                    const Word mask = word(walker->pulses, walker->rotation, form);
                    if ((walker->complement ? 1.0 - mask[slot] : mask[slot]) < 0.5) {
                        return;
                    }

                    const size_t position = pick(*walker, cycle);
                    if (!walker->announced) {
                        walker->announced = true;
                        MF_LOG(Journal::Component::USER, Journal::Context::UserCode,
                            "Culture: walker {} first fire at {:.2f}s on position {}", walker->id, *time, position);
                    }
                    fire(*walker, *web, position, form, walker->slot / speed(*time, link.get()) * rate, link.get());
                    if (link) {
                        link->hit(position, static_cast<float>(walker->level), static_cast<uint32_t>(form));
                    }
                },
                [walker, time, link](uint64_t) {
                    std::vector<double> gaps;
                    if (*time < walker->ends_at) {
                        gaps.assign(slots, walker->slot / speed(*time, link.get()));
                    }
                    return gaps;
                },
                Vruta::ProcessingToken::SAMPLE_ACCURATE);

            scheduler->add_task(routine, "culture_walker_" + std::to_string(walker->id), true); }, walker->id == 0 ? 0.0 : entry_seconds);
    }

    entries->start();
    MF_LOG(Journal::Component::USER, Journal::Context::UserCode,
        "Culture: {} walkers scheduled", walkers.size());
}

void piece()
{
    auto link = std::make_shared<Link>();
    organisms(link);
    culture(link);
}

}
