#include "Culture.hpp"

#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"

#include <execution>

// NOLINTBEGIN
namespace Culture::Fields {

using namespace MayaFlux::ShaderCompat;

const auto organelle_tremor = MF_FIELD_T(organelle_tremor, [](vec3 p, float t) -> vec3 {
    return vec3(
               sin(p.y * 11.0f + t * 2.3f),
               sin(p.z * 10.0f - t * 2.1f),
               sin(p.x * 12.0f + t * 1.9f))
        * 0.008f;
});

const auto membrane_boil = MF_FIELD_T(membrane_boil, [](vec3 p, float t) -> vec3 {
    vec3 q = p * 3.4f;
    return vec3(
               sin(q.y + t * 1.3f) + cos(q.z - t * 1.1f),
               sin(q.z + t * 1.2f) + cos(q.x - t * 0.9f),
               sin(q.x - t * 1.4f) + cos(q.y + t * 1.0f))
        * 0.02f;
});

const auto drift_sway_a = MF_FIELD_T(drift_sway_a, [](vec3 p, float t) -> vec3 {
    return vec3(
        sin(t * 0.31f + p.x * 0.15f) * 0.26f,
        sin(t * 0.19f + 1.3f + p.y * 0.1f) * 0.10f,
        cos(t * 0.27f + p.z * 0.15f) * 0.24f);
});
const auto drift_sway_b = MF_FIELD_T(drift_sway_b, [](vec3 p, float t) -> vec3 {
    return vec3(
        cos(t * 0.23f + 2.1f + p.x * 0.12f) * 0.30f,
        sin(t * 0.29f + p.y * 0.1f) * 0.10f,
        sin(t * 0.20f + 0.6f + p.z * 0.12f) * 0.30f);
});
const auto drift_sway_c = MF_FIELD_T(drift_sway_c, [](vec3 p, float t) -> vec3 {
    return vec3(
        sin(t * 0.17f + 0.9f + p.x * 0.13f) * 0.22f,
        cos(t * 0.33f + p.y * 0.1f) * 0.14f,
        cos(t * 0.24f + 1.8f + p.z * 0.13f) * 0.26f);
});

const auto ripple_a = MF_FIELD_T(ripple_a, [](vec3 p, float t) -> vec3 {
    float w = sin(p.x * 2.6f - t * 3.4f);
    return vec3(0.0f, w * 0.16f, w * 0.05f);
});
const auto ripple_b = MF_FIELD_T(ripple_b, [](vec3 p, float t) -> vec3 {
    float w = sin(p.z * 2.9f + t * 3.0f);
    return vec3(w * 0.15f, w * 0.05f, 0.0f);
});
const auto ripple_c = MF_FIELD_T(ripple_c, [](vec3 p, float t) -> vec3 {
    float w = sin((p.x + p.z) * 2.2f - t * 3.8f);
    return vec3(w * 0.10f, w * 0.12f, w * -0.06f);
});

}
// NOLINTEND

namespace Culture {

using namespace MayaFlux;
using Kinesis::FieldTarget;
using Nodes::Network::GpuFieldOperator;
using Nodes::Network::PathOperator;

namespace {

    constexpr size_t count = 24;
    constexpr float reach = 6.4F;
    constexpr float min_gap = 2.4F;
    constexpr float dt = 1.0F / 60.0F;
    constexpr float tau = 6.28318F;
    constexpr float richness = 1.0F;

    struct Shape {
        float core { 0.4F };
        float squash { 1.0F };
        float length { 1.0F };
        float taper { 0.5F };
        float curl { 0.0F };
        float wave { 0.0F };
        float fiber { 0.0F };
        float pulse { 0.06F };
    };

    struct Limb {
        glm::vec3 dir { 0.0F, 1.0F, 0.0F };
        glm::vec3 bend { 1.0F, 0.0F, 0.0F };
        float scale { 1.0F };
        float grow { 1.0F };
        float grow_to { 1.0F };
        float grow_rate { 0.0F };
    };

