#include "common.h"

[[vk::push_constant]]
PushData push_data;

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure tlas;

[[vk::binding(1, 0)]]
RWTexture2D<float4> out_image;

struct UniformBuffer {
    float4x4 inv_view;
    float4x4 inv_proj;
};

[vk::binding(2, 0)]
ConstantBuffer<UniformBuffer> uniform;

// Source: https://www.shadertoy.com/view/XtGGzG
float3 viridis_quintic(float x, float2 range) {
	x = saturate((x - range.x) / (range.y - range.x));
	float4 x1 = float4( 1.0, x, x * x, x * x * x ); // 1 x x2 x3
	float4 x2 = x1 * x1.w * x; // x4 x5 x6 x7
	return saturate(float3(
	    dot(x1.xyzw, float4(+0.280268003, -0.143510503, +2.225793877, -14.815088879 )) + dot(x2.xy, float2(+25.212752309, -11.772589584)),
	    dot(x1.xyzw, float4(-0.002117546, +1.617109353, -1.909305070, +2.701152864  )) + dot(x2.xy, float2(-1.685288385,  +0.178738871)),
	    dot(x1.xyzw, float4(+0.300805501, +2.614650302, -12.019139090, +28.933559110)) + dot(x2.xy, float2(-33.491294770, +13.762053843)))
	);
}

// Source: https://www.shadertoy.com/view/XtGGzG
float3 inferno_quintic(float x, float2 range) {
	x = saturate((x - range.x) / (range.y - range.x));
	float4 x1 = float4( 1.0, x, x * x, x * x * x ); // 1 x x2 x3
	float4 x2 = x1 * x1.w * x; // x4 x5 x6 x7
	return saturate(float3(
	    dot(x1.xyzw, float4(-0.027780558, +1.228188385, +0.278906882, +3.892783760)) + dot(x2.xy, float2(-8.490712758, +4.069046086)),
	    dot(x1.xyzw, float4(+0.014065206, +0.015360518, +1.605395918, -4.821108251)) + dot(x2.xy, float2(+8.389314011, -4.193858954)),
	    dot(x1.xyzw, float4(-0.019628385, +3.122510347, -5.893222355, +2.798380308)) + dot(x2.xy, float2(-3.608884658, +4.324996022)))
	);
}

float3 heatmap(RayDesc ray, float2 range) {
    Payload payload;
    uint64_t start = clockARB();
    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    uint64_t end = clockARB();

    uint64_t diff_raw = (end > start) ? (end - start) : 0;
    float diff = float(diff_raw);
    return inferno_quintic(diff, range);
}

// source: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
static uint rng_state;

uint rand_pcg() {
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random() {
    uint seed = rand_pcg();
    return float(seed) / 4294967296.0;
}

void orthonormal_basis(float3 normal, out float3 tangent, out float3 bitangent) {
    float3 up = abs(normal.z) < 1.0 - EPSILON ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);

    tangent = normalize(cross(up, normal));
    bitangent = cross(normal, tangent);
}

float3 random_uniform_hemisphere(float3 normal) {
    float z = random();
    float r = sqrt(1.0 - z * z);
    float phi = 2.0 * PI * random();

    float3 local_dir = float3(r * cos(phi), r * sin(phi), z);

    float3 tangent, bitangent;
    orthonormal_basis(normal, tangent, bitangent);

    float3 world_dir = local_dir.x * tangent + local_dir.y * bitangent + local_dir.z * normal;

    return normalize(world_dir);
}

float3 sample_cosine_hemisphere() {
    float r1 = random();
    float r2 = random();

    float phi = 2 * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    float z = sqrt(1 - r2);

    return float3(x, y, z);
}

float3 get_accumulated_color(float3 color) {
    if (push_data.frame_count == 1) {
        return color;
    }

    uint2 launch_id = DispatchRaysIndex().xy;

    float3 previous_color = out_image[int2(launch_id)].rgb;
    float3 accumulated_color = color;

    accumulated_color = lerp(previous_color, color, 1.0 / float(push_data.frame_count));

    return accumulated_color;
}

