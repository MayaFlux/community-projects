// NOLINTBEGIN
#pragma once

#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/SimulationIncludes.hpp"

#include "MayaFlux/Kinesis/MotionChannel.hpp"

/**
 * @file Attrition.hpp
 * @brief The shared ideas and starting forms behind Smoke and the Water.
 *
 * A hand moves through a changing volume as five voices gradually join it.
 * Touch, sound, and the medium all influence the work as its forms slowly
 * loosen and transform.
 */
namespace Attrition {

using namespace MayaFlux;

inline constexpr uint32_t k_window_width = 3840;
inline constexpr uint32_t k_window_height = 2160;
inline constexpr float k_aspect = static_cast<float>(k_window_width) / static_cast<float>(k_window_height);

inline constexpr uint32_t k_sim = 64;
inline constexpr uint32_t k_sim_x = 114;
inline constexpr float k_half = 1.9F;
inline constexpr float k_half_x = k_half * k_aspect;
inline constexpr float k_eye_distance = 5.2F;
inline constexpr float k_form = 1.2F;
inline constexpr float k_form_inv = 1.0F / k_form;
inline constexpr float k_dt = 1.0F / 60.0F;
inline constexpr int k_jacobi = 36;

/** @brief The pace of the work. */
inline constexpr double k_pace = 1.0;

inline constexpr double k_stir_interval = 0.14;
inline constexpr size_t k_max_impulses = 3;
inline constexpr size_t k_queue = 256;
inline constexpr float k_ring_radius = 0.26F * k_form;
inline constexpr float k_ring_strength = 2.4F;
inline constexpr float k_jet_share = 0.30F;
inline constexpr float k_swirl_share = 1.0F;
inline constexpr float k_broad_factor = 1.8F;
inline constexpr float k_speed_share = 0.7F;
inline constexpr float k_blaze_radius = 0.30F * k_form;
inline constexpr float k_unwind = 0.8F;
inline constexpr double k_unwind_onset = 0.3;
inline constexpr uint64_t k_unwind_delay = 43;

inline constexpr uint32_t k_cells_x = 4;
inline constexpr uint32_t k_cells_y = 3;
inline constexpr size_t k_trajectory_window = 48;
inline constexpr size_t k_gesture_window = 8;
inline constexpr float k_accent_ref = 300.0F;
inline constexpr float k_accent_gain = 0.8F;
inline constexpr float k_curvature_ref = 10.0F;
inline constexpr float k_curl_tighten = 0.4F;
inline constexpr float k_inject_crossings = 4.0F;
inline constexpr float k_inject_wide = 1.9F;
inline constexpr float k_inject_tight = 0.5F;
inline constexpr float k_inject_throw = 0.5F;

inline constexpr double k_survey_interval = 1.2;
inline constexpr uint64_t k_rung_reluctance = 55;
inline constexpr uint64_t k_opening_surveys = 4;

inline constexpr double k_look_interval = 1.0 / 30.0;
inline constexpr double k_look_rise = 0.08;
inline constexpr double k_look_fall = 0.004;
inline constexpr float k_loud_glow = 0.6F;

inline constexpr size_t k_voices = 5;
inline constexpr size_t k_taps = 2;
inline constexpr double k_voice_interval = 1.0 / 30.0;
inline constexpr uint64_t k_noise_max = 48000 * 90;
inline constexpr double k_target_rms = 0.5;
inline constexpr double k_pen_speed_ref = 3.0;
inline constexpr double k_loud_gain = 10.0;
inline constexpr double k_output_gain = 3.0;
inline constexpr double k_tempo_min = 0.25;
inline constexpr double k_tempo_max = 3.0;
inline constexpr double k_flip_band = 0.15;
inline constexpr double k_breath_period = 97.0;
inline constexpr float k_voice_influx = 0.9F;

/** @brief The five recordings the voices play, one per voice. Replace these with paths to your own sounds. */
inline const std::array<std::string, k_voices> k_noise {
    "res/1.wav", "res/2.wav", "res/wind.wav", "res/3.wav", "res/source.wav"
};

inline constexpr std::array<double, k_taps> k_tap_polarity { 1.0, -1.0 };
inline constexpr std::array<std::pair<double, double>, k_taps> k_tap_windows {
    std::pair { 0.0, 1.0 }, std::pair { 0.35, 0.9 }
};

inline constexpr std::array<glm::vec3, k_voices> k_voice_anchors {
    glm::vec3 { -0.7F * k_half_x, -0.5F * k_half, 0.0F },
    glm::vec3 { -0.35F * k_half_x, 0.55F * k_half, 0.3F * k_half },
    glm::vec3 { 0.0F, -0.1F * k_half, -0.4F * k_half },
    glm::vec3 { 0.35F * k_half_x, 0.55F * k_half, 0.2F * k_half },
    glm::vec3 { 0.7F * k_half_x, -0.5F * k_half, -0.2F * k_half }
};

/**
 * @struct VoiceProfile
 * @brief The character and entrance of one of the five musical voices.
 */
struct VoiceProfile {
    double arrival;
    double wake;
    double turn;
    double phrase;
    double fade;
    double period;
    double pitch;
};

inline constexpr std::array<VoiceProfile, k_voices> k_profiles {
    VoiceProfile { 0.00, 6.0, 1.5, 7.0, 3.0, 37.0, 1.0 },
    VoiceProfile { 0.10, 10.0, 4.0, 11.0, 5.0, 53.0, 0.75 },
    VoiceProfile { 0.26, 14.0, 2.5, 16.0, 8.0, 71.0, 1.0 },
    VoiceProfile { 0.45, 18.0, 7.0, 5.0, 4.0, 89.0, 1.5 },
    VoiceProfile { 0.68, 24.0, 3.0, 23.0, 9.0, 113.0, 0.5 }
};

/**
 * @struct Section
 * @brief A point in the work's transformation, where the sound begins to shift.
 */
struct Section {
    double until;
    std::array<double, k_taps> ratios;
};

inline constexpr std::array<Section, 4> k_sections {
    Section { 0.20, { 1.0, 0.5 } },
    Section { 0.45, { 1.0, 0.75 } },
    Section { 0.70, { 1.5, 0.75 } },
    Section { 1.01, { 2.0, 1.0 } }
};

/**
 * @brief Smoothly bring a quality toward a new value.
 * @param step Time elapsed in this step, in seconds.
 * @param seconds Approximate time for the change to unfold.
 */
[[nodiscard]] inline double glide(double step, double seconds)
{
    return 1.0 - std::exp(-step * k_pace / std::max(seconds, 1e-3));
}

/**
 * @struct Measures
 * @brief A reading of the volume's changing form and character.
 */
struct Measures {
    double variance { 0.0 };
    double broken { 0.0 };
    double occupancy { 0.0 };
    double roughness { 0.0 };
    double imbalance { 0.0 };
    double skew { 0.0 };
    double opening { 0.0 };
    double structure { 1.0 };
    size_t rung { 0 };
    uint64_t surveys { 0 };

