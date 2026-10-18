// NOLINTBEGIN
#pragma once

#include "Attrition.hpp"

namespace Attrition {

/**
 * @struct Impulse
 * @brief A mark made by the pen as it moves through the volume.
 */
struct Impulse {
    glm::vec3 tip { 0.0F };
    glm::vec3 axis { 0.0F, 0.0F, 1.0F };
    float force { 0.0F };
    float reach { k_ring_radius };
};

/**
 * @struct Gesture
 * @brief The pen's movement and pressure as they shape the performance.
 */
struct Gesture {
    std::atomic<float> speed { 0.0F };
    std::atomic<float> acceleration { 0.0F };
    std::atomic<float> curvature { 0.0F };
    std::atomic<float> heading { 0.0F };
    std::atomic<float> straightness { 1.0F };
    std::atomic<float> turning { 0.0F };
    std::atomic<float> spread { 0.0F };
    std::atomic<float> path { 0.0F };
    std::atomic<float> press_rate { 0.0F };
    std::atomic<float> press_accel { 0.0F };
    std::atomic<float> hover_rate { 0.0F };
    std::atomic<float> rotation { 0.0F };
    std::atomic<float> pressure { 0.0F };
    std::atomic<float> lean_x { 0.0F };
    std::atomic<float> lean_y { 0.0F };
    std::atomic<uint32_t> cell { 0 };
    std::atomic<uint32_t> crossings { 0 };
    std::atomic<uint32_t> unique_cells { 0 };
};

/**
 * @class Hand
 * @brief The hand that brings touch and movement into the piece.
 *
 * The hand follows the connected tablet and turns its gestures into marks
 * that travel through the material.
 */
class Hand {
public:
    Hand() = default;
    Hand(const Hand&) = delete;
    Hand& operator=(const Hand&) = delete;

    /**
     * @brief Connect to a tablet and begin following its movement.
     * @return True when a tablet is available.
     */
    bool bind();

    [[nodiscard]] const Gesture& gesture() const { return m_gesture; }

    /** @brief Number of pen marks gathered so far. */
    [[nodiscard]] uint64_t written() const { return m_written.load(std::memory_order_acquire); }

    /**
     * @brief Gather new pen marks since the previous read.
     * @param cursor Position already reached; advanced to the latest mark.
     * @param out Receives the new marks.
     */
    void collect(uint64_t& cursor, std::vector<Impulse>& out) const;

private:
    void frame(Nodes::Input::TabletContext& ctx);
    void wrap();

    std::shared_ptr<Nodes::Input::TabletNode> m_node;
    std::unique_ptr<Kinesis::MotionChannel<glm::vec2>> m_pos;
    std::unique_ptr<Kinesis::MotionChannel<double>> m_press;
    std::unique_ptr<Kinesis::MotionChannel<glm::vec2>> m_tilt;
    std::unique_ptr<Kinesis::MotionChannel<double>> m_hover;
    std::unique_ptr<Kinesis::MotionChannel<double>> m_rotation;

    Gesture m_gesture;
    std::atomic<float> m_depth { 0.5F };
    std::chrono::steady_clock::time_point m_last;

    std::array<Impulse, k_queue> m_queue {};
    std::atomic<uint64_t> m_written { 0 };
};

/**
 * @class Mass
 * @brief The volume of smoke, light, and liquid at the centre of the work.
 */
class Mass {
public:
    Mass() = default;
    Mass(const Mass&) = delete;
    Mass& operator=(const Mass&) = delete;

    /**
     * @brief Prepare the changing volume for the visual work.
     * @param window Window in which the volume appears.
     */
    void build(const std::shared_ptr<Core::Window>& window);

    /**
     * @brief Give the volume a new form and visual character.
     * @param mode Form to enter.
     */
    void apply(const Mode& mode);

    /**
     * @brief Let the volume lose another measure of structure.
     * @param rung Stage of change to enter.
     */
    void step_down(size_t rung);

    /**
     * @brief Restore the structure removed during earlier stages.
     * @param rung Stage to return to.
     */
    void mend(size_t rung);

    /**
     * @brief Continue the movement of a form that turns over time.
     * @param mode Current form.
     */
    void turn(const Mode& mode);

    [[nodiscard]] const std::shared_ptr<Buffers::VolumeGridBuffer>& volume() const { return m_volume; }
    [[nodiscard]] const std::shared_ptr<Buffers::InfluxProcessor>& blaze() const { return m_blaze; }
    [[nodiscard]] const std::shared_ptr<Buffers::RaymarchBuffer>& march() const { return m_march; }
    [[nodiscard]] const std::shared_ptr<Buffers::RaymarchBuffer>& fire() const { return m_fire; }
    [[nodiscard]] const Kinesis::Lattice3D& lattice() const { return m_volume->get_lattice(); }

private:
    std::shared_ptr<Buffers::VolumeGridBuffer> m_volume;
    std::shared_ptr<Buffers::InfluxProcessor> m_blaze;
    std::shared_ptr<Buffers::RaymarchBuffer> m_march;
    std::shared_ptr<Buffers::RaymarchBuffer> m_fire;
    Buffers::VolumeGridBuffer::FlowStages m_stages;
    double m_tilt { 0.0 };
};

/**
 * @class Stirrer
 * @brief Carries each pen gesture into the volume, where its motion returns over time.
 */
class Stirrer {
public:
    /**
     * @brief Let the volume respond to touch, its own changes, and the current form.
     * @param mass Volume being shaped.
     * @param hand Source of touch and movement.
     * @param mode Current form.
     * @param measures Latest reading of the volume.
     * @param feel Shared pace and intensity.
     */
    void step(Mass& mass, const Hand& hand, const Mode& mode, const Measures& measures, const Feel& feel);

    /** @brief Clear the returning gestures when beginning a new form. */
    void reset(const Hand& hand);

private:
    struct Echo {
        uint64_t due { 0 };
        Impulse impulse;
    };

    void inject(Mass& mass, const Hand& hand, const Mode& mode, const Impulse& last) const;
    void apply(const Mass& mass, const Impulse& q, float strength);

    std::vector<glm::vec4> m_flow;
    std::deque<Echo> m_echoes;
    uint64_t m_cursor { 0 };
    uint64_t m_stirred { 0 };
    double m_answer { 0.0 };
};

} // namespace Attrition
// NOLINTEND