    struct Organism {
        std::vector<Limb> limb;
        std::array<glm::vec3, 3> basis {};
        glm::vec3 center { 0.0F };
        glm::vec3 spin_axis { 0.0F, 1.0F, 0.0F };
        std::array<size_t, 3> fields { 0, 1, 0 };
        std::array<float, 3> palette { 0.5F, 0.5F, 0.8F };
        Shape base;
        Shape from;
        Shape to;
        Shape now;
        size_t type { 0 };
        size_t segments { 6 };
        size_t strands { 1 };
        size_t yarn { 200 };
        size_t field_count { 2 };
        float morph_start { 0.0F };
        float morph_length { 1.0F };
        float next_mutation { 4.0F };
        float spin { 0.1F };
        float phase { 0.0F };
        float wave_rate { 0.4F };
        float pulse_rate { 0.3F };
        float hue_drift { 0.0F };
        float thickness { 0.14F };
        float twist { 3.0F };
        float pressure { 0.0F };
        uint32_t seen_hits { 0 };
        uint32_t hit_form { 0 };
    };

    struct Rng {
        uint32_t state { 29U };

        float uniform(float low, float high)
        {
            state = Kinesis::Discrete::xorshift32(state);
            return low + (high - low) * static_cast<float>(static_cast<double>(state) / 4294967296.0);
        }

        glm::vec3 direction()
        {
            const glm::vec3 d(uniform(-1.0F, 1.0F), uniform(-1.0F, 1.0F), uniform(-1.0F, 1.0F));
            return glm::length(d) < 1e-3F ? glm::vec3(1.0F, 0.0F, 0.0F) : glm::normalize(d);
        }
    };

    struct Colony {
        std::vector<Organism> organism;
        std::vector<std::shared_ptr<Nodes::GpuSync::PathGeneratorNode>> node;
        std::vector<size_t> position;
        std::shared_ptr<Link> link;
        Rng rng;
        float time { 0.0F };
        uint64_t frame { 0 };
    };

    const std::array<const Kinesis::TemporalVectorField*, 8> pool {
        &Fields::ripple_a, &Fields::ripple_b, &Fields::ripple_c,
        &Fields::drift_sway_a, &Fields::drift_sway_b, &Fields::drift_sway_c,
        &Fields::organelle_tremor, &Fields::membrane_boil
    };

    constexpr std::array<std::array<float, 3>, 8> palettes { {
        { 0.50F, 0.55F, 0.80F },
        { 0.09F, 0.60F, 0.84F },
        { 0.58F, 0.38F, 0.76F },
        { 0.02F, 0.50F, 0.80F },
        { 0.27F, 0.45F, 0.70F },
        { 0.75F, 0.30F, 0.74F },
        { 0.13F, 0.20F, 0.86F },
        { 0.95F, 0.40F, 0.76F },
    } };

    struct Texture {
        uint32_t colour;
        float decay;
        float beat;
    };

    constexpr std::array<Texture, 6> textures { {
        { .colour = 9, .decay = 0.8F, .beat = 1.2F },
        { .colour = 3, .decay = 1.8F, .beat = 0.5F },
        { .colour = 13, .decay = 0.5F, .beat = 2.0F },
        { .colour = 5, .decay = 1.4F, .beat = 0.8F },
        { .colour = 8, .decay = 0.7F, .beat = 1.5F },
        { .colour = 11, .decay = 1.1F, .beat = 1.0F },
    } };

    glm::vec3 hsv(float h, float s, float v)
    {
        h -= std::floor(h);
        const float k = h * 6.0F;
        const float c = v * s;
        const float x = c * (1.0F - std::abs(std::fmod(k, 2.0F) - 1.0F));
        const float m = v - c;
        glm::vec3 rgb;
        switch (static_cast<int>(k) % 6) {
        case 0:
            rgb = { c, x, 0.0F };
            break;
        case 1:
            rgb = { x, c, 0.0F };
            break;
        case 2:
            rgb = { 0.0F, c, x };
            break;
        case 3:
            rgb = { 0.0F, x, c };
            break;
        case 4:
            rgb = { x, 0.0F, c };
            break;
        default:
            rgb = { c, 0.0F, x };
            break;
        }
        return rgb + glm::vec3(m);
    }

