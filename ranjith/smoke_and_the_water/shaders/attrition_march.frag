#version 460

layout(location = 0) in vec3 frag_position;
layout(location = 1) flat in vec3 eye;

layout(set = 1, binding = 0, std430) readonly buffer VolumeField {
    float field[];
};

layout(push_constant) uniform PC {
    uint width;
    uint height;
    uint depth;
    uint max_steps;
    float bounds_min_x;
    float bounds_min_y;
    float bounds_min_z;
    float step_scale;
    float cell_size_x;
    float cell_size_y;
    float cell_size_z;
    float density_scale;
    float cool_r;
    float cool_g;
    float cool_b;
    float absorption;
    float hot_r;
    float hot_g;
    float hot_b;
    float emission;
    float threshold;
    float pad0;
    float pad1;
    float pad2;
} pc;

layout(location = 0) out vec4 out_color;

uint cell_index(ivec3 cell, uvec3 dims)
{
    ivec3 c = clamp(cell, ivec3(0), ivec3(dims) - 1);
    return uint(c.z) * dims.x * dims.y + uint(c.y) * dims.x + uint(c.x);
}

float sample_field(vec3 p, uvec3 dims)
{
    vec3 base = floor(p - 0.5);
    vec3 f = p - 0.5 - base;
    ivec3 b = ivec3(base);

    float c000 = field[cell_index(b + ivec3(0, 0, 0), dims)];
    float c100 = field[cell_index(b + ivec3(1, 0, 0), dims)];
    float c010 = field[cell_index(b + ivec3(0, 1, 0), dims)];
    float c110 = field[cell_index(b + ivec3(1, 1, 0), dims)];
    float c001 = field[cell_index(b + ivec3(0, 0, 1), dims)];
    float c101 = field[cell_index(b + ivec3(1, 0, 1), dims)];
    float c011 = field[cell_index(b + ivec3(0, 1, 1), dims)];
    float c111 = field[cell_index(b + ivec3(1, 1, 1), dims)];

    float x00 = mix(c000, c100, f.x);
    float x10 = mix(c010, c110, f.x);
    float x01 = mix(c001, c101, f.x);
    float x11 = mix(c011, c111, f.x);
    return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}

vec3 environment(vec3 dir, vec3 tint)
{
    float up = dir.y * 0.5 + 0.5;
    vec3 sky = mix(vec3(0.01, 0.015, 0.03), tint, smoothstep(0.15, 0.95, up));
    float horizon = exp(-pow((dir.y + 0.05) * 7.0, 2.0));
    float softbox = pow(max(dot(dir, normalize(vec3(0.45, 0.8, 0.35))), 0.0), 40.0) * 2.4;
    float strip = pow(max(dot(dir, normalize(vec3(-0.7, 0.25, 0.6))), 0.0), 18.0) * 0.9;
    return sky + tint * horizon * 0.55 + vec3(softbox + strip);
}

