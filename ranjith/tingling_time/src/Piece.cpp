// NOLINTBEGIN
#include "Piece.hpp"

namespace TinglingTime {

void Piece::start()
{
    if (!Hand::present()) {
        std::cout << "no tablet\n";
        return;
    }

    m_recency = make_timer();
    m_window = MayaFlux::create_window({ "pen", 1920, 1080 });

    m_bank.on_strike([this](size_t body, double strength) { m_eye.strike(body, strength); });
    m_bank.build();
    m_bank.enter(0);
    m_bank.enter(1);
    m_garden.build(m_bank);
    m_written.build();

    bind_keys();
    m_eye.open(m_window);
    m_window->show();
    schedule();

    Hand::choose([this](std::optional<uint32_t> tool) {
        if (!tool) {
            std::cout << "no tablet chosen\n";
            return;
        }
        m_hand.bind(*tool, {
                               .contact = [this](float pressure) { contact(pressure); },
                               .frame = [this] { frame(); },
                           });
    });
}

void Piece::contact(float pressure)
{
    ++m_contacts;

    m_bank.force_strike(0.75);
    if (m_bank.mode() == PerformMode::FULL) {
        m_bank.burst();
    }

    if (m_contacts % 4 == 0) {
        m_bank.admit(0.5 + local_uniform(0.0, 3.0));
    }

    if (pressure >= k_hard_press) {
        m_bank.accent(m_recency && m_recency->is_active());
    }

    if (m_recency) {
        m_recency->schedule(0.9, []() { });
    }
}

void Piece::choose(PerformMode mode)
{
    m_bank.set_mode(mode);
    m_written.settle((mode == PerformMode::RESONANT || mode == PerformMode::FULL) ? 1.0 : 0.0);
}

void Piece::frame()
{
    const Gesture& gesture = m_hand.gesture();
    const double motion = gesture.motion.load(std::memory_order_relaxed);

    m_eye.move(motion);
    m_bank.drive(motion);

    if (!gesture.touching.load(std::memory_order_relaxed)) {
        return;
    }

    if (m_bank.mode() != PerformMode::TAP && local_uniform(0.0, 1.0) < motion * 0.15) {
        m_bank.strike(std::min(1.0, 0.5 + motion));
    }
}

void Piece::schedule()
{
    schedule_metro(0.25, [this] { m_bank.tend_journeys(); }, "voice_journeys");
    schedule_metro(0.5, [this] { m_bank.tend_drift(); }, "voice_drift");
    schedule_metro(0.1, [this] { m_bank.enforce_cap(); }, "tingling_cap");
    schedule_metro(k_swell_interval, [this] { m_bank.tend_swell(); }, "voice_swell");
}

void Piece::bind_keys()
{
    on_key_pressed(m_window, IO::Keys::N1, [this] { choose(PerformMode::TAP); });
    on_key_pressed(m_window, IO::Keys::N2, [this] { choose(PerformMode::TAP_MOTION); });
    on_key_pressed(m_window, IO::Keys::N3, [this] { choose(PerformMode::RESONANT); });
    on_key_pressed(m_window, IO::Keys::N4, [this] { choose(PerformMode::FULL); });
}

void tingling_time()
{
    auto piece = make_persistent_shared<Piece>();
    piece->start();
}

} // namespace TinglingTime
// NOLINTEND