    glm::vec3 rotate(const glm::vec3& v, const glm::vec3& axis, float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return v * c + glm::cross(axis, v) * s + axis * (glm::dot(axis, v) * (1.0F - c));
    }

    Shape mix(const Shape& a, const Shape& b, float t)
    {
        return {
            .core = glm::mix(a.core, b.core, t),
            .squash = glm::mix(a.squash, b.squash, t),
            .length = glm::mix(a.length, b.length, t),
            .taper = glm::mix(a.taper, b.taper, t),
            .curl = glm::mix(a.curl, b.curl, t),
            .wave = glm::mix(a.wave, b.wave, t),
            .fiber = glm::mix(a.fiber, b.fiber, t),
            .pulse = glm::mix(a.pulse, b.pulse, t),
        };
    }

    glm::vec3 limb_direction(size_t type, size_t k, size_t total)
    {
        const float f = (static_cast<float>(k) + 0.5F) / static_cast<float>(total);
        const float angle = static_cast<float>(k) * 2.39996F;
        float y = 1.0F - 2.0F * f;

        switch (type) {
        case 1:
        case 3:
            y = -(0.15F + 0.85F * f);
            break;
        case 4:
            y = 0.5F * (f - 0.5F);
            break;
        case 5:
            y = (k % 2 == 0 ? 1.0F : -1.0F) * (0.75F + 0.25F * f);
            break;
        default:
            break;
        }

        const float ring = std::sqrt(std::max(0.0F, 1.0F - y * y));
        return { std::cos(angle) * ring, y, std::sin(angle) * ring };
    }

    float measure(const Shape& s)
    {
        return s.core + 0.5F * s.length;
    }

    size_t stroke_size(const Organism& o)
    {
        return o.yarn + o.limb.size() * o.strands * o.segments;
    }

    void order_by_neighbourhood(std::vector<Limb>& limb)
    {
        const auto rings = static_cast<float>(std::max<size_t>(2, static_cast<size_t>(std::sqrt(static_cast<float>(limb.size())))));
        const auto band = [&](const Limb& l) { return std::min(rings - 1.0F, std::floor((l.dir.y * 0.5F + 0.5F) * rings)); };
        std::ranges::sort(limb, [&](const Limb& a, const Limb& b) {
            const float ba = band(a);
            const float bb = band(b);
            if (ba != bb) {
                return ba < bb;
            }
            const float aa = std::atan2(a.dir.z, a.dir.x);
            const float ab = std::atan2(b.dir.z, b.dir.x);
            return static_cast<int>(ba) % 2 == 0 ? aa < ab : aa > ab;
        });
    }