void main()
{
    uvec3 dims = uvec3(pc.width, pc.height, pc.depth);
    vec3 cell = vec3(pc.cell_size_x, pc.cell_size_y, pc.cell_size_z);
    vec3 lo = vec3(pc.bounds_min_x, pc.bounds_min_y, pc.bounds_min_z);
    vec3 hi = lo + vec3(dims) * cell;

    vec3 direction = normalize(frag_position - eye);
    vec3 inv_dir = 1.0 / direction;

    vec3 t_lo = (lo - eye) * inv_dir;
    vec3 t_hi = (hi - eye) * inv_dir;
    vec3 t_near = min(t_lo, t_hi);
    vec3 t_far = max(t_lo, t_hi);

    float enter = max(max(t_near.x, t_near.y), max(t_near.z, 0.0));
    float exit = min(min(t_far.x, t_far.y), t_far.z);

    if (exit <= enter) {
        discard;
    }

    float step_length = min(min(cell.x, cell.y), cell.z) * pc.step_scale;
    float span = exit - enter;
    uint steps = min(pc.max_steps, uint(span / step_length) + 1u);

    vec3 cool = vec3(pc.cool_r, pc.cool_g, pc.cool_b);
    vec3 hot = vec3(pc.hot_r, pc.hot_g, pc.hot_b);

    if (pc.absorption < 0.0) {
        bool soft = pc.absorption < -1.5;
        float found = -1.0;
        float peak = 0.0;
        float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);

        for (uint i = 0u; i < steps; ++i) {
            float t = enter + (float(i) + jitter) * step_length;
            vec3 lattice = (eye + direction * t - lo) / cell;
            float scaled = sample_field(lattice, dims) * pc.density_scale;
            peak = max(peak, scaled);

            if (scaled > pc.threshold) {
                float near_t = t - step_length;
                float far_t = t;
                for (int k = 0; k < 8; ++k) {
                    float mid = 0.5 * (near_t + far_t);
                    vec3 inner = (eye + direction * mid - lo) / cell;
                    if (sample_field(inner, dims) * pc.density_scale > pc.threshold) {
                        far_t = mid;
                    } else {
                        near_t = mid;
                    }
                }
                found = far_t;
                break;
            }
        }

        if (found < 0.0) {
            if (soft) {
                float edge = smoothstep(pc.threshold * 0.4, pc.threshold, peak);
                if (edge > 0.0) {
                    out_color = vec4(mix(cool, hot * 0.4, 0.5) * pc.emission * 0.8, edge);
                    return;
                }
            }
            discard;
        }

        vec3 at = (eye + direction * found - lo) / cell;
        const float reach = 1.75;
        vec3 gradient = vec3(
            sample_field(at + vec3(reach, 0.0, 0.0), dims) - sample_field(at - vec3(reach, 0.0, 0.0), dims),
            sample_field(at + vec3(0.0, reach, 0.0), dims) - sample_field(at - vec3(0.0, reach, 0.0), dims),
            sample_field(at + vec3(0.0, 0.0, reach), dims) - sample_field(at - vec3(0.0, 0.0, reach), dims));

        vec3 normal = -normalize(gradient + vec3(1e-6));
        vec3 view = -direction;
        vec3 mirrored = reflect(direction, normal);

        float facing = max(dot(normal, view), 0.0);
        float fresnel = pow(1.0 - facing, 3.0);
        float diffuse = max(dot(normal, normalize(vec3(0.4, 0.8, 0.45))), 0.0);

        vec3 shine = environment(mirrored, hot);

        float film_phase = facing * 2.2 + dot(at, vec3(0.011, 0.017, 0.013)) + length(gradient) * 3.0;
        vec3 film = 0.5 + 0.5 * cos(6.28318 * (vec3(0.0, 0.33, 0.67) + film_phase));
        float region = smoothstep(0.35, 0.8, 0.5 + 0.5 * sin(dot(at, vec3(0.021, 0.013, 0.017))));
        shine = shine * mix(vec3(1.0), film * 1.3, 0.28 * region) + film * fresnel * 0.1 * region;
        vec3 body = mix(cool, cool * (0.7 + 0.6 * film), 0.35) * (0.25 + 0.75 * diffuse);
        vec3 colour = mix(body, shine, clamp(0.3 + 0.7 * fresnel, 0.0, 1.0)) * pc.emission;

        out_color = vec4(colour, 1.0);
        return;
    }

    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;

    for (uint i = 0u; i < steps; ++i) {
        float t = enter + (float(i) + 0.5) * step_length;
        vec3 world = eye + direction * t;
        vec3 lattice = (world - lo) / cell;

        float density = sample_field(lattice, dims) * pc.density_scale;

        if (density > pc.threshold) {
            float alpha = 1.0 - exp(-density * pc.absorption * step_length);
            vec3 emitted = mix(cool, hot, clamp(density, 0.0, 1.0)) * pc.emission * density;

            accumulated += transmittance * alpha * emitted;
            transmittance *= 1.0 - alpha;

            if (transmittance < 0.01) {
                break;
            }
        }
    }

    out_color = vec4(accumulated, 1.0 - transmittance);
}
