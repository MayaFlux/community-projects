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

vec2 hash22(vec2 p)
{
    return vec2(hash21(p), hash21(p + 17.0));
}

float grain_scatter(vec2 uv, float density, float seed)
{
    vec2 p = uv * density;
    vec2 cell = floor(p);
    float nearest = 8.0;
    vec2 owner = cell;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 c = cell + vec2(float(x), float(y));
            float d = distance(p, c + hash22(c));
            if (d < nearest) {
                nearest = d;
                owner = c;
            }
        }
    }

    return hash21(owner + vec2(seed, seed * 0.37)) - 0.5;
}

void sort_values(inout float v[MAX_LAYERS], uint n)
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
}

void main()
{
    float r[MAX_LAYERS];
    float g[MAX_LAYERS];
    float b[MAX_LAYERS];
    uint n = min(pc.layer_count, MAX_LAYERS);

    for (uint i = 0u; i < n; ++i) {
        vec3 c = texture(textureArray, vec3(fragTexCoord, float(i))).rgb;
        r[i] = c.r;
        g[i] = c.g;
        b[i] = c.b;
    }

    sort_values(r, n);
    sort_values(g, n);
    sort_values(b, n);

    vec4 knobs = layerData[0];
    float scatter = grain_scatter(fragTexCoord, 90.0, floor(knobs.z * 8.0) * 7.31);

    float pick = clamp(knobs.x + scatter * knobs.y, 0.0, 1.0) * float(n - 1u);
    uint lower = uint(floor(pick));
    uint upper = min(lower + 1u, n - 1u);
    float f = fract(pick);

    outColor = vec4(
            mix(r[lower], r[upper], f),
            mix(g[lower], g[upper], f),
            mix(b[lower], b[upper], f),
            1.0);
}
