#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

/// @brief Per pixel delay map. Pass it in RenderConfig::additional_textures
///        under the name delayMap; the first additional texture binds at 2.
///        The red channel in [0, 1] chooses the layer: 0 is layer 0 and 1 is
///        the last layer in use, with neighbouring layers blended.
layout(set = 0, binding = 2) uniform sampler2D delayMap;

const uint MAX_LAYERS = 30u;

/// @brief layer_count: layers in use, at most MAX_LAYERS.
/// @brief mode: 0 use the map as is, 1 invert it.
/// @brief weights: not used.
layout(push_constant) uniform PC {
    uint layer_count;
    uint mode;
    float weights[MAX_LAYERS];
} pc;

void main()
{
    uint count = max(min(pc.layer_count, MAX_LAYERS), 1u);

    float d = clamp(texture(delayMap, fragTexCoord).r, 0.0, 1.0);
    if (pc.mode == 1u) {
        d = 1.0 - d;
    }

    float position = d * float(count - 1u);
    uint lower = uint(floor(position));
    uint upper = min(lower + 1u, count - 1u);

    vec4 a = texture(textureArray, vec3(fragTexCoord, float(lower)));
    vec4 b = texture(textureArray, vec3(fragTexCoord, float(upper)));

    outColor = mix(a, b, fract(position));
}
