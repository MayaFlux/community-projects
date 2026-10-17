#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

/// @brief Two vec4 per layer: layerData[2 * i] is the params, layerData[2 * i + 1] the timing.
layout(set = 1, binding = 0) readonly buffer LayerData {
    vec4 layerData[];
};

const uint MAX_LAYERS = 30u;

layout(push_constant) uniform PC {
    uint layer_count;
    uint mode;
    float weights[MAX_LAYERS];
} pc;

/// @brief Draws each layer as a part placed by its params, in layer order, over the ones before.
/// @brief params: xy = centre in 0 to 1, z = scale, w = rotation in radians.
void main()
{
    vec4 accum = vec4(0.0);
    uint count = min(pc.layer_count, MAX_LAYERS);

    for (uint i = 0u; i < count; ++i) {
        if (pc.weights[i] <= 0.0) {
            continue;
        }

        vec4 params = layerData[2u * i];
        float scale = max(params.z, 0.0001);
        float c = cos(params.w);
        float s = sin(params.w);

        vec2 p = (fragTexCoord - params.xy) / scale;
        p = vec2(c * p.x + s * p.y, -s * p.x + c * p.y) + 0.5;

        if (any(lessThan(p, vec2(0.0))) || any(greaterThan(p, vec2(1.0)))) {
            continue;
        }

        vec4 part = texture(textureArray, vec3(p, float(i)));
        float a = part.a * pc.weights[i];
        accum = vec4(mix(accum.rgb, part.rgb, a), a + accum.a * (1.0 - a));
    }

    outColor = accum;
}
