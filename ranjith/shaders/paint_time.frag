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

void main()
{
    vec3 paint = texture(textureArray, vec3(fragTexCoord, 0.0)).rgb;
    float brightness = dot(paint, vec3(0.299, 0.587, 0.114));
    float depth = clamp(layerData[0].x, 0.0, 1.0);

    uint moments = min(pc.layer_count, MAX_LAYERS) - 1u;
    float pos = clamp(brightness * depth, 0.0, 1.0) * float(moments - 1u);
    uint lower = uint(floor(pos));
    uint upper = min(lower + 1u, moments - 1u);

    vec4 a = texture(textureArray, vec3(fragTexCoord, float(1u + lower)));
    vec4 b = texture(textureArray, vec3(fragTexCoord, float(1u + upper)));

    outColor = mix(a, b, fract(pos));
}