    Organism make_organism(Rng& rng, size_t id, const glm::vec3& center, float size)
    {
        const size_t type = id % 6;
        Organism o;
        o.center = center;
        o.type = type;

        struct Plan {
            size_t limbs;
            size_t strands;
            size_t segments;
            size_t yarn;
            float line;
            Shape shape;
        };
        const std::array<Plan, 6> plans { {
            { .limbs = 600, .strands = 1, .segments = 8, .yarn = 5000, .line = 0.8F, .shape = { .core = 0.36F, .squash = 1.0F, .length = 0.6F, .taper = 0.4F, .curl = 0.0F, .wave = 0.03F, .fiber = 0.0F, .pulse = 0.05F } },
            { .limbs = 8, .strands = 60, .segments = 24, .yarn = 5000, .line = 0.7F, .shape = { .core = 0.42F, .squash = 0.85F, .length = 1.7F, .taper = 0.65F, .curl = 2.4F, .wave = 0.15F, .fiber = 0.12F, .pulse = 0.08F } },
            { .limbs = 2000, .strands = 1, .segments = 6, .yarn = 6000, .line = 0.6F, .shape = { .core = 0.30F, .squash = 1.0F, .length = 0.95F, .taper = 0.85F, .curl = 0.0F, .wave = 0.0F, .fiber = 0.0F, .pulse = 0.04F } },
            { .limbs = 60, .strands = 8, .segments = 24, .yarn = 6000, .line = 0.7F, .shape = { .core = 0.45F, .squash = 0.55F, .length = 1.5F, .taper = 0.5F, .curl = 0.5F, .wave = 0.3F, .fiber = 0.05F, .pulse = 0.14F } },
            { .limbs = 500, .strands = 1, .segments = 10, .yarn = 6000, .line = 0.7F, .shape = { .core = 0.35F, .squash = 1.0F, .length = 0.95F, .taper = 0.5F, .curl = 0.45F, .wave = 0.05F, .fiber = 0.0F, .pulse = 0.06F } },
            { .limbs = 14, .strands = 24, .segments = 20, .yarn = 6000, .line = 0.8F, .shape = { .core = 0.32F, .squash = 1.4F, .length = 1.1F, .taper = 0.4F, .curl = -1.0F, .wave = 0.08F, .fiber = 0.08F, .pulse = 0.05F } },
        } };

        const Plan& plan = plans[type];
        const auto scaled = [](size_t n) { return std::max<size_t>(1, static_cast<size_t>(static_cast<float>(n) * richness)); };
        o.segments = plan.segments;
        o.strands = plan.strands;
        o.yarn = scaled(plan.yarn);
        o.base = plan.shape;
        o.base.core *= size;
        o.base.length *= size;
        o.base.fiber *= size;
        o.now = o.from = o.to = o.base;

        o.basis[0] = rng.direction();
        o.basis[1] = glm::normalize(glm::cross(o.basis[0], rng.direction()) + glm::vec3(1e-3F));
        o.basis[2] = glm::cross(o.basis[0], o.basis[1]);
        o.spin_axis = rng.direction();
        o.spin = rng.uniform(0.04F, 0.18F);
        o.phase = rng.uniform(0.0F, tau);
        o.wave_rate = rng.uniform(0.25F, 0.7F);
        o.pulse_rate = rng.uniform(0.2F, 0.55F);
        o.next_mutation = rng.uniform(12.0F, 30.0F);
        o.thickness = rng.uniform(0.14F, 0.22F) * plan.line;
        o.twist = rng.uniform(1.5F, 4.5F);

        const size_t limbs = plan.strands > 1 ? plan.limbs : scaled(plan.limbs);
        for (size_t k = 0; k < limbs; ++k) {
            Limb limb;
            limb.dir = limb_direction(type, k, limbs);
            const glm::vec3 reference = std::abs(limb.dir.y) > 0.9F ? glm::vec3(1.0F, 0.0F, 0.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
            const glm::vec3 first = glm::normalize(glm::cross(limb.dir, reference));
            limb.bend = rotate(first, limb.dir, rng.uniform(0.0F, tau));
            limb.scale = rng.uniform(0.7F, 1.3F);
            o.limb.push_back(limb);
        }
        order_by_neighbourhood(o.limb);

        o.palette = palettes[(id * 3) % palettes.size()];
        o.palette[0] += rng.uniform(-0.02F, 0.02F);

        const size_t first = (id * 3) % pool.size();
        const size_t second = (id * 3 + 1 + id / 8) % pool.size();
        o.fields = { first, second == first ? (second + 1) % pool.size() : second, 0 };
        o.field_count = 2;
        return o;
    }

    std::vector<Kakshya::LineVertex> stroke(const Organism& o, float time)
    {
        std::vector<Kakshya::LineVertex> out;
        out.reserve(stroke_size(o));

        const float angle = o.spin * time;
        std::array<glm::vec3, 3> axes {};
        for (size_t i = 0; i < 3; ++i) {
            axes[i] = rotate(o.basis[i], o.spin_axis, angle);
        }

        const Shape& s = o.now;
        const float beat = 1.0F + s.pulse * std::sin(tau * o.pulse_rate * time + o.phase);
        const float radius = s.core * beat;
        const float hue = o.palette[0] + o.hue_drift;
        const float sat = o.palette[1];
        const float val = o.palette[2];
        const glm::vec3 squash(1.0F, s.squash, 1.0F);

        const auto add = [&](const glm::vec3& local, const glm::vec3& colour, float thick) {
            Kakshya::LineVertex v;
            v.position = o.center + axes[0] * local.x + axes[1] * local.y + axes[2] * local.z;
            v.color = glm::clamp(colour, glm::vec3(0.0F), glm::vec3(1.0F));
            v.thickness = thick;
            out.push_back(v);
        };

        for (size_t i = 0; i < o.yarn; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(o.yarn - 1);
            const float phi = std::acos(1.0F - 2.0F * t);
            const float theta = static_cast<float>(o.yarn) / 12.0F * tau * t + o.phase;
            const float depth = 0.72F + 0.28F * std::sin(5.0F * tau * t + o.phase);
            const glm::vec3 p(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
            add(p * (radius * depth) * squash, hsv(hue, sat * 0.8F, std::min(1.0F, val * (0.55F + 0.45F * depth))), o.thickness);
        }

        const std::array<glm::vec3, 3> sprout { glm::vec3(1.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F), glm::vec3(0.0F, 0.0F, 1.0F) };
        std::vector<glm::vec3> centre(o.segments);
        std::vector<glm::vec3> n1(o.segments);
        std::vector<glm::vec3> n2(o.segments);
        bool outward = true;

        for (const Limb& limb : o.limb) {
            const glm::vec3 surface = limb.dir * radius * squash;
            const float length = s.length * limb.scale * limb.grow;
            const glm::vec3 side = glm::cross(limb.dir, limb.bend);

            for (size_t j = 0; j < o.segments; ++j) {
                const float u = static_cast<float>(j) / static_cast<float>(o.segments - 1);
                glm::vec3 offset = limb.dir * (length * u);
                if (std::abs(s.curl) > 1e-3F) {
                    const float theta = s.curl * u;
                    offset = (limb.dir * std::sin(theta) + limb.bend * (1.0F - std::cos(theta))) * (length / s.curl);
                }
                const float ripple = std::sin(u * 6.0F - tau * o.wave_rate * time + o.phase);
                centre[j] = surface + offset + side * (s.wave * length * u * ripple);
            }
            for (size_t j = 0; j < o.segments; ++j) {
                const glm::vec3 tangent = glm::normalize(centre[std::min(j + 1, o.segments - 1)] - centre[j == 0 ? 0 : j - 1] + glm::vec3(1e-5F));
                n1[j] = glm::normalize(glm::cross(tangent, sprout[j % 3] + glm::vec3(0.2F, 0.5F, 0.3F)) + glm::vec3(1e-4F));
                n2[j] = glm::cross(tangent, n1[j]);
            }

            for (size_t b = 0; b < o.strands; ++b) {
                const float around = static_cast<float>(b) * 2.39996F;
                const float within = 0.35F + 0.65F * std::fmod(static_cast<float>(b) * 0.61803F, 1.0F);
                const float fibre = 0.9F + 0.2F * std::sin(static_cast<float>(b) * 2.3F);
                for (size_t step = 0; step < o.segments; ++step) {
                    const size_t j = outward ? step : o.segments - 1 - step;
                    const float u = static_cast<float>(j) / static_cast<float>(o.segments - 1);
                    glm::vec3 p = centre[j];
                    if (o.strands > 1) {
                        const float spiral = around + o.twist * u;
                        p += (n1[j] * std::cos(spiral) + n2[j] * std::sin(spiral)) * (s.fiber * within * (1.0F - 0.8F * s.taper * u) * (limb.grow * 0.8F + 0.2F));
                    }
                    const glm::vec3 colour = hsv(hue + 0.06F * u, sat, val * (1.0F - 0.6F * u) * fibre);
                    add(p, colour, o.thickness * (1.0F - 0.6F * s.taper * u));
                }
                outward = !outward;
            }
        }
        return out;
    }

    void evolve(Colony& colony, Organism& o, uint32_t form)
    {
        Rng& rng = colony.rng;
        o.from = o.now;
        o.to = o.base;

        switch (form) {
        case 0:
            o.to.length *= rng.uniform(1.1F, 1.4F);
            o.to.fiber *= rng.uniform(1.0F, 1.4F);
            break;
        case 1:
            o.to.curl = -o.now.curl * rng.uniform(0.8F, 1.2F) + rng.uniform(-0.3F, 0.3F);
            break;
        case 2:
            o.to.squash = std::clamp(2.0F - o.now.squash, 0.4F, 1.6F);
            o.to.wave *= rng.uniform(1.2F, 1.8F);
            break;
        default:
            o.to.core *= rng.uniform(1.1F, 1.25F);
            o.to.length *= rng.uniform(0.65F, 0.9F);
            break;
        }
        o.to.curl += rng.uniform(-0.3F, 0.3F);
        o.to.pulse *= rng.uniform(0.8F, 1.3F);

        o.morph_start = colony.time;
        o.morph_length = rng.uniform(5.0F, 9.0F);
        o.next_mutation = colony.time + o.morph_length + rng.uniform(20.0F, 40.0F);
        o.pressure = 0.0F;

        const size_t first = static_cast<size_t>(rng.uniform(0.0F, 0.999F) * static_cast<float>(o.limb.size()));
        const size_t patch = std::max<size_t>(1, static_cast<size_t>(rng.uniform(0.03F, 0.10F) * static_cast<float>(o.limb.size())));
        const bool retract = o.limb[first].grow_to > 0.5F && rng.uniform(0.0F, 1.0F) < 0.6F;
        const float rate = retract ? rng.uniform(0.12F, 0.3F) : rng.uniform(0.08F, 0.2F);
        for (size_t n = 0; n < patch; ++n) {
            Limb& limb = o.limb[(first + n) % o.limb.size()];
            limb.grow_to = retract ? 0.0F : 1.0F;
            limb.grow_rate = rate;
        }
    }

    void advance(Colony& colony)
    {
        colony.time += dt;
        ++colony.frame;

        const float drift = colony.link ? colony.link->drift.load(std::memory_order_relaxed) : 0.0F;
        size_t restless = 0;

        for (size_t i = 0; i < colony.organism.size(); ++i) {
            Organism& o = colony.organism[i];
            const bool settled = colony.time >= o.morph_start + o.morph_length;

            if (colony.link) {
                auto& bond = colony.link->bond[colony.position[i]];
                const uint32_t hits = bond.hits.load(std::memory_order_relaxed);
                if (hits != o.seen_hits) {
                    const float strength = bond.hit_strength.load(std::memory_order_relaxed);
                    const auto times = static_cast<float>(std::min<uint32_t>(hits - o.seen_hits, 3U));
                    o.seen_hits = hits;
                    o.pressure += strength * times * 0.12F * (0.6F + drift);
                    o.hit_form = bond.form.load(std::memory_order_relaxed);
                }
            }

            if (settled && (o.pressure >= 1.0F || colony.time >= o.next_mutation)) {
                evolve(colony, o, o.hit_form);
            }
            if (!settled) {
                ++restless;
            }

            const float t = std::clamp((colony.time - o.morph_start) / o.morph_length, 0.0F, 1.0F);
            o.now = mix(o.from, o.to, glm::smoothstep(0.0F, 1.0F, t));
            o.hue_drift = 0.04F * std::sin(0.05F * colony.time + o.phase);

            for (Limb& limb : o.limb) {
                const float step = limb.grow_rate * dt;
                limb.grow += std::clamp(limb.grow_to - limb.grow, -step, step);
            }
        }

        if (colony.link) {
            const float fraction = static_cast<float>(restless) / static_cast<float>(colony.organism.size());
            colony.link->tempo.store(std::clamp(1.0F + 0.05F * (fraction - 0.3F), 0.97F, 1.04F), std::memory_order_relaxed);
        }

        std::vector<size_t> due;
        for (size_t i = 0; i < colony.organism.size(); ++i) {
            if ((colony.frame + i) % 2 == 0) {
                due.push_back(i);
            }
        }

        std::vector<std::vector<Kakshya::LineVertex>> built(due.size());
        std::transform(std::execution::par_unseq, due.begin(), due.end(), built.begin(),
            [&](size_t i) { return stroke(colony.organism[i], colony.time); });

        for (size_t k = 0; k < due.size(); ++k) {
            colony.node[due[k]]->set_control_points(built[k]);
        }
    }

    std::vector<glm::vec3> place(Rng& rng, const glm::vec3& middle)
    {
        std::vector<glm::vec3> out;
        float gap = min_gap;
        for (size_t i = 0; i < count; ++i) {
            glm::vec3 spot(0.0F);
            for (int attempt = 0;; ++attempt) {
                const float radius = reach * std::pow(rng.uniform(0.0F, 1.0F), 1.6F);
                spot = middle + rng.direction() * radius * glm::vec3(1.0F, 0.75F, 1.0F);
                const bool clear = std::ranges::none_of(out, [&](const glm::vec3& other) {
                    return glm::distance(spot, other) < gap;
                });
                if (clear) {
                    break;
                }
                if (attempt > 200) {
                    gap *= 0.92F;
                    attempt = 0;
                }
            }
            out.push_back(spot);
        }
        return out;
    }

}

void organisms(std::shared_ptr<Link> link)
{
    auto window = MayaFlux::create_window({ "Culture - organisms", 1600, 900 });
    window->show();

    const glm::vec3 middle(0.6F, -0.4F, 0.3F);
    auto colony = make_persistent_shared<Colony>();
    colony->link = std::move(link);
    const auto spots = place(colony->rng, middle);
    for (size_t i = 0; i < count; ++i) {
        const float size = glm::mix(1.35F, 0.7F, std::min(1.0F, glm::distance(spots[i], middle) / reach));
        colony->organism.push_back(make_organism(colony->rng, i, spots[i], size));
    }

    std::vector<size_t> ranked(count);
    std::iota(ranked.begin(), ranked.end(), size_t { 0 });
    std::ranges::sort(ranked, [&](size_t a, size_t b) {
        return measure(colony->organism[a].base) > measure(colony->organism[b].base);
    });
    colony->position.assign(count, 0);
    for (size_t rank = 0; rank < count; ++rank) {
        colony->position[ranked[rank]] = rank * (k_positions - 1) / (count - 1);
    }

    if (colony->link) {
        for (size_t i = 0; i < count; ++i) {
            auto& bond = colony->link->bond[colony->position[i]];
            const Texture& texture = textures[colony->organism[i].type];
            bond.colour.store(texture.colour, std::memory_order_relaxed);
            bond.decay.store(texture.decay, std::memory_order_relaxed);
            bond.beat.store(texture.beat, std::memory_order_relaxed);
        }
    }

    auto cloud = vega.PointCloudNetwork() | Graphics;
    auto paths = cloud->create_operator<PathOperator>(Kinesis::InterpolationMode::LINEAR, 2);

    for (size_t i = 0; i < count; ++i) {
        paths->add_path(stroke(colony->organism[i], 0.0F), Kinesis::InterpolationMode::LINEAR, 2, stroke_size(colony->organism[i]));
        colony->node.push_back(paths->get_path(i));
        colony->node.back()->set_path_thickness(colony->organism[i].thickness);
    }

    auto field = cloud->get_operator_chain()->emplace<GpuFieldOperator>(Kakshya::VertexLayout::for_lines());
    for (size_t c = 0; c < count; ++c) {
        for (size_t k = 0; k < colony->organism[c].field_count; ++k) {
            field->bind(FieldTarget::POSITION, *pool[colony->organism[c].fields[k]], static_cast<uint32_t>(c));
        }
    }

    auto geom_buf = vega.NetworkGeometryBuffer(cloud) | Graphics;
    geom_buf->setup_rendering({
        .target_window = window,
        .topology = Portal::Graphics::PrimitiveTopology::LINE_STRIP,
    });

    bind_fly_preset(window, geom_buf->get_render_processor(),
        {
            .initial_eye = middle + glm::vec3(0.0F, 0.8F, 15.0F),
            .initial_target = middle,
        },
        {}, "Culture organisms");

    auto life = Kriya::pattern<uint64_t>(
        [](uint64_t i) { return i; },
        [colony](uint64_t) { advance(*colony); },
        std::vector<double> { 1.0 / 60.0 },
        Vruta::ProcessingToken::FRAME_ACCURATE);
    get_scheduler()->add_task(life, "culture_organisms", true);

    size_t lines = 0;
    for (const auto& o : colony->organism) {
        lines += stroke_size(o) - 1;
    }
    MF_LOG(Journal::Component::USER, Journal::Context::UserCode,
        "Culture organisms: {} bodies, {} line segments in total ({} per body on average)",
        count, lines, lines / count);
}

}