float3 get_debug_color(RayDesc ray) {
    float3 color = float3(0.0);

    if (push_data.render_settings[0].debug_channel == DebugChannel::Heatmap) {
        color = heatmap(ray, float2(10000.0, 100000.0));
    } else {
        Payload payload = {};
        payload.color = float3(0.0);

        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
        color = payload.color;
    }

    return color;
}

float3 get_sky_color(float3 dir) {
    dir = normalize(dir);

    float transition = pow(max(0.0, dir.y), 0.5); // Плавность перехода

    float3 horizon_color = float3(0.4, 0.6, 0.8);  // Нежно-голубой у горизонта
    float3 zenith_color = float3(0.05, 0.15, 0.4); // Глубокий синий в зените
    float3 ground_color = float3(0.1, 0.1, 0.1);   // Почти черный низ

    if (dir.y > 0) {
        return lerp(horizon_color, zenith_color, transition);
    } else {
        return lerp(horizon_color, ground_color, pow(max(0.0, -dir.y), 0.2));
    }
}

float3 path_trace(RayDesc ray, int max_depth) {
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            // radiance += throughput * float3(0.3, 0.6, 1.0) * 10.0;
            // radiance += throughput * get_sky_color(ray.Direction) * 10.0;
            // if (dot(ray.Direction, float3(0.0, 0.2, 1.0)) > 0.999) {
            //     radiance += throughput * float3(0.7, 0.4, 0.5) * 100.0;
            // }

            break;
        }

        if (any(payload.emission > 0.0)) {
            radiance += throughput * payload.emission * 40.0;
            break;
        }

        float3 next_dir = sample_cosine_hemisphere();

        if (dot(ray.Direction, payload.normal) > 0.0) {
            payload.normal = -payload.normal;
        }

        float3 tangent, bitangent;
        orthonormal_basis(payload.normal, tangent, bitangent);
        next_dir = next_dir.x * tangent + next_dir.y * bitangent + next_dir.z * payload.normal;
        next_dir = normalize(next_dir);

        float3 brdf = payload.color / PI;
        float cos_theta = max(dot(next_dir, payload.normal), 0.0);
        float pdf = cos_theta / PI;

        if (pdf < EPSILON) {
            break;
        }

        throughput *= brdf * cos_theta / pdf;

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;

        ray.Origin = hitpos + (payload.normal * EPSILON);
        ray.Direction = next_dir;
    }

    return radiance;
}

[shader("raygeneration")]
void main() {
    uint2 launch_id = DispatchRaysIndex().xy; // Current pixel coords (x,y)
    uint2 launch_size = DispatchRaysDimensions().xy; // Image size (width, height)

    uint pixel_index = launch_id.y * launch_size.x + launch_id.x;
    rng_state = pixel_index + push_data.frame_count * 1_000_000u;

    float2 uv = (float2(launch_id) + 0.5) / float2(launch_size) * 2.0 - 1.0; // NDC in range [-1, 1]

    // Anti-Aliasing
    uv.x += (random() - 0.5) * 0.002;
    uv.y += (random() - 0.5) * 0.002;

    RenderSettings render_settings = push_data.render_settings[0];

    if (push_data.frame_count > render_settings.iterations) {
        return;
    }

    float4 origin = mul(uniform.inv_view, float4(0.0, 0.0, 0.0, 1.0)); // Camera position
    float4 target = mul(uniform.inv_proj, float4(uv, 1.0, 1.0)); // View space target
    float4 direction = mul(uniform.inv_view, float4(normalize(target.xyz), 0.0)); // World space direction

    RayDesc ray = {};
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = T_MIN;
    ray.TMax = T_MAX;

    if (push_data.render_settings[0].debug_channel != DebugChannel::None) {
        float3 debug_color = get_debug_color(ray);
        out_image[int2(launch_id)] = float4(get_accumulated_color(debug_color), 1.0);
        return;
    }

    float3 radiance = float3(0.0);

    for (int sample = 0; sample < render_settings.samples; ++sample) {
        radiance += path_trace(ray, render_settings.max_depth);
    }
    radiance /= render_settings.samples;

    out_image[int2(launch_id)] = float4(get_accumulated_color(radiance), 1.0);
}