    [[nodiscard]] double lost() const { return 1.0 - structure; }
};

/**
 * @struct Feel
 * @brief The shared pace and intensity of the image and sound.
 */
struct Feel {
    double tempo { 1.0 };
    double loudness { 0.0 };
};

/**
 * @struct Conduct
 * @brief Controls for the performance, including voice entrances and intensity.
 */
struct Conduct {
    std::atomic<float> pressure { 0.45F };
    std::atomic<bool> hushed { false };
    std::atomic<bool> breathing { true };
    std::atomic<bool> frozen { false };
    std::atomic<bool> auto_arrival { true };
    std::array<std::atomic<bool>, k_voices> awake {};
};

/**
 * @struct Palette
 * @brief Colours that shift as the volume changes.
 */
struct Palette {
    std::array<glm::vec3, 3> low {
        glm::vec3 { 0.06F, 0.09F, 0.16F },
        glm::vec3 { 0.19F, 0.07F, 0.10F },
        glm::vec3 { 0.10F, 0.13F, 0.05F }
    };
    std::array<glm::vec3, 3> high {
        glm::vec3 { 0.78F, 0.83F, 0.92F },
        glm::vec3 { 0.92F, 0.62F, 0.38F },
        glm::vec3 { 0.56F, 0.60F, 0.34F }
    };
};

/**
 * @struct Mode
 * @brief One of the forms the changing material can take.
 *
 * Shell, knot, combustion, and liquid each give the volume a different shape,
 * movement, and visual character.
 */
struct Mode {
    const char* name;
    Kinesis::SpatialField density;
    Kinesis::VectorField velocity;
    Kinesis::SpatialField temperature;
    float ring_scale;
    float blaze_rate;
    float fire;
    float smoke;
    Palette palette {};
    float emission { 0.85F };
    float viscosity { 0.00004F };
    float buoyancy_temperature { 2.2F };
    float buoyancy_density { 0.5F };
    glm::vec3 gravity { 0.0F, 1.0F, 0.0F };
    float spin { 0.0F };
    float temperature_dissipation { 0.9955F };
    float smoke_absorption { 6.2F };
    float smoke_threshold { 0.004F };
    float fire_absorption { 6.0F };
    float fire_emission { 5.4F };
    float fire_threshold { 0.05F };
    glm::vec3 fire_cool { 0.55F, 0.06F, 0.01F };
    glm::vec3 fire_hot { 1.00F, 0.90F, 0.58F };
    bool blaze_gated { false };
};

/**
 * @brief Create the four forms, in the order selected by N1 through N4.
 */
[[nodiscard]] std::vector<Mode> make_modes();

} // namespace Attrition
// NOLINTEND
