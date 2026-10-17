#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

const uint MAX_LAYERS = 30u;

/// @brief layer_count: layers in use, at most MAX_LAYERS.
/// @brief mode: 0 summed absolute difference of neighbouring layers per channel,
///        1 the same reduced to luminance, 2 signed difference of layers 0 and 1
///        centred on grey.
/// @brief weights: scale of each difference, weight i for layers i and i + 1.
///        Modes 0 and 1 skip weights of zero or less. Mode 2 uses weight 0.
layout(push_constant) uniform PC {
    uint layer_count;
    uint mode;
    float weights[MAX_LAYERS];
} pc;

void main()
{
    uint count = min(pc.layer_count, MAX_LAYERS);

    if (count < 2u) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    if (pc.mode == 2u) {
        vec4 a = texture(textureArray, vec3(fragTexCoord, 0.0));
        vec4 b = texture(textureArray, vec3(fragTexCoord, 1.0));
        outColor = vec4(clamp(0.5 + (a.rgb - b.rgb) * pc.weights[0], 0.0, 1.0), 1.0);
        return;
    }

    vec3 accum = vec3(0.0);
    vec4 previous = texture(textureArray, vec3(fragTexCoord, 0.0));

    for (uint i = 1u; i < count; ++i) {
        vec4 current = texture(textureArray, vec3(fragTexCoord, float(i)));
        float w = pc.weights[i - 1u];
        if (w > 0.0) {
            accum += abs(previous.rgb - current.rgb) * w;
        }
        previous = current;
    }

    if (pc.mode == 1u) {
        accum = vec3(dot(accum, vec3(0.299, 0.587, 0.114)));
    }

    outColor = vec4(clamp(accum, 0.0, 1.0), 1.0);
}
