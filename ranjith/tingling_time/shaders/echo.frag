#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

/// @brief Two vec4 per layer: layerData[2 * i] is the params, layerData[2 * i + 1] the timing.
layout(set = 1, binding = 0) readonly buffer LayerData {
    vec4 layerData[];
};

const uint MAX_LAYERS = 30u;
const vec2 ASPECT = vec2(1.7778, 1.0);

layout(push_constant) uniform PC {
    uint layer_count;
    uint mode;
    float weights[MAX_LAYERS];
} pc;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float value_noise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(cell), hash21(cell + vec2(1.0, 0.0)), f.x),
        mix(hash21(cell + vec2(0.0, 1.0)), hash21(cell + vec2(1.0, 1.0)), f.x),
        f.y);
}

float field(vec2 p)
{
    return value_noise(p) * 0.65 + value_noise(p * 2.3 + 7.1) * 0.35;
}

/// @brief Layer zero is the live picture. Every later layer claims the region where a noise field,
/// @brief bent by the live brightness, lies below its level, and shows its own moment there.
/// @brief params: xy = offset of the field, z = cells across the picture.
/// @brief timing: x = seconds since the layer entered, w = 1 once fed.
/// @brief The seam is darkened and the moment is pushed along the field's own slope.
void main()
{
    vec3 col = texture(textureArray, vec3(fragTexCoord, 0.0)).rgb;
    float luma = dot(col, vec3(0.299, 0.587, 0.114));
    uint count = min(pc.layer_count, MAX_LAYERS);

    for (uint i = 1u; i < count; ++i) {
        float level = clamp(pc.weights[i], 0.0, 1.0);
        vec4 timing = layerData[2u * i + 1u];
        if (timing.w < 0.5 || level <= 0.002) {
            continue;
        }

        vec4 params = layerData[2u * i];
        vec2 p = fragTexCoord * ASPECT * params.z + params.xy + vec2(timing.x * 0.05, -timing.x * 0.03);

        float f = field(p) + (luma - 0.5) * 0.4;
        float threshold = mix(0.1, 0.9, level);
        float inside = 1.0 - smoothstep(threshold - 0.006, threshold + 0.006, f);

        vec2 push = vec2(field(p + 3.7) - 0.5, field(p + 11.3) - 0.5) * 0.25 * level;
        vec3 moment = texture(textureArray, vec3(fragTexCoord + push, float(i))).rgb;

        float seam = smoothstep(0.0, 0.03, abs(f - threshold));
        col = mix(col, moment, inside);
        col *= mix(0.15, 1.0, seam);
    }

    outColor = vec4(col, 1.0);
}
