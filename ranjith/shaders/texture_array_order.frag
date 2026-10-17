#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

const uint MAX_LAYERS = 30u;

/// @brief layer_count: layers in use, at most MAX_LAYERS.
/// @brief mode: 0 per channel median, 1 minimum, 2 maximum.
/// @brief weights: a layer with a weight of zero or less is left out.
layout(push_constant) uniform PC {
    uint layer_count;
    uint mode;
    float weights[MAX_LAYERS];
} pc;

float order_statistic(inout float v[MAX_LAYERS], uint n, uint rank)
{
    for (uint i = 1u; i < n; ++i) {
        float key = v[i];
        int j = int(i) - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            --j;
        }
        v[j + 1] = key;
    }
    return v[rank];
}

void main()
{
    float r[MAX_LAYERS];
    float g[MAX_LAYERS];
    float b[MAX_LAYERS];
    float a[MAX_LAYERS];
    uint n = 0u;
    uint count = min(pc.layer_count, MAX_LAYERS);

    for (uint i = 0u; i < count; ++i) {
        if (pc.weights[i] <= 0.0) {
            continue;
        }

        vec4 c = texture(textureArray, vec3(fragTexCoord, float(i)));
        r[n] = c.r;
        g[n] = c.g;
        b[n] = c.b;
        a[n] = c.a;
        ++n;
    }

    if (n == 0u) {
        outColor = vec4(0.0);
        return;
    }

    uint rank = n / 2u;
    if (pc.mode == 1u) {
        rank = 0u;
    } else if (pc.mode == 2u) {
        rank = n - 1u;
    }

    outColor = vec4(
        order_statistic(r, n, rank),
        order_statistic(g, n, rank),
        order_statistic(b, n, rank),
        order_statistic(a, n, rank));
}
