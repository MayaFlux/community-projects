#version 460

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} transform;

layout(set = 1, binding = 0, std430) readonly buffer Drone0 { float drone_0[]; };
layout(set = 1, binding = 1, std430) readonly buffer Drone1 { float drone_1[]; };
layout(set = 1, binding = 2, std430) readonly buffer Drone2 { float drone_2[]; };
layout(set = 1, binding = 3, std430) readonly buffer Drone3 { float drone_3[]; };
layout(set = 1, binding = 4, std430) readonly buffer Drone4 { float drone_4[]; };

layout(push_constant) uniform PC {
    float time;
    float energy[5];
    float lift;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_position;

const int BLOCK = 512;
const float ASPECT = 1.7777778;
const float RELIEF = 1.6;
const float LIMIT = 1.7;
const float BASE = 0.3;
const float DOME = 3.0;
const float RIPPLE = 0.15;
const float LEVEL = 3.0;

#define TAP(buf, u) \
    mix(buf[int(fract(u) * float(BLOCK))], \
        buf[(int(fract(u) * float(BLOCK)) + 1) % BLOCK], \
        fract(fract(u) * float(BLOCK)))

float crag(vec2 uv) {
    float a = 1.0 - abs(sin(uv.x * 19.0 + sin(uv.y * 13.0) * 1.7));
    float b = 1.0 - abs(sin(uv.x * 41.0 - uv.y * 37.0));
    return 0.55 + 0.6 * a * (0.6 + 0.4 * b);
}

/**
 * @brief One feature of the ground: a mountain or a depth that can narrow into a spire.
 *
 * pos holds the two rates and two phases of the slow drift of its place, shape the rate and phase
 * of its slow morph between a swell (the wide radius) and a spire (the narrow radius, steeper and
 * taller, so the same mass stands on less ground). level is the voice's smoothed
 * energy; the feature spreads from a little over half its radius up to the whole of it as the
 * level rises, and its height follows the level. A mountain is a cusped peak, ridged by crags
 * while it is a swell and smooth as a spire. A depth is a pit with a flat floor and steep walls.
 */
float feature(vec2 uv, vec4 pos, vec2 shape, float wide, float narrow, float level, bool depth) {
    vec2 centre = 0.5 + 0.36 * vec2(sin(pc.time * pos.x + pos.z), sin(pc.time * pos.y + pos.w));
    float spire = smoothstep(0.55, 0.9, 0.5 + 0.5 * sin(pc.time * shape.x + shape.y));
    float radius = mix(wide, narrow, spire) * mix(0.55, 1.0, level);
    float gain = mix(1.0, 1.8, spire) * level * DOME;
    float d = length((uv - centre) * vec2(ASPECT, 1.0));

    if (depth) {
        return -(1.0 - smoothstep(mix(0.35, 0.2, spire) * radius, mix(0.9, 0.75, spire) * radius, d)) * gain * 1.3;
    }
    return exp(-mix(1.3, 2.4, spire) * pow(d / radius, 1.4)) * mix(crag(uv), 1.0, spire) * gain;
}

/**
 * @brief Point shader that raises the plane into mountains and spires and sinks it into depths.
 *
 * Five features, one per drone voice, drift slowly over the plane. Voices 0, 2 and 4 are
 * mountains, voices 1 and 3 are depths, and each morphs now and then between a broad swell and a
 * narrow tall spire. Heights follow the voices' smoothed energies from the push constants, which
 * rise over seconds, so the ground grows and sinks gradually as each voice enters. The old
 * plane-wide sum of the five blocks stays at a small fraction as the slow sweeping rumble. The features
 * are broad and round, low in Q, so each covers a wide area. lift, from 0 to 1, scales the rumble. The
 * height moves the point along the plane's normal before the geometry transform and brightens it.
 * Trial values.
 */
void main()
{
    float u = inPosition.x * 0.5 + 0.5;
    float v = inPosition.y * 0.5 + 0.5;
    vec2 uv = vec2(u, v);

    float s0 = TAP(drone_0, u + v * 0.31);
    float s1 = TAP(drone_1, u + v * 0.47);
    float s2 = TAP(drone_2, u - v * 0.23);
    float s3 = TAP(drone_3, u + v * 0.71);
    float s4 = TAP(drone_4, u - v * 0.53);

    float base = clamp((0.90 * s0 + 0.80 * s1 + 0.70 * s2 + 0.60 * s3 + 0.50 * s4) * 0.4, -1.0, 1.0) * BASE * pc.lift;

    float e0 = clamp(pc.energy[0] * LEVEL, 0.0, 1.0);
    float e1 = clamp(pc.energy[1] * LEVEL, 0.0, 1.0);
    float e2 = clamp(pc.energy[2] * LEVEL, 0.0, 1.0);
    float e3 = clamp(pc.energy[3] * LEVEL, 0.0, 1.0);
    float e4 = clamp(pc.energy[4] * LEVEL, 0.0, 1.0);

    float local = 0.0;
    local += feature(uv, vec4(0.071, 0.053, 0.0, 1.3), vec2(0.061, 0.4), 0.50, 0.16, e0, false);
    local += feature(uv, vec4(0.047, 0.083, 2.1, 0.4), vec2(0.043, 2.2), 0.40, 0.14, e1, true);
    local += feature(uv, vec4(0.059, 0.037, 4.2, 2.7), vec2(0.079, 4.1), 0.42, 0.13, e2, false);
    local += feature(uv, vec4(0.089, 0.061, 1.1, 5.0), vec2(0.053, 5.3), 0.34, 0.12, e3, true);
    local += feature(uv, vec4(0.043, 0.097, 3.3, 3.9), vec2(0.067, 1.0), 0.32, 0.11, e4, false);
    local += RIPPLE * (e0 * s0 + e2 * s2 + e4 * s4 - e1 * abs(s1) - e3 * abs(s3));

    float h = clamp(base + local, -LIMIT, LIMIT) * RELIEF;

    vec3 displaced = vec3(inPosition.xy, inPosition.z + h);

    vec4 world_pos = transform.geometry * vec4(displaced, 1.0);
    gl_Position = transform.projection * transform.view * world_pos;
    gl_PointSize = inSize;
    out_color = inColor * (1.0 + 3.0 * abs(h));
    out_position = world_pos.xyz;
}
