#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

layout(set = 1, binding = 0) readonly buffer LayerData {
    vec4 layerData[];
};

const uint MAX_LAYERS = 30u;

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

void main()
{
    float phase = layerData[0].x;
    vec2 p = fragTexCoord * vec2(3.0, 2.0);

    float n = value_noise(p + vec2(phase * 0.20, -phase * 0.13)) * 0.65
        + value_noise(p * 2.3 - vec2(phase * 0.07)) * 0.35;

    uint count = min(pc.layer_count, MAX_LAYERS);
    float pos = clamp((n - 0.25) / 0.5, 0.0, 0.999) * float(count);
    uint layer = min(uint(floor(pos)), count - 1u);

    if (layerData[2u * layer + 1u].w < 0.5) {
        layer = 0u;
    }

    float across = fract(pos);
    float seam = smoothstep(0.0, 0.05, across) * (1.0 - smoothstep(0.95, 1.0, across));

    vec3 colour = texture(textureArray, vec3(fragTexCoord, float(layer))).rgb;
    outColor = vec4(colour * mix(0.3, 1.0, seam), 1.0);
}
