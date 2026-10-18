// NOLINTBEGIN
#pragma once

#include "Material.hpp"

namespace Attrition {

/**
 * @class Survey
 * @brief Follows the volume as its shape and structure change.
 */
class Survey {
public:
    /**
     * @brief Read the changing volume and update its state.
     * @param mass Volume being observed.
     * @param out Receives the latest reading.
     */
    void step(Mass& mass, Measures& out);

    /** @brief Begin a fresh reading for a new form. */
    void reset(Measures& out);

private:
    std::vector<float> m_density;
    uint64_t m_churn { 0 };
};

/**
 * @class Look
 * @brief Lets the image transform gradually as the volume changes.
 */
class Look {
public:
    /**
     * @brief Let the image follow the volume's latest changes.
     * @param mass Volume being shown.
     * @param mode Current form and its colours.
     * @param measures Latest reading of the volume.
     * @param feel Shared sound intensity.
     */
    void step(Mass& mass, const Mode& mode, const Measures& measures, const Feel& feel);

    /** @brief Return the image to the beginning of a new form. */
    void reset();

private:
    void colour(Mass& mass, const Mode& mode, const Feel& feel) const;

    double m_lost { 0.0 };
    double m_broken { 0.0 };
};

/**
 * @class Voices
 * @brief Five musical voices whose pace and character answer the changing volume and the pen.
 */
class Voices {
public:
    /**
     * @brief Prepare the voices and their connection to the volume.
     * @param mass Volume shaped by the sound.
     * @param conduct Controls for the performance.
     */
    void open(Mass& mass, Conduct& conduct);

    /**
     * @brief Let the voices evolve with the volume and the pen's movement.
     * @param measures Latest reading of the volume.
     * @param pen Current gesture.
     * @param conduct Controls for the performance.
     * @param feel Receives the shared pace and intensity.
     */
    void step(const Measures& measures, const Gesture& pen, Conduct& conduct, Feel& feel);

private:
    struct Lane {
        std::shared_ptr<Kriya::SamplingPipeline> sampler;
        uint32_t channel { 0 };
    };

    struct Voice {
        std::array<Kriya::TapSet, k_taps> sets;
        std::array<Lane, k_taps> lanes;
        std::array<double, k_taps> ratio {};
        std::array<double, k_taps> level {};
        std::shared_ptr<Buffers::InfluxProcessor> influx;
        double trim { 1.0 };
        double gain { 0.0 };
        double direction { 1.0 };
        double target { 1.0 };
        double pending { 1.0 };
        double energy { 0.0 };
        double phrase { 0.0 };
        double clock { 0.0 };
    };

    void turn(Voice& voice, const VoiceProfile& profile, const Feel& feel);
    double meter(Voice& voice) const;

    std::array<Voice, k_voices> m_voices;
    uint32_t m_outputs { 1 };
    double m_breath { 0.0 };
    double m_slow { 1.0 };
    double m_fast { 1.0 };
};

} // namespace Attrition
// NOLINTEND
