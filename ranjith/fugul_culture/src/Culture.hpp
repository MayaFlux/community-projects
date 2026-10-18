#pragma once

namespace Culture {

inline constexpr size_t k_positions = 32;

/**
 * @class Link
 * @brief Connects the colony's evolving forms with the resonant music.
 *
 * When the sound and picture run together, the organisms colour and tune their
 * corresponding strands, while musical strikes can prompt those organisms to
 * change shape. Either side can also be experienced on its own.
 */
class Link {
public:
    struct Bond {
        std::atomic<uint32_t> colour { 8 };
        std::atomic<float> decay { 1.0F };
        std::atomic<float> beat { 1.0F };
        std::atomic<float> hit_strength { 0.0F };
        std::atomic<uint32_t> hits { 0 };
        std::atomic<uint32_t> form { 0 };
    };

    std::array<Bond, k_positions> bond;
    std::atomic<float> tempo { 1.0F };
    std::atomic<float> drift { 0.0F };

    /** @brief Share a musical strike with the organism associated with its strand. */
    void hit(size_t position, float strength, uint32_t form)
    {
        bond[position].hit_strength.store(strength, std::memory_order_relaxed);
        bond[position].form.store(form, std::memory_order_relaxed);
        bond[position].hits.fetch_add(1, std::memory_order_relaxed);
    }
};

/**
 * @brief Play the resonant web as six interweaving musical voices.
 * @param link Connect to the evolving colony, or leave empty to hear the music alone.
 */
void culture(std::shared_ptr<Link> link = nullptr);

/**
 * @brief Show a drifting colony of filament organisms as it changes over time.
 * @param link Connect to the musical web, or leave empty to see the colony alone.
 */
void organisms(std::shared_ptr<Link> link = nullptr);

/**
 * @brief Experience the colony and resonant music together, each influencing the other.
 */
void piece();

}